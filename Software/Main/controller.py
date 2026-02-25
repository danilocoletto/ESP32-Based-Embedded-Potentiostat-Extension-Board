import csv
import json
import time  
from PyQt6 import QtWidgets
from PyQt6.QtCore import QTimer
from PyQt6.QtWidgets import QFileDialog, QSpinBox, QDoubleSpinBox, QCheckBox, QLineEdit
from modules.processor import DataProcessor, PotentiostatGraph
from modules.serial_manager import SerialScanner, SerialWorker
from modules.protocol_defs import COMANDOS_SISTEMA, SCALES_CONFIG, EXPERIMENT_PAGES, PAQUETES_CONFIG, COMANDOS_UART

class PotentiostatController:
    def __init__(self, view):
        self.view = view
        self.worker = None
        self.comm_state = "IDLE" 
        self.retry_count = 0
        self.MAX_RETRIES = 2
        self.processor = DataProcessor(window_size=5)
        self.graph = PotentiostatGraph(self.view.graphicsView)
        self.current_experiment = "NO_EXPERIMENT"
        # Ganancia inicial por defecto (ejemplo 100 Ohms de la escala 40mA)
        self.current_gain = 100.0
        self.total_experiment_time = 0
        self.progress_timer = QTimer()
        self.progress_timer.timeout.connect(self.update_progress_bar)


    def iniciar_flujo_experimento(self):
        """Paso 1: Inicia el protocolo enviando READY_UP"""
        if not self.worker or not self.worker.isRunning():
            print("Error: Sin conexión.")
            return

        self.comm_state = "WAITING_READY_ACK"
        self.enviar_comando_uart("PREPARE") # Envía READY_UP\n

    def handle_system_command(self, clave, descripcion):
        """Maneja las respuestas ASCII del ESP32 según el estado de comunicación"""
        print(f"ESP32 -> {clave}: {descripcion}")

        if clave == "READY_ACK" and self.comm_state == "WAITING_READY_ACK":
            self.comm_state = "WAITING_PREPARING"
            # Esperamos ahora el mensaje "PREPARING" que envía la ESP al cambiar de estado

        elif clave == "PREPARING" and self.comm_state == "WAITING_PREPARING":
            self.comm_state = "WAITING_CONF_OK"
            self.enviar_configuracion()

        elif clave == "CONF_OK" and self.comm_state == "WAITING_CONF_OK":
            self.comm_state = "WAITING_EXECUTING"
            self.enviar_comando_uart("START") # Envía START_EXP\n

        elif clave == "CONF_ERR" and self.comm_state == "WAITING_CONF_OK":
            if self.retry_count < self.MAX_RETRIES:
                self.retry_count += 1
                print(f"Error de config. Reintento {self.retry_count}...")
                self.enviar_configuracion()
            else:
                print("Fallo crítico de configuración. Abortando.")
                self.reset_comm_flow()

        elif clave == "EXECUTING" and self.comm_state == "WAITING_EXECUTING":
            print("Experimento en marcha.")
            self.iniciar_logica_ui_experimento() # Inicia barra de progreso

        elif clave == "FINISHED":
            print("Experimento finalizado exitosamente.")
            self.progress_timer.stop()
            self.view.progressBar.setValue(100)

        elif clave == "WAITING":
            self.reset_comm_flow()

    def enviar_configuracion(self):
        paquete = self.preparar_paquete_configuracion()
        if paquete:
            self.worker.enviar_datos(paquete)


    def reset_comm_flow(self):
        self.comm_state = "IDLE"
        self.retry_count = 0
        self.progress_timer.stop()

    def iniciar_logica_ui_experimento(self):
        """Inicia cronómetros y UI una vez que la ESP confirma EXECUTING"""
        self.total_experiment_time = self.calcular_tiempo_total()

        if self.total_experiment_time <= 0:
            print("Advertencia: El tiempo estimado es 0. Ajustando a 1s para evitar error.")
            self.total_experiment_time = 1.0

        self.view.progressBar.setValue(0)
        if self.worker:
            self.worker.start_time = time.time() # Reset t0 para el gráfico

        self.progress_timer.start(100)
    
    def calcular_tiempo_total(self):
        """
        Calcula la duración estimada del experimento en segundos.
        Las fórmulas dependen de la física de cada técnica electroquímica.
        """
        if self.current_experiment == "NO_EXPERIMENT":
            return 0

        # Obtenemos los parámetros de la página actual
        params = self.obtener_parametros_activos()
        t_total = 0.0

        if self.current_experiment == "actionSWV":
            # 1. Tiempo de Quiet Time (Reposición)
            t_quiet = params.get("swv_quiet_t", 0.0)
            
            # 2. Si el preacondicionamiento está activo, sumamos ese tiempo
            t_precond = 0.0
            if self.view.precond_on_off.isChecked():
                t_precond = params.get("deposition_time", 0.0) + params.get("precond_quiet_t", 0.0)
            
            # 3. Tiempo de la rampa SWV
            # Cálculo: |V_final - V_inicial| / (Frecuencia * Step_E)
            v_init = params.get("swv_initial_pot", 0)
            v_final = params.get("swv_final_pot", 0)
            freq = params.get("swv_pulse_freq", 1) # Evitar división por cero
            step = params.get("swv_step_pot", 1)
            
            # El tiempo de barrido es el número de puntos dividido la frecuencia
            n_puntos = abs(v_final - v_init) / step if step != 0 else 0
            t_sweep = n_puntos / freq if freq != 0 else 0
            
            t_total = t_precond + t_quiet + t_sweep

        elif self.current_experiment == "actionLSV":
            t_quiet = params.get("lsv_quiet_time", 0.0)
            scan_rate = params.get("lsv_scan_rate", 1.0)
            segments = int(params.get("lsv_segments", 1))
            
            # Puntos de control en orden
            pts = [params.get("lsv_initial_pot", 0)]
            sw1 = params.get("lsv_switch_pot1", 0)
            sw2 = params.get("lsv_switch_pot2", 0)
            final = params.get("lsv_final_pot", 0)
            
            distancia_total = 0
            for i in range(1, segments + 1):
                # El último segmento va al final, los demás alternan sw1/sw2
                target = final if i == segments else (sw1 if i % 2 != 0 else sw2)
                distancia_total += abs(target - pts[-1])
                pts.append(target)
            
            t_sweep = distancia_total / scan_rate if scan_rate != 0 else 0
            t_total = t_quiet + t_sweep

        print(f"Tiempo estimado de ensayo: {t_total:.2f} segundos.")
        return t_total

    def update_progress_bar(self):
        """Calcula el porcentaje basado en el tiempo transcurrido del SerialWorker"""
        if self.worker and self.worker.running:
            # Obtenemos el tiempo que ya pasó desde el inicio del hilo
            elapsed = time.time() - self.worker.start_time
            
            # Calculamos porcentaje
            percentage = int((elapsed / self.total_experiment_time) * 100)
            
            if percentage >= 100:
                self.view.progressBar.setValue(100)
                self.progress_timer.stop()
            else:
                self.view.progressBar.setValue(percentage)
        else:
            self.progress_timer.stop()

    def seleccionar_experimento(self, action_name):
        """Cambia la página del StackedWidget y actualiza el tipo de experimento"""
        if action_name in EXPERIMENT_PAGES:
            config = EXPERIMENT_PAGES[action_name]
            self.current_experiment = action_name
            
            # Cambiar físicamente la página en la interfaz
            self.view.stackedWidget.setCurrentIndex(config["index"])
            print(f"Experimento seleccionado: {config['nombre']}")

    def auto_conectar(self):
        """Lógica de escaneo y conexión automática"""
        self.view.actualizar_ui_conexion("SEARCHING", "Status:   SCANNING...")
        
        id_potenciostato = COMANDOS_SISTEMA["ID"]
        puerto = SerialScanner.encontrar_potenciostato(baud=921600, id_esperado=id_potenciostato)
        
        if puerto:
            self.iniciar_comunicacion(puerto)
            self.view.actualizar_ui_conexion("CONNECTED", f"Status:   CONNECTED ({puerto})")
            return True
        else:
            self.view.actualizar_ui_conexion("DISCONNECTED", "Status:   NOT FOUND")
            return False

    def iniciar_comunicacion(self, puerto):
        """Gestiona el ciclo de vida del hilo SerialWorker"""
        if self.worker and self.worker.isRunning():
            self.worker.stop()
            self.worker.wait()

        self.worker = SerialWorker(puerto, 921600)
        self.worker.dato_procesado.connect(self.dispatch_data)

        self.worker.comando_texto.connect(self.handle_system_command) 
        self.worker.start()

    def dispatch_data(self, data):
        tipo = data['tipo']
        vals = data['valores']
        t = data['timestamp']
        
        v_applied = vals[0] 
        v_adc_diff = 0.0

        # Lógica diferenciada por técnica
        if tipo == "Square Wave Voltammetry":
            # SWV envía Forward (vals[1]) y Reverse (vals[2])
            v_adc_diff = vals[1] - vals[2]
            
        elif tipo == "Cyclic Voltammetry":
            # CV ya envía la corriente neta o el valor directo en vals[1]
            v_adc_diff = vals[1]

        # PROCESAMIENTO
        sample = self.processor.process_sample(t, v_applied, v_adc_diff, self.current_gain)
        
        # Actualizar Interfaz
        self.view.TimeDisplay.setText(f" Time:   {sample[0]:.2f} s")
        self.view.CurrentDisplay.setText(f"Current: {sample[2]:.6f} mA")
        
        # GRAFICACIÓN
        self.graph.update(self.processor.data_history)

    def cambiar_escala(self, action_name):
        if action_name in SCALES_CONFIG:
            info = SCALES_CONFIG[action_name]
            # 'multiplier' en tu protocol_defs.py representa la R de shunt (Ohms)
            self.current_gain = float(info['multiplier']) 
            
            self.view.CurrentScaleDisplay.setText(f"Current Scale Selected: {info['name']}")
            self.view.AmpGainDisplay.setText(f"Amplifier Gain: {info['gain_label']}")

    def obtener_parametros_activos(self):
        """Devuelve un diccionario con los valores de los SpinBoxes de la página actual"""
        if self.current_experiment == "NO_EXPERIMENT":
            return None
            
        params = {}
        idx = EXPERIMENT_PAGES[self.current_experiment]["index"]
        page_widget = self.view.stackedWidget.widget(idx)
        
        for spinbox in page_widget.findChildren((QSpinBox, QDoubleSpinBox)):
            params[spinbox.objectName()] = spinbox.value()
            
        return params


    def detener_comunicacion(self):
        if self.worker:
            self.worker.stop()
        
        self.view.actualizar_ui_conexion("DISCONNECTED", "Status:   DISCONNECTED")
        return False

    
    def guardar_datos_csv(self):
        """Genera un archivo CSV. Permite guardar aunque el historial esté vacío."""
        # Abrir cuadro de diálogo para elegir ubicación
        file_path, _ = QFileDialog.getSaveFileName(
            self.view, "Save Data", "", "CSV Files (*.csv);;All Files (*)"
        )

        if file_path:
            try:
                with open(file_path, mode='w', newline='') as f:
                    writer = csv.writer(f)
                    # Escribir encabezados requeridos: timestamp, tensión, corriente, filtrada 
                    writer.writerow(["time_stamp", "v_applied_mv", "current_ma", "current_filtered_ma"])
                    
                    # Escribir datos si existen 
                    if self.processor.data_history:
                        writer.writerows(self.processor.data_history)
                
                print("CSV guardado exitosamente (podría estar vacío).")
            except Exception as e:
                print(f"Error saving CSV: {e}")

    def guardar_grafico_imagen(self):
        """Guarda la vista actual del gráfico. Permite guardar el canvas vacío."""
        file_path, _ = QFileDialog.getSaveFileName(
            self.view, "Save Graphic", "", "PNG Image (*.png);;JPEG Image (*.jpg)"
        )

        if file_path:
            try:
                # El exportador de pyqtgraph guardará el estado actual del widget 
                self.graph.export_image(file_path)
                print("Imagen del gráfico guardada exitosamente.")
            except Exception as e:
                print(f"Error saving Image: {e}")


    def guardar_configuracion_ui(self):
        """
        Guarda la configuración solo del experimento activo y filtra parámetros 
        deshabilitados (como los de acondicionamiento).
        """
        if self.current_experiment == "NO_EXPERIMENT":
            print("No hay un experimento seleccionado para guardar.")
            return

        exp_info = EXPERIMENT_PAGES[self.current_experiment]
        config_to_save = {
            "experiment_id": self.current_experiment,
            "experiment_name": exp_info["nombre"],
            "page_index": exp_info["index"],
            "parameters": {}
        }

        page_widget = self.view.stackedWidget.widget(exp_info["index"])
        
        from PyQt6.QtWidgets import QSpinBox, QDoubleSpinBox, QCheckBox, QLineEdit
        
        # Obtenemos todos los widgets de la página
        for widget in page_widget.findChildren((QSpinBox, QDoubleSpinBox, QCheckBox, QLineEdit)):
            name = widget.objectName()
            
            # 1. FILTRO DE WIDGETS INTERNOS: 
            # Evitamos guardar el 'qt_spinbox_lineedit' que es un hijo interno de los SpinBoxes
            if "qt_spinbox_lineedit" in name:
                continue

            # 2. FILTRO DE SOLO LECTURA:
            if hasattr(widget, 'isReadOnly') and widget.isReadOnly():
                continue

            # 3. FILTRO DE PARÁMETROS HABILITADOS (Tu requerimiento principal):
            # Si el widget está deshabilitado en la UI (porque el checkbox no está marcado), no se guarda.
            if not widget.isEnabled():
                continue
                
            # 4. EXTRACCIÓN DE DATOS:
            if isinstance(widget, (QSpinBox, QDoubleSpinBox)):
                config_to_save["parameters"][name] = widget.value()
            elif isinstance(widget, QCheckBox):
                config_to_save["parameters"][name] = widget.isChecked()
            elif isinstance(widget, QLineEdit):
                config_to_save["parameters"][name] = widget.text()

        # Guardado de archivo
        from PyQt6.QtWidgets import QFileDialog
        import json

        file_path, _ = QFileDialog.getSaveFileName(
            self.view, f"Save Configuration - {exp_info['nombre']}", "", "Config Files (*.json)"
        )

        if file_path:
            try:
                with open(file_path, 'w') as f:
                    json.dump(config_to_save, f, indent=4)
                print(f"Configuración guardada (parámetros activos solamente).")
            except Exception as e:
                print(f"Error al guardar: {e}")

    def cargar_configuracion_ui(self):
        """Lee un archivo .json y restaura los valores y el tipo de experimento"""
        file_path, _ = QFileDialog.getOpenFileName(
            self.view, "Cargar Configuración", "", "Config Files (*.json)"
        )

        if not file_path:
            return

        try:
            with open(file_path, 'r') as f:
                config = json.load(f)

            # 1. Recuperar el ID del experimento (ej: "actionSWV")
            # Usamos get con un fallback por si el JSON es de una versión vieja
            exp_id = config.get("experiment_id")
            
            if exp_id and exp_id in EXPERIMENT_PAGES:
                # Usamos nuestra función existente para cambiar la pestaña y el ID interno
                self.seleccionar_experimento(exp_id)
            elif "page_index" in config:
                # Fallback: solo cambiar la página si no hay ID
                self.view.stackedWidget.setCurrentIndex(config["page_index"])

            # 2. Restaurar parámetros de los widgets
            params = config.get("parameters", {})
            for name, value in params.items():
                # Corregido: usamos la instancia del widget directamente o QtWidgets.QWidget
                widget = self.view.findChild(QtWidgets.QWidget, name)
                
                if widget:
                    if isinstance(widget, (QSpinBox, QDoubleSpinBox)):
                        widget.setValue(value)
                    elif isinstance(widget, QCheckBox):
                        widget.setChecked(value)
                    elif isinstance(widget, QLineEdit):
                        widget.setText(str(value))
            
            print(f"Configuración de {exp_id} cargada exitosamente.")
            
        except Exception as e:
            # Ahora el error mostrará detalles más precisos si falta algo
            print(f"Error al cargar configuración: {e}")


    def preparar_paquete_configuracion(self):
        """
        Construye el string de configuración basado en el experimento actual
        y los parámetros habilitados en la interfaz, convirtiendo booleanos a 1/0.
        """
        if self.current_experiment not in PAQUETES_CONFIG:
            return None

        config_def = PAQUETES_CONFIG[self.current_experiment]
        header = config_def["header"]
        
        valores = []
        idx = EXPERIMENT_PAGES[self.current_experiment]["index"]
        page_widget = self.view.stackedWidget.widget(idx)

        for param_name in config_def["params"]:
            # Buscamos el widget por el nombre definido en protocol_defs.py
            widget = page_widget.findChild(QtWidgets.QWidget, param_name)
            
            if widget:
                # 1. Si el widget está deshabilitado por lógica de UI, enviamos "0"
                if not widget.isEnabled():
                    valores.append("0")
                
                # 2. Si es un CheckBox, enviamos "1" si está tildado, "0" si no
                elif isinstance(widget, QCheckBox):
                    val = "1" if widget.isChecked() else "0"
                    valores.append(val)
                
                # 3. Si es un SpinBox (Double o normal), extraemos su valor numérico
                elif isinstance(widget, (QSpinBox, QDoubleSpinBox)):
                    valores.append(str(widget.value()))
                
                # 4. Fallback para LineEdits u otros
                elif hasattr(widget, 'text'):
                    valores.append(widget.text())
                else:
                    valores.append("0")
            else:
                # Si el widget no existe en la página, enviamos un valor neutro
                valores.append("0")

        # Construcción del paquete final: "HEADER:val1,val2,val3...\n"
        paquete_final = f"{header}:{','.join(valores)}\n"

        # Depuración para verificar lo que sale hacia el ESP32
        print(f"\n[DEBUG TX] Paquete de Configuración: {paquete_final.strip()}")
        
        return paquete_final

    def _obtener_valor_widget(self, widget):
        """Función auxiliar para normalizar valores de widgets a string"""
        if not widget: return "0"
        
        if hasattr(widget, 'value'): # SpinBoxes
            return str(widget.value())
        elif hasattr(widget, 'isChecked'): # CheckBoxes
            return "1" if widget.isChecked() else "0"
        elif hasattr(widget, 'text'): # LineEdits
            return widget.text()
        return "0"
    
    def enviar_comando_uart(self, clave_comando):
        """
        Envía un comando predefinido al ESP32.
        Uso: self.enviar_comando_uart("GET_BATTERY")
        """
        # Importamos las definiciones para tener acceso a los strings
        from modules.protocol_defs import COMANDOS_UART
        
        if clave_comando in COMANDOS_UART:
            comando_str = COMANDOS_UART[clave_comando]
            
            if self.worker and self.worker.isRunning():
                # Enviamos el comando a través del worker
                self.worker.enviar_datos(comando_str)
                print(f"UART Tx -> {comando_str.strip()}")
            else:
                print("Error: No hay una conexión serie activa.")
        else:
            print(f"Error: El comando '{clave_comando}' no existe en el protocolo.")

    def limpiar_experimento_completo(self):
        """Reinicia el procesador, el gráfico y la barra de progreso."""
        self.processor.clear_data()
        self.graph.clear_graph()
        self.view.progressBar.setValue(0)
        print("Gráfico e historial de datos reiniciados.")