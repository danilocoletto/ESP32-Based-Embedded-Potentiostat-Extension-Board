import sys
import os
import ctypes
import logging
from PyQt6 import QtWidgets, uic
from PyQt6.QtGui import QIcon, QPixmap
from PyQt6.QtCore import Qt
from PyQt6.QtWidgets import QMessageBox
from controller import PotentiostatController


# Cambiar a logging.DEBUG para ver todos los mensajes, logging.WARNING para silenciarlos en producción

#logging.basicConfig(level=logging.DEBUG, format='%(message)s')    # desarrollo — muestra todo
logging.basicConfig(level=logging.INFO, format='%(message)s')     # prudcción —  muestra info también
#logging.basicConfig(level=logging.WARNING, format='%(message)s')  # producción — solo errores

# pero para lo que venga de PyQt6 usá WARNING
logging.getLogger('PyQt6').setLevel(logging.INFO)

try:
    myappid = 'giaqa.potentiostat.v2'
    ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID(myappid)
except Exception:
    pass

class PotenciostatoApp(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        
    # --- GESTIÓN DE RUTAS DINÁMICAS ---
        base_dir = os.path.dirname(os.path.abspath(__file__))
        ui_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'gui', 'main_windows_gui.ui')
        
        # Rutas de los dos logos
        ruta_icono_ventana = os.path.join(base_dir, 'logos', 'logo_GIAQA_blanca.png') # El de la barra/esquina
        ruta_logo_interno = os.path.join(base_dir, 'logos', 'uns_logo.svg') # El SVG de adentro

        # Cargar la interfaz
        uic.loadUi(ui_path, self)

        # --- PASO 2: CONFIGURAR ICONO DE VENTANA Y BARRA DE TAREAS ---
        if os.path.exists(ruta_icono_ventana):
            pix_icono = QPixmap(ruta_icono_ventana)
            # Forzamos a que el icono se trate como un cuadrado perfecto antes de asignarlo
            # Esto evita estiramientos si la imagen original no es 1:1
            size = min(pix_icono.width(), pix_icono.height())
            rect = pix_icono.rect()
            rect.setHeight(size)
            rect.setWidth(size)
            
            icono_cuadrado = pix_icono.copy(rect)
            self.setWindowIcon(QIcon(icono_cuadrado))
        else:
            logging.warning(f"The window icon was not found in: {ruta_icono_ventana}")

        # --- PASO 3: CONFIGURAR LOGO INTERNO (SVG) ---
        if os.path.exists(ruta_logo_interno):
            # Para SVGs, QIcon es muy eficiente renderizándolos para QLabels
            # Creamos un pixmap a partir del archivo SVG
            pixmap_svg = QIcon(ruta_logo_interno).pixmap(200, 200) # Tamaño base
            
            self.LabelLogoInterno.setPixmap(pixmap_svg.scaled(
                self.LabelLogoInterno.width(), 
                self.LabelLogoInterno.height(), 
                Qt.AspectRatioMode.KeepAspectRatio, 
                Qt.TransformationMode.SmoothTransformation
            ))
            self.LabelLogoInterno.setAlignment(Qt.AlignmentFlag.AlignCenter)
        else:
            logging.warning(f"The SVG logo was not found in: {ruta_logo_interno}")



        self.setWindowTitle("Lab Potentiostat Interface V1.0")
        # Inicializar el controlador pasándole esta vista (self)
        self.controller = PotentiostatController(self)

        # Forzar estado inicial (Página 0 - EmptyPage)
        self.stackedWidget.setCurrentIndex(0)

        # --- CONEXIONES DE INTERFAZ ---
        self.init_button.clicked.connect(self.start_experiment)
        self.stop_button.clicked.connect(self.stop_experiment)

        self.clearGraph.clicked.connect(self.controller.limpiar_experimento_completo)
        
        # Acciones de Menú

        # Conectar las acciones del menú Experiment
        self.actionSWV.triggered.connect(lambda: self.controller.seleccionar_experimento("actionSWV"))
        self.actionLSV_CV.triggered.connect(lambda: self.controller.seleccionar_experimento("actionLSV/CV"))
        self.actionCPE.triggered.connect(lambda: self.controller.seleccionar_experimento("actionCPE"))

        self.actionReset_Instrument.triggered.connect(self.controller.reset_instrumento)


        self.actionScan_and_Connect.triggered.connect(self.controller.auto_conectar)
        self.actionDisconnect.triggered.connect(self.controller.detener_comunicacion)

        self.actionSaveData.triggered.connect(self.controller.guardar_datos_csv)
        self.actionSaveGraphic.triggered.connect(self.controller.guardar_grafico_imagen)
        self.actionExit.triggered.connect(self.close)
        self.actionSaveConfiguration.triggered.connect(self.controller.guardar_configuracion_ui)
        self.actionOpenConfiguration.triggered.connect(self.controller.cargar_configuracion_ui)

        # Lista de tus acciones de escala del .ui 
        acciones_escala = [
        self.action100_mA, self.action40_mA, self.action1_mA, 
        self.action100_uA, self.action10_uA, self.action1_uA, 
        self.action100_nA, self.action40_nA 
        ]

        for action in acciones_escala:
            # Usamos un truco de lambda para pasar el nombre de la acción
            action.triggered.connect(lambda checked, a=action: self.controller.cambiar_escala(a.objectName()))

    def start_experiment(self):
        if self.controller.current_experiment != "NO_EXPERIMENT":
            # Iniciamos el flujo de Handshakes
            logging.debug("Start Button Pressed")
            self.init_button.setEnabled(False)
            self.controller.limpiar_experimento_completo()
            self.controller.iniciar_flujo_experimento()

    def stop_experiment(self):
        # El comando STOP (ABORT\n) es prioritario y rompe cualquier estado
        logging.debug("Stop Button Pressed")
        self.controller.enviar_comando_uart("STOP")
        self.controller.reset_comm_flow()

    def actualizar_ui_conexion(self, estado, mensaje):
        """Maneja EXCLUSIVAMENTE los estilos visuales de la conexión"""
        self.StatusDisplay.setText(mensaje)
        
        if estado == "CONNECTED":
            self.StatusDisplay.setStyleSheet("background-color: #A7C4A0; border-radius: 0px; border: 1px solid #5F745B; color: #1A1A1A; font-weight: bold;")
            self.init_button.setEnabled(True)
            self.stop_button.setEnabled(True)
            
        elif estado == "SEARCHING":
            # Color naranja o azul suave para indicar que está "reintentando"
            self.StatusDisplay.setStyleSheet("background-color: #FDE68A; border-radius: 0px; border: 1px solid #D97706; color: #92400E; font-weight: bold;")
            self.init_button.setEnabled(False)
            
        else: # DISCONNECTED
            self.StatusDisplay.setStyleSheet("border: 1px solid #5F745B; border-radius: 0px; color: #1A1A1A; font-weight: bold;")
            self.init_button.setEnabled(False)
            self.stop_button.setEnabled(False)

    def actualizar_ui_electrodos(self, estado):
        """Maneja el estilo visual del indicador de celdas/electrodos."""
        if estado == "ENGAGED":
            # Estilo verde similar al CONNECTED de conexión
            self.ElectrodesDisplay.setText("Electrodes Status:   ENGAGED")
            self.ElectrodesDisplay.setStyleSheet(
                "background-color: #A7C4A0; border-radius: 0px; "
                "border: 1px solid #5F745B; color: #1A1A1A; font-weight: bold;"
            )
        else: # DISENGAGED
            # Estilo gris por defecto
            self.ElectrodesDisplay.setText("Electrodes Status:   DISENGAGED")
            self.ElectrodesDisplay.setStyleSheet(
                "border: 1px solid #B0BCC7; border-radius: 0px; "
                "background-color: #E6EBF0; font-weight: bold;"
            )
    
    def closeEvent(self, event):
        """Evento que se dispara al cerrar la ventana o pulsar Exit."""
        hay_datos, hay_config = self.controller.tiene_datos_pendientes()

        if hay_datos or hay_config:
            # Crear cuadro de diálogo personalizado
            mensaje = "Unsaved data or configurations detected.\n\nDo you want to exit anyway?"
            reply = QMessageBox.question(
                self, 'Confirm Exit', mensaje,
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No, 
                QMessageBox.StandardButton.No
            )

            if reply == QMessageBox.StandardButton.Yes:
                self.controller.cerrar_recursos()
                event.accept()
            else:
                event.ignore()
        else:
            # Si no hay nada, cerrar directamente
            self.controller.cerrar_recursos()
            event.accept()

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    
    # ESTILO GLOBAL PARA DIÁLOGOS
    app.setStyleSheet("""
        QMessageBox, QFileDialog {
            background-color: #F0F4F9; /* Fondo azul claro de la UI */
            font-family: "Segoe UI";
        }
        
        QMessageBox QLabel {
            color: #1A1A1A; /* Texto negro carbón */
            font-size: 13px;
        }
        
        QPushButton {
            background-color: #FFFFFF;
            border: 1px solid #B0BCC7;
            border-radius: 4px;
            padding: 5px 15px;
            color: #1A1A1A;
            min-width: 80px;
        }
        
        QPushButton:hover {
            border: 1px solid #7092BE;
            background-color: #E6EBF0;
        }
        
        /* Estilo para el explorador de archivos */
        QFileDialog QListView, QFileDialog QTreeView {
            background-color: #FFFFFF;
            border: 1px solid #D1D9E0;
        }
        
        QLineEdit {
            border: 1px solid #B0BCC7;
            border-radius: 4px;
            padding: 3px;
        }
    """)
    
    window = PotenciostatoApp()
    window.show()
    sys.exit(app.exec())