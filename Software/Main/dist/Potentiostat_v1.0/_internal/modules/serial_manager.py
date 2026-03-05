import os
import time
import serial
import struct
import logging
import serial.tools.list_ports
from PyQt6.QtCore import QThread, pyqtSignal
from modules.protocol_defs import EXPERIMENT_CONFIG, COMANDOS_SISTEMA
from modules.utils import get_writable_dir



class ScannerWorker(QThread):
    """Hilo dedicado al escaneo de puertos — no bloquea la UI."""
    dispositivo_encontrado = pyqtSignal(str)   # emite el puerto: "COM3"
    escaneo_fallido = pyqtSignal()             # emite si no encontró nada

    def __init__(self, baud=921600):
        super().__init__()
        self.baud = baud

    def run(self):
        puerto = SerialScanner.encontrar_potenciostato(baud=self.baud)
        if puerto:
            self.dispositivo_encontrado.emit(puerto)
        else:
            self.escaneo_fallido.emit()



class SerialScanner:
    @staticmethod
    def encontrar_potenciostato(baud=921600, id_esperado = COMANDOS_SISTEMA["ID"]):
        # 1. Obtener lista de todos los puertos COM físicos/virtuales
        puertos = serial.tools.list_ports.comports()
        
        for p in puertos:
            logging.info(f"Trying Port {p.device} ...")
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
                    logging.info(f"¡Device found in {p.device}!")
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
    dispositivo_desconectado = pyqtSignal()  # ← agregar esta línea

    def __init__(self, port, baud):
        super().__init__()
        self.port = port
        self.baud = baud
        self.running = False
        self.start_time = 0
        self.ser_inst = None  # Instancia persistente para Tx/Rx
        
        # Archivo de respaldo interno
        self.backup_path = os.path.join(get_writable_dir(), "internal_backup.log")
        self._backup_file = None  # ← archivo persistente durante el ensayo

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
                logging.error(f"Error while sending data throught UART: {e}")

    def stop(self):
        """Detiene el bucle del hilo de forma segura"""
        self.running = False


    def run(self):
        """
        Bucle principal del hilo que gestiona la lectura de datos binarios de experimentos
        y mensajes de control ASCII (handshake) desde la ESP32.
        """
        try:
            self.ser_inst = serial.Serial(self.port, self.baud, timeout=0.1)
            self.running = True
            self.start_time = time.time()

            while self.running:
                try:
                    if self.ser_inst.in_waiting > 0:
                        byte1 = self.ser_inst.read(1)

                        # 1. DETECCIÓN DE ENCABEZADO BINARIO (EXPERIMENTOS)
                        detected_header = None
                        for header in EXPERIMENT_CONFIG.keys():
                            if byte1 == header[0:1]:
                                byte2 = self.ser_inst.read(1)
                                if len(byte2) == 0:
                                    continue
                                if byte2 == header[1:2]:
                                    detected_header = header
                                    break
                        
                        if detected_header:
                            config = EXPERIMENT_CONFIG[detected_header]
                            payload = self.ser_inst.read(config["size"])
                            
                            if len(payload) == config["size"]:
                                valores = struct.unpack(config["format"], payload)
                                self.ser_inst.read(1)
                                logging.debug(f"DEBUG UART RX -> Técnica: {config['nombre']} | Formato: {config['format']} | Datos: {valores}")
                                timestamp = time.time() - self.start_time
                                
                                data_dict = {
                                    "tipo": config["nombre"],
                                    "timestamp": round(timestamp, 4),
                                    "valores": valores
                                }
                                
                                self.dato_procesado.emit(data_dict)
                                self.registrar_respaldo(data_dict)
                            continue

                        # 2. DETECCIÓN DE COMANDOS ASCII (HANDSHAKE)
                        try:
                            primera_letra = byte1.decode('ascii', errors='ignore')
                            resto_linea = self.ser_inst.readline().decode('ascii', errors='ignore')
                            linea_completa = (primera_letra + resto_linea).strip()
                            
                            if linea_completa in COMANDOS_SISTEMA:
                                self.comando_texto.emit(linea_completa, COMANDOS_SISTEMA[linea_completa])
                        except Exception:
                            pass
                    else:
                        time.sleep(0.005)

                except (serial.SerialException, OSError):
                    # Desconexión física detectada — notificar al controller y salir
                    logging.warning("Physical disconnection detected in SerialWorker.")
                    self.dispositivo_desconectado.emit()
                    break

            if self.ser_inst and self.ser_inst.is_open:
                try:
                    self.ser_inst.close()
                except Exception:
                    pass

        except Exception as e:
            logging.error(f"Critical Error in SerialWorker Thread: {e}")
            import traceback
            traceback.print_exc()
        finally:
            self.running = False
            self.cerrar_archivo_respaldo()


    def registrar_respaldo(self, data):
        """Escribe en el archivo ya abierto — sin abrir/cerrar en cada llamada."""
        if self._backup_file:
            try:
                self._backup_file.write(f"[{data['timestamp']}] {data['tipo']}: {data['valores']}\n")
                # flush() periódico lo maneja el timer, no acá
            except Exception as e:
                logging.error(f"Error while writing backup file: {e}")


    def resetear_archivo_respaldo(self):
        """Cierra el archivo anterior si existe y abre uno nuevo limpio."""
        self.cerrar_archivo_respaldo()
        try:
            self._backup_file = open(self.backup_path, "w")
            self._backup_file.write(f"--- Nuevo Ensayo Iniciado: {time.ctime()} ---\n")
        except Exception as e:
            logging.error(f"Error while opening backup file: {e}")
            self._backup_file = None


    def cerrar_archivo_respaldo(self):
        """Flush final y cierre seguro del archivo de backup."""
        if self._backup_file:
            try:
                self._backup_file.flush()
                self._backup_file.close()
            except Exception as e:
                logging.error(f"Error while closening backup file: {e}")
            finally:
                self._backup_file = None

