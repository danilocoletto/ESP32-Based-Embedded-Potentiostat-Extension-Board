import sys
import os
from PyQt6 import QtWidgets, uic
from controller import PotentiostatController

class PotenciostatoApp(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        
        # Carga de UI
        ui_path = os.path.join(os.path.dirname(__file__), 'gui', 'main_windows_gui.ui')
        uic.loadUi(ui_path, self)

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
        self.actionLSV.triggered.connect(lambda: self.controller.seleccionar_experimento("actionLSV"))
        self.actionCPE.triggered.connect(lambda: self.controller.seleccionar_experimento("actionCPE"))

        self.actionScan_and_Connect.triggered.connect(self.controller.auto_conectar)
        self.actionDisconnect.triggered.connect(self.controller.detener_comunicacion)

        self.actionSaveData.triggered.connect(self.controller.guardar_datos_csv)
        self.actionSaveGraphic.triggered.connect(self.controller.guardar_grafico_imagen)

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
            self.controller.limpiar_experimento_completo()
            print("Gráfico e historial de datos reiniciados.")
            self.controller.iniciar_flujo_experimento()

    def stop_experiment(self):
        # El comando STOP (ABORT\n) es prioritario y rompe cualquier estado
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
            self.StatusDisplay.setStyleSheet("border: 1px solid #7092BE; border-radius: 0px; background-color: #F0F4F9; font-weight: bold;")
            
        else: # DISCONNECTED
            self.StatusDisplay.setStyleSheet("border: 1px solid #5F745B; border-radius: 0px; color: #1A1A1A; font-weight: bold;")
            self.init_button.setEnabled(False)
            self.stop_button.setEnabled(False)

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    window = PotenciostatoApp()
    window.show()
    sys.exit(app.exec())