import csv
import json
import time
import logging
from PyQt6 import QtWidgets
from PyQt6.QtCore import QTimer
from PyQt6.QtWidgets import QMessageBox, QFileDialog, QSpinBox, QDoubleSpinBox, QCheckBox, QLineEdit
from modules.processor import DataProcessor, PotentiostatGraph
from modules.serial_manager import SerialScanner, SerialWorker, ScannerWorker
from modules.protocol_defs import COMANDOS_SISTEMA, SCALES_CONFIG, EXPERIMENT_PAGES, PAQUETES_CONFIG, COMANDOS_UART

class PotentiostatController:
    def __init__(self, view):
        self.view = view
        self.worker = None
        self._scanner = None            # Referencia al hilo de escaneo activo
        self.comm_state = "IDLE" 
        self.retry_count = 0
        self.MAX_RETRIES = 2

        # Experimento seleccionado actualmente
        self.current_experiment = "NO_EXPERIMENT"
        # Ganancia inicial por defecto (ejemplo 100 Ohms de la escala 40mA)
        self.current_gain = 100.0
        self.total_experiment_time = 0

        # Variable para almacenar el último valor de corriente procesado para displays númericos
        self.last_current_ma = 0.0
        self.last_time_s = 0.0

        # Timer para actualizar los displays numéricos a velocidad humana (4Hz)
        self.display_update_timer = QTimer()
        self.display_update_timer.timeout.connect(self.refresh_numeric_displays)
        self.display_update_timer.start(250) # 250ms es ideal para lectura visual


        # Timer para contabilizar el tiempo de la barra de progreso y el experimento
        self.progress_timer = QTimer()
        self.progress_timer.timeout.connect(self.update_progress_bar)
        self.establecer_escala_inicial()

        # Timer para tasa de refresco del grafico (10Hz)
        self.graph_update_timer = QTimer()
        self.graph_update_timer.timeout.connect(self._refresh_graph)
        self.graph_update_timer.start(100)  # 10 Hz es suficiente para el ojo humano

        self.processor = DataProcessor(median_size=5, window_size=61)
        self.graph = PotentiostatGraph(self.view.graphicsView)


    def establecer_escala_inicial(self):
        """Configura la UI con los valores de la escala de 100mA por defecto."""
        # Buscamos la configuración de 100mA en SCALES_CONFIG
        info_defecto = SCALES_CONFIG.get("action40_mA") 
        if info_defecto:
            self.current_gain = float(info_defecto['multiplier']) 
            self.view.CurrentScaleDisplay.setText(f"Scale Selected:   {info_defecto['name']}")
            self.view.AmpGainDisplay.setText(f"Amplifier Gain: {info_defecto['gain_label']}")


    def iniciar_flujo_experimento(self):
        """Paso 1: Inicia el protocolo enviando READY_UP"""
        if not self.worker or not self.worker.isRunning():
            logging.error("Error while starting: No connection.")
            return

        self.comm_state = "WAITING_READY_ACK"
        self.enviar_comando_uart("PREPARE") # Envía READY_UP\n

    def handle_system_command(self, clave, descripcion):
        """
        Maneja las respuestas ASCII del ESP32 gestionando la secuencia de:
        Handshake -> Configuración de Ganancia -> Configuración de Experimento -> Ejecución.
        """
        logging.debug(f"ESP32 -> Command Sent: {clave} | Meaning - {descripcion}") 

        # --- GESTIÓN DE ESTADO DE ELECTRODOS (Independiente del flujo de experimento) ---
        if clave == "CELL1":
            self.view.actualizar_ui_electrodos("ENGAGED") 
        elif clave == "CELL0":
            self.view.actualizar_ui_electrodos("DISENGAGED") 

        # --- FLUJO DE CONTROL DE EXPERIMENTO ---
        
        # Paso 1: Recepción de READY_ACK tras enviar PREPARE (READY_UP)
        if clave == "READY_ACK" and self.comm_state == "WAITING_READY_ACK":
            self.comm_state = "WAITING_PREPARING" 

        # Paso 2: ESP confirma que está lista para recibir configuración
        elif clave == "PREPARING" and self.comm_state == "WAITING_PREPARING":
            self.comm_state = "WAITING_GAIN_OK" 
            self.enviar_configuracion_ganancia() 

        # Paso 3: Validación de Ganancia (Ahora con comando específico CONF_GAIN_OK)
        elif clave == "CONF_GAIN_OK" and self.comm_state == "WAITING_GAIN_OK":
            # La ganancia se configuró en hardware, ahora esperamos el reporte GAINX
            logging.info("Gain Configured. Waiting for sync with hardware...")
            pass 

        elif clave.startswith("GAIN") and self.comm_state == "WAITING_GAIN_OK":
            try:
                # Extraemos el ID (ej: "GAIN2" -> 2)
                id_recibido = int(clave.replace("GAIN", "")) 
                self.sincronizar_escala_por_id(id_recibido) 
                
                # Una vez sincronizada la ganancia en la UI, pasamos a configurar la técnica
                logging.debug(f"GAIN ID Received: {id_recibido}")
                logging.info(f"UI Synchronized with Hardware. Sending configuration for experiment...") 
                self.comm_state = "WAITING_CONF_OK" # Cambiamos de estado antes de enviar
                self.enviar_configuracion() 
            except ValueError:
                logging.error(f"Error processing gain ID: {clave}")
                self.reset_comm_flow()
                self.comm_state = "IDLE"

        # Paso 4: Confirmación de parámetros de la técnica (CONF_OK se queda para esto)
        elif clave == "CONF_OK" and self.comm_state == "WAITING_CONF_OK":
            logging.info(f"Configuration OK. Experiment starting...")
            self.comm_state = "WAITING_EXECUTING" 
            self.enviar_comando_uart("START")

        elif clave == "CONF_ERR":
            # Manejo de error tanto en ganancia como en técnica
            if self.retry_count < self.MAX_RETRIES:
                self.retry_count += 1
                logging.error(f"Configuration Error. Retry {self.retry_count}...") 
                # Dependiendo de dónde falló, reintentamos uno u otro
                if self.comm_state == "WAITING_GAIN_OK":
                    self.enviar_configuracion_ganancia()
                else:
                    self.enviar_configuracion()
            else:
                logging.error("Critical failure in configuration process. Aborting experiment.")
                self.reset_comm_flow()
                self.comm_state = "IDLE" 

        # Paso 5: Ejecución y Finalización
        elif clave == "EXECUTING" and self.comm_state == "WAITING_EXECUTING":
            self.comm_state = "RUNNING"
            self.iniciar_logica_ui_experimento()

        elif clave == "FINISHED"  and self.comm_state == "RUNNING":
            self.comm_state = "FINISHING"           # ← esperamos el WAITING de la ESP
            self.progress_timer.stop() 
            self.view.progressBar.setValue(100) 
            self.graph_update_timer.stop()          # ← Congela el gráfico al terminar
            self.display_update_timer.stop()

            if self.worker:
                self.worker.cerrar_archivo_respaldo()  # ← flush y cierre al terminar
            
            logging.info(f"Experiment Finished.")

        elif clave == "WAITING" and self.comm_state == "FINISHING":
            self.reset_comm_flow()
            self.comm_state = "IDLE"
            logging.info("Device back to idle. Ready for next experiment.")


    def enviar_configuracion_ganancia(self):
        """Busca el ID de la escala actual y lo envía a la ESP32."""
        # Iniciamos con el ID de la escala por defecto (action40_mA -> ID 1) 
        gain_id = 1 
        
        for config in SCALES_CONFIG.values():
            # Usamos round para evitar problemas de precisión con flotantes
            if round(float(config['multiplier']), 2) == round(self.current_gain, 2):
                gain_id = config['id_hardware']
                break
        
        logging.debug(f"Sending Gain ID: {gain_id}")
        self.worker.enviar_datos(f"SET_GAIN:{gain_id}\n")

    def sincronizar_escala_por_id(self, id_hw):
        """Actualiza la UI basándose en el ID de hardware confirmado por la ESP32."""
        for info in SCALES_CONFIG.values():
            if info['id_hardware'] == id_hw:
                self.current_gain = float(info['multiplier']) 
                self.view.CurrentScaleDisplay.setText(f"Scale Selected:   {info['name']}") 
                self.view.AmpGainDisplay.setText(f"Amplifier Gain: {info['gain_label']}") 
                break

    def enviar_configuracion(self):
        paquete = self.preparar_paquete_configuracion()
        if paquete:
            self.worker.enviar_datos(paquete)


    def reset_comm_flow(self):
        self.retry_count = 0
        self.progress_timer.stop()
        self.graph_update_timer.stop()   
        self.display_update_timer.stop()
        self.view.init_button.setEnabled(True)

    def iniciar_logica_ui_experimento(self):
        """Inicia cronómetros y UI una vez que la ESP confirma EXECUTING"""
        self.total_experiment_time = self.calcular_tiempo_total()

        if self.total_experiment_time <= 0:
            logging.warning("Warning: The estimated time is 0. Adjusting to 1 seg to avoid error.")
            self.total_experiment_time = 1.0

        self.view.progressBar.setValue(0)

        if self.worker:
            self.worker.resetear_archivo_respaldo()
            self.worker.start_time = time.time() # Reset t0 para el gráfico

        self.progress_timer.start(100)
        self.graph_update_timer.start(100)  # ← Reiniciar acá
        self.display_update_timer.start(250)
    
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

        elif self.current_experiment == "actionLSV/CV":
            # 1. Obtención de parámetros con conversión explícita a float
            t_quiet = float(params.get("lsv_quiet_time", 0.0))
            scan_rate = float(params.get("lsv_scan_rate", 1.0))
            segments = int(params.get("lsv_segments", 1))
            
            v_init = float(params.get("lsv_initial_pot", 0))
            sw1 = float(params.get("lsv_switch_pot1", 0))
            sw2 = float(params.get("lsv_switch_pot2", 0))
            final = float(params.get("lsv_final_pot", 0))
            
            distancia_total = 0
            posicion_actual = v_init
            
            # 2. Replicamos la lógica de targets del firmware (C)
            for i in range(1, segments + 1):
                if i == segments:
                    # El último segmento siempre termina en el potencial FINAL
                    target = final
                elif i == 1:
                    # El primer segmento siempre va al Switch 1
                    target = sw1
                else:
                    # Los intermedios oscilan entre Switch 2 (pares) y Switch 1 (impares)
                    target = sw2 if i % 2 == 0 else sw1
                
                # Sumamos la distancia absoluta de este tramo
                distancia_total += abs(target - posicion_actual)
                
                # El final de este segmento es el inicio del siguiente
                posicion_actual = target 
            
            # 3. Cálculo del tiempo de barrido (t = d / v)
            t_sweep = distancia_total / scan_rate if scan_rate != 0 else 0
            t_total = t_quiet + t_sweep

        logging.debug(f"Estimated experiment time: {t_total:.2f} seconds.")
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
        """Cambia la técnica solo si no hay un experimento en curso."""
        # BLOQUEO DE SEGURIDAD
        if self.comm_state != "IDLE":
            QMessageBox.warning(self.view, "Action Denied", 
                "An experiment is currently running. Stop it before switching techniques.")
            return

        if action_name in EXPERIMENT_PAGES:
            config = EXPERIMENT_PAGES[action_name]
            self.current_experiment = action_name
            
            # Cambiar físicamente la página en la interfaz
            self.graph.set_experiment_mode(action_name)
            self.view.stackedWidget.setCurrentIndex(config["index"])
            logging.debug(f"Selected Experiment: {config['nombre']}")

    
    def reset_instrumento(self):
        """Envía RESET y comienza bucle de autoconexión."""
        logging.debug("Sending RESET command...")
        self.enviar_comando_uart("RESET")
        
        # Resetear estados internos locales
        self.reset_comm_flow()
        self.view.actualizar_ui_conexion("DISCONNECTED", "Status: Resetting Device")
        if self.worker:
            self.worker.stop()
        
        # Esperar un momento a que el ESP32 inicie el reboot físico antes de buscarlo
        QTimer.singleShot(4000, self.intentar_autoconexion_bucle)

    def intentar_autoconexion_bucle(self):
        """Intenta conectar recursivamente hasta tener éxito."""

        if self.worker and self.worker.isRunning():
            return  # Ya se reconectó, cancelar este bucle
        
        self.view.actualizar_ui_conexion("SEARCHING", "Status: RECONNECTING")
        
        self._scanner = ScannerWorker(baud=921600)
        self._scanner.dispositivo_encontrado.connect(self._on_dispositivo_encontrado)
        self._scanner.escaneo_fallido.connect(
            lambda: QTimer.singleShot(2000, self.intentar_autoconexion_bucle)  # reintenta en 2s
        )
        self._scanner.start()

    def auto_conectar(self):
        """Lógica de escaneo y conexión automática"""
        self.view.actualizar_ui_conexion("SEARCHING", "Status:   SCANNING...")

        self._scanner = ScannerWorker(baud=921600)
        self._scanner.dispositivo_encontrado.connect(self._on_dispositivo_encontrado)
        self._scanner.escaneo_fallido.connect(self._on_escaneo_fallido)
        self._scanner.start()
            
    def _on_dispositivo_encontrado(self, puerto):
        self.iniciar_comunicacion(puerto)
        self.view.actualizar_ui_conexion("CONNECTED", f"Status:   CONNECTED ({puerto})")

    def _on_escaneo_fallido(self):
        self.view.actualizar_ui_conexion("DISCONNECTED", "Status:   NOT FOUND")

    def iniciar_comunicacion(self, puerto):
        """Gestiona el ciclo de vida del hilo SerialWorker"""
        if self.worker and self.worker.isRunning():
            self.worker.stop()
            self.worker.wait()
            self.worker.dato_procesado.disconnect()
            self.worker.comando_texto.disconnect()

        self.worker = SerialWorker(puerto, 921600)
        self.worker.dato_procesado.connect(self.dispatch_data)
        self.worker.comando_texto.connect(self.handle_system_command) 

        if self.worker:
            self.worker.start()
            QTimer.singleShot(1000, lambda: self.enviar_comando_uart("ELEC_STATUS"))

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
            
        elif tipo == "Linear/Cyclic Voltammetry":
            # CV ya envía la corriente neta o el valor directo en vals[1]
            v_adc_diff = vals[1]

        elif tipo == "Controlled Potential Electrolysis":
            v_adc_diff = vals[2]  # vals[0]=index, vals[1]=voltage, vals[2]=current

        # PROCESAMIENTO
        sample = self.processor.process_sample(t, v_applied, v_adc_diff, self.current_gain)

        # Estas variables se guardan para el refresco de los displays númericos
        self.last_time_s = sample[0]
        self.last_current_ma = sample[2]
    
    def _refresh_graph(self):
        # Solo renderiza si hay datos nuevos
        if self.processor.data_history:
            self.graph.update(self.processor.data_history, self.current_experiment)

    def refresh_numeric_displays(self):
        """Actualiza los QLineEdit de tiempo y corriente a una velocidad legible."""
        # 1. Actualizar Tiempo
        self.view.TimeDisplay.setText(f" Time:   {self.last_time_s:.2f} s")

        # 2. Lógica de Auto-escala para Corriente
        abs_current = abs(self.last_current_ma)
        
        if abs_current < 0.001:  # nA
            display_val = self.last_current_ma * 1e6
            unit = "nA"
        elif abs_current < 1.0:   # uA
            display_val = self.last_current_ma * 1e3
            unit = "µA"
        else:                     # mA
            display_val = self.last_current_ma
            unit = "mA"

        # 3. Actualizar Display de Corriente
        self.view.CurrentDisplay.setText(f"Current:   {display_val:.3f} {unit}")


    def cambiar_escala(self, action_name):
        """
        Cambia la ganancia del hardware solo si no hay un experimento en curso.
        """
        # --- BLOQUEO DE SEGURIDAD ---
        if self.comm_state != "IDLE":
            QMessageBox.warning(
                self.view, 
                "Operation Denied", 
                "Cannot change the current scale while an experiment is running.\n"
                "Please, stop the experiment first."
            )
            return

        # Si está en IDLE, procedemos con el cambio
        if action_name in SCALES_CONFIG:
            info = SCALES_CONFIG[action_name]
            # 'multiplier' en tu protocol_defs.py representa la R de shunt (Ohms)
            self.current_gain = float(info['multiplier']) 
            
            self.view.CurrentScaleDisplay.setText(f"Scale Selected: {info['name']}")
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
                
                logging.debug("CSV succesfully saved.")
            except Exception as e:
                logging.error(f"Error saving CSV: {e}")

    def guardar_grafico_imagen(self):
        """Guarda la vista actual del gráfico. Permite guardar el canvas vacío."""
        file_path, _ = QFileDialog.getSaveFileName(
            self.view, "Save Graphic", "", "PNG Image (*.png);;JPEG Image (*.jpg)"
        )

        if file_path:
            try:
                # El exportador de pyqtgraph guardará el estado actual del widget 
                self.graph.export_image(file_path)
                logging.debug("Graphic image succesfully saved.")
            except Exception as e:
                logging.error(f"Error saving Image: {e}")

    def guardar_configuracion_ui(self):
        """
        Guarda la configuración del experimento activo, incluyendo la escala actual,
        y filtra parámetros deshabilitados.
        """
        if self.current_experiment == "NO_EXPERIMENT":
            QtWidgets.QMessageBox.warning(self.view, "Guardar", "No hay un experimento seleccionado.")
            return

        exp_info = EXPERIMENT_PAGES[self.current_experiment]
        
        # --- NUEVO: Buscar el ID de la escala actual para guardarlo ---
        current_scale_id = 1  # Valor por defecto (40mA)
        for scale_info in SCALES_CONFIG.values():
            if round(float(scale_info['multiplier']), 2) == round(self.current_gain, 2):
                current_scale_id = scale_info['id_hardware']
                break

        config_to_save = {
            "experiment_id": self.current_experiment,
            "experiment_name": exp_info["nombre"],
            "page_index": exp_info["index"],
            "selected_scale_id": current_scale_id, # Guardamos la escala
            "parameters": {}
        }

        page_widget = self.view.stackedWidget.widget(exp_info["index"])

        for widget in page_widget.findChildren((QSpinBox, QDoubleSpinBox, QCheckBox, QLineEdit)):
            name = widget.objectName()
            if "qt_spinbox_lineedit" in name: continue
            if hasattr(widget, 'isReadOnly') and widget.isReadOnly(): continue
            if not widget.isEnabled(): continue
                
            if isinstance(widget, (QSpinBox, QDoubleSpinBox)):
                config_to_save["parameters"][name] = widget.value()
            elif isinstance(widget, QCheckBox):
                config_to_save["parameters"][name] = widget.isChecked()
            elif isinstance(widget, QLineEdit):
                config_to_save["parameters"][name] = widget.text()

        file_path, _ = QtWidgets.QFileDialog.getSaveFileName(
            self.view, f"Save Configuration - {exp_info['nombre']}", "", "Config Files (*.json)"
        )

        if file_path:
            try:
                with open(file_path, 'w') as f:
                    json.dump(config_to_save, f, indent=4)
                QtWidgets.QMessageBox.information(self.view, "Success", "Configuration Saved Succesfully.")
                logging.debug("Success", "Configuration Succesfully Saved.")
            except Exception as e:
                QtWidgets.QMessageBox.critical(self.view, "Error", f"Unable to save configuration: {e}")
                logging.error(f"Save Error. Unable to save configuration: {e}")


    def cargar_configuracion_ui(self):
        """Lee un archivo .json, restaura los valores, la técnica y la escala."""
        file_path, _ = QtWidgets.QFileDialog.getOpenFileName(
            self.view, "Load Configuration", "", "Config Files (*.json)"
        )

        if not file_path: 
            return

        try:
            with open(file_path, 'r') as f:
                config = json.load(f)

            # ── VALIDACIÓN DE FORMATO ──────────────────────────────────────
            valido, motivo = self._validar_configuracion(config)
            if not valido:
                QtWidgets.QMessageBox.warning(
                    self.view, "Invalid Configuration File",
                    f"The file could not be loaded because its format is invalid.\n\nReason: {motivo}"
                )
                logging.warning(f"Configuration file rejected: {motivo} | File: {file_path}")
                return
            # ──────────────────────────────────────────────────────────────

            exp_id = config.get("experiment_id")
            exp_name = config.get("experiment_name")
            if exp_id and exp_id in EXPERIMENT_PAGES:
                self.seleccionar_experimento(exp_id)
            elif "page_index" in config:
                self.view.stackedWidget.setCurrentIndex(config["page_index"])

            if "selected_scale_id" in config:
                self.sincronizar_escala_por_id(config["selected_scale_id"])

            params = config.get("parameters", {})
            for name, value in params.items():
                widget = self.view.findChild(QtWidgets.QWidget, name)
                if widget:
                    if isinstance(widget, (QSpinBox, QDoubleSpinBox)):
                        widget.setValue(value)
                    elif isinstance(widget, QCheckBox):
                        widget.setChecked(value)
                    elif isinstance(widget, QLineEdit):
                        widget.setText(str(value))

            QtWidgets.QMessageBox.information(self.view, "Load", f"Configuration for '{exp_name}' loaded with success.")
            logging.debug(f"Configuration for '{exp_name}' loaded successfully")

        except json.JSONDecodeError as e:
            # Archivo que no es JSON válido en absoluto
            QtWidgets.QMessageBox.critical(
                self.view, "Load Error",
                f"The file is not a valid JSON file.\n\nDetail: {e}"
            )
            logging.error(f"JSON parse error loading config: {e}")
        except Exception as e:
            QtWidgets.QMessageBox.critical(self.view, "Load Error", f"Detail: {e}")
            logging.error(f"Load Error. Detail: {e}")


    def _validar_configuracion(self, config):
        """
        Verifica que el archivo JSON tenga el formato esperado antes de cargarlo.
        Retorna (True, None) si es válido, o (False, motivo) si no lo es.
        """
        # Campos obligatorios
        campos_requeridos = ["experiment_id", "experiment_name", "page_index", "parameters"]
        for campo in campos_requeridos:
            if campo not in config:
                return False, f"Missing required field: '{campo}'"

        # El experimento debe ser uno de los conocidos
        if config["experiment_id"] not in EXPERIMENT_PAGES:
            return False, f"Unknown experiment ID: '{config['experiment_id']}'"

        # page_index debe ser un entero válido
        if not isinstance(config["page_index"], int):
            return False, f"'page_index' must be an integer, got: {type(config['page_index']).__name__}"

        # parameters debe ser un diccionario
        if not isinstance(config["parameters"], dict):
            return False, f"'parameters' must be a dictionary, got: {type(config['parameters']).__name__}"

        # selected_scale_id si existe debe ser un entero conocido
        if "selected_scale_id" in config:
            scale_id = config["selected_scale_id"]
            ids_validos = [s["id_hardware"] for s in SCALES_CONFIG.values()]
            if not isinstance(scale_id, int) or scale_id not in ids_validos:
                return False, f"Invalid scale ID: '{scale_id}'"

        return True, None


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
        logging.debug(f"\n[DEBUG TX] Paquete de Configuración: {paquete_final.strip()}")
        
        return paquete_final
    
    
    def enviar_comando_uart(self, clave_comando):
        """
        Envía un comando predefinido al ESP32.
        Uso: self.enviar_comando_uart("GET_STATE")
        """
        
        if clave_comando in COMANDOS_UART:
            comando_str = COMANDOS_UART[clave_comando]
            
            if self.worker and self.worker.isRunning():
                # Enviamos el comando a través del worker
                self.worker.enviar_datos(comando_str)
                logging.debug(f"Python Command Sent -> {comando_str.strip()}")
            else:
                logging.error("Error: There is no active serial connection at the momment.")
        else:
            logging.error(f"Error: The command '{clave_comando}' doesn't exist.")

    def limpiar_experimento_completo(self):
        """Reinicia el procesador, el gráfico y la barra de progreso."""
        self.processor.clear_data()
        self.graph.clear_graph()
        self.view.progressBar.setValue(0)
        logging.debug("Graphic and Data Historial Reset.")

    
    def tiene_datos_pendientes(self):
        """Verifica si hay datos en el historial o parámetros modificados."""
        # Comprobar si hay datos capturados en el procesador
        hay_datos = len(self.processor.data_history) > 0
        
        # Comprobar si el experimento actual es distinto al estado inicial
        hay_config = self.current_experiment != "NO_EXPERIMENT"
        
        return hay_datos, hay_config

    def cerrar_recursos(self):
        """Detiene hilos y cierra puertos antes de salir."""

        self.display_update_timer.stop()
        self.graph_update_timer.stop()
        self.progress_timer.stop()

        if self.worker:
            self.worker.stop()
            self.worker.wait()