import serial.tools.list_ports
import serial
import struct
import time
import os
from PyQt6.QtCore import QThread, pyqtSignal
from modules.protocol_defs import EXPERIMENT_CONFIG, COMANDOS_SISTEMA


class SerialScanner:
    @staticmethod
    def encontrar_potenciostato(baud=921600, id_esperado = COMANDOS_SISTEMA["ID"]):
        # 1. Obtener lista de todos los puertos COM físicos/virtuales
        puertos = serial.tools.list_ports.comports()
        
        for p in puertos:
            print(f"Probando puerto: {p.device}...")
            try:
                # Intentamos abrir el puerto con un timeout muy corto para no colgar la app
                ser = serial.Serial(p.device, baud, timeout=1)
                
                # El ESP32 suele resetearse al abrir puerto, esperamos un momento
                time.sleep(1.5) 
                
                # 2. Enviamos el comando de identificación
                ser.write(b"?ID\n") 
                
                # 3. Leemos la respuesta
                respuesta = ser.readline().decode('utf-8').strip()
                
                if respuesta == id_esperado:
                    print(f"¡Dispositivo encontrado en {p.device}!")
                    ser.close()
                    return p.device # Devolvemos el nombre del puerto (ej. "COM3")
                
                ser.close()
            except Exception:
                continue # Si el puerto está ocupado o falla, pasamos al siguiente
                
        return None # No se encontró nada
    

class SerialWorker(QThread):
    # Señales para comunicación con el Controller
    dato_procesado = pyqtSignal(dict) 
    comando_texto = pyqtSignal(str, str)

    def __init__(self, port, baud):
        super().__init__()
        self.port = port
        self.baud = baud
        self.running = False
        self.start_time = 0
        self.ser_inst = None  # Instancia persistente para Tx/Rx
        
        # Archivo de respaldo interno
        self.backup_path = "internal_backup.log"

    def enviar_datos(self, datos_str):
        """
        Envía un string codificado al ESP32 por UART.
        Se usa para comandos individuales y paquetes de configuración.
        """
        if self.ser_inst and self.ser_inst.is_open:
            try:
                self.ser_inst.write(datos_str.encode('ascii'))
                # Opcional: asegurar que se envíe inmediatamente
                self.ser_inst.flush() 
            except Exception as e:
                print(f"Error al enviar datos por UART: {e}")

    def stop(self):
        """Detiene el bucle del hilo de forma segura"""
        self.running = False

    def run(self):
        """
        Bucle principal del hilo que gestiona la lectura de datos binarios de experimentos
        y mensajes de control ASCII (handshake) desde la ESP32.
        """
        try:
            # Inicializamos la conexión serie
            self.ser_inst = serial.Serial(self.port, self.baud, timeout=0.1)
            self.running = True
            self.start_time = time.time()  # T0 para el timestamp de las muestras

            while self.running:
                if self.ser_inst.in_waiting > 0:
                    # Leemos el primer byte para determinar si es el inicio de un paquete binario
                    byte1 = self.ser_inst.read(1)

                    # 1. DETECCIÓN DE ENCABEZADO BINARIO (EXPERIMENTOS)
                    detected_header = None
                    for header in EXPERIMENT_CONFIG.keys():
                        if byte1 == header[0:1]:
                            # Si coincide el primer byte, leemos el segundo para confirmar
                            byte2 = self.ser_inst.read(1)
                            if byte2 == header[1:2]:
                                detected_header = header
                                break
                    
                    if detected_header:
                        config = EXPERIMENT_CONFIG[detected_header]
                        payload = self.ser_inst.read(config["size"])
                        
                        if len(payload) == config["size"]:
                            # Desempaquetar según el formato definido en protocol_defs
                            valores = struct.unpack(config["format"], payload)
                            self.ser_inst.read(1)
                            # --- LÍNEA PARA DEPURACIÓN ---
                            print(f"DEBUG UART RX -> Técnica: {config['nombre']} | Formato: {config['format']} | Datos: {valores}")
                            timestamp = time.time() - self.start_time
                            
                            data_dict = {
                                "tipo": config["nombre"],
                                "timestamp": round(timestamp, 4),
                                "valores": valores
                            }
                            
                            # Emitir para procesamiento y graficación
                            self.dato_procesado.emit(data_dict)
                            self.registrar_respaldo(data_dict)
                        continue

                    # 2. DETECCIÓN DE COMANDOS ASCII (HANDSHAKE)
                    # Si no es binario, leemos el resto de la línea hasta el '\n'
                    try:
                        # Decodificamos el primer byte y lo sumamos al resto de la línea
                        primera_letra = byte1.decode('ascii', errors='ignore')
                        resto_linea = self.ser_inst.readline().decode('ascii', errors='ignore')
                        linea_completa = (primera_letra + resto_linea).strip()
                        
                        # Si la palabra recibida está en nuestro diccionario de comandos
                        if linea_completa in COMANDOS_SISTEMA:
                            # --- LÍNEA PARA DEPURACIÓN DE ESTADOS ---
                            print(f"DEBUG ESP_STATE -> Comando: {linea_completa} ({COMANDOS_SISTEMA[linea_completa]})")
                            # Emitimos la clave (ej: "READY_ACK") y su descripción
                            self.comando_texto.emit(linea_completa, COMANDOS_SISTEMA[linea_completa])
                    except Exception as e:
                        # Ignorar errores de decodificación de bytes basura
                        pass

            # Cierre limpio del puerto al salir del bucle
            if self.ser_inst and self.ser_inst.is_open:
                self.ser_inst.close()

        except Exception as e:
            print(f"Error crítico en el hilo SerialWorker: {e}")
        finally:
            self.running = False

    def registrar_respaldo(self, data):
        """Guarda una copia de seguridad interna en formato texto plano"""
        try:
            with open(self.backup_path, "a") as f:
                f.write(f"[{data['timestamp']}] {data['tipo']}: {data['valores']}\n")
        except:
            pass