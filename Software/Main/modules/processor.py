import numpy as np
import pyqtgraph as pg
import pyqtgraph.exporters

class DataProcessor:
    def __init__(self, window_size=7):
        self.window_size = window_size
        # Estructura requerida: [timestamp, voltage_mv, current_ma, current_filtered_ma]
        self.data_history = [] 
        self._buffer_filtrado = []

    def process_sample(self, timestamp, v_applied_mv, v_adc_v, gain_ohms):
        # Conversión: Tensión ADC (V) / Ganancia (Ω) = Corriente (mA)
        current_ma = (v_adc_v / gain_ohms) * 1000
        
        # Filtro de Media Móvil
        self._buffer_filtrado.append(current_ma)
        if len(self._buffer_filtrado) > self.window_size:
            self._buffer_filtrado.pop(0)
        current_filtered_ma = sum(self._buffer_filtrado) / len(self._buffer_filtrado)
        
        # Guardar en el arreglo con el orden solicitado
        sample = [timestamp, v_applied_mv, current_ma, current_filtered_ma]
        self.data_history.append(sample)
        return sample
    
    def clear_data(self):
        """Limpia el historial de datos y los buffers de filtrado."""
        self.data_history = []
        self._buffer_filtrado = []
    """""
    def __init__(self, alpha=0.3):
        #alpha: Factor de suavizado (0.0 a 1.0).
        #       - 0.1: Filtrado muy fuerte (curva muy suave, pero con más retraso).
        #       - 0.5: Filtrado equilibrado.
        #       - 0.8: Filtrado leve (sigue mucho al ruido).
        self.alpha = alpha
        self.data_history = [] 
        self._last_filtered_val = None # Reemplaza al buffer de lista

    def process_sample(self, timestamp, v_applied_mv, v_adc_v, gain_ohms):
        # Conversión: Tensión ADC (V) / Ganancia (Ω) = Corriente (mA)
        current_ma = (v_adc_v / gain_ohms) * 1000
        
        # Inicialización en la primera muestra
        if self._last_filtered_val is None:
            self._last_filtered_val = current_ma
        
        # --- FILTRO EMA (Fórmula: S_t = α * X_t + (1 - α) * S_{t-1}) ---
        current_filtered_ma = (self.alpha * current_ma) + ((1 - self.alpha) * self._last_filtered_val)
        
        # Guardamos para la siguiente iteración
        self._last_filtered_val = current_filtered_ma
        
        # Guardar en el historial
        sample = [timestamp, v_applied_mv, current_ma, current_filtered_ma]
        self.data_history.append(sample)
        return sample

    def clear_data(self):
        #Limpia el historial y el estado del filtro.
        self.data_history = []
        self._last_filtered_val = None
    """

class PotentiostatGraph:
    def __init__(self, plot_widget):
        self.plot = plot_widget
        self.plot.setBackground('w') 
        
        # Configure grid and axes colors
        self.plot.showGrid(x=True, y=True, alpha=0.3)
        self.plot.getAxis('left').setPen('k')
        self.plot.getAxis('bottom').setPen('k')
        self.plot.getAxis('left').setTextPen('k')
        self.plot.getAxis('bottom').setTextPen('k')

        # Disable automatic SI prefixing (prevents things like "k" or "m" before units)
        self.plot.getAxis('left').enableAutoSIPrefix(False)
        self.plot.getAxis('bottom').enableAutoSIPrefix(False)

        # Curves: Raw (Steel Blue) and Filtered (Red)
        self.curve_raw = self.plot.plot(pen=pg.mkPen(color="#0078D4", width=1.5), symbol=None)
        self.curve_filtered = self.plot.plot(pen=pg.mkPen('r', width=2))
        
        # Initial labels
        self.set_experiment_mode("NO_EXPERIMENT") # Default mode
        self.plot.setFixedSize(606, 575) 

    def set_experiment_mode(self, action_name):
        """Reconfigures the axes titles and units based on the experiment type."""

        # Definimos el estilo en un diccionario para reutilizarlo
        axis_style = {
        'color': '#1A1A1A',          # Negro casi puro para legibilidad
        'font-size': '14px',         # Tamaño acorde a tus otros parámetros 
        'font-family': 'Segoe UI',   # Coherencia con el resto de la app 
        'font-weight': 'bold'        # <--- ESTO PONE LA LETRA EN NEGRITA
        }

        if action_name in ["NO_EXPERIMENT", "actionSWV", "actionLSV"]:
            self.plot.setLabel('bottom', 'Potential', units='mV', **axis_style)
            self.plot.setLabel('left', 'Current', units='mA', **axis_style)
        elif action_name == "actionCPE":
            self.plot.setLabel('bottom', 'Time', units='s', **axis_style)
            self.plot.setLabel('left', 'Current', units='mA', **axis_style)

    def update(self, history, experiment_id=None):
        if not history: 
            return
            
        data = np.array(history)
        
        # Usamos experiment_id para decidir el eje X (Time vs Potential)
        if experiment_id == "actionCPE":
            x_data = data[:, 0] # Columna de Timestamp 
        else:
            x_data = data[:, 1] # Columna de Potential 
            
        i_ma = data[:, 2] # Corriente cruda 
        i_filt_ma = data[:, 3] # Corriente filtrada 

        # Lógica de Auto-escala para unidades (se mantiene igual) 
        max_val = np.max(np.abs(i_ma))
        if max_val < 0.001: 
            mult, unit = 1e6, "nA"
        elif max_val < 1.0: 
            mult, unit = 1e3, "µA"
        else: 
            mult, unit = 1.0, "mA"

        # Actualizamos la etiqueta del eje Y con la unidad detectada 
        self.plot.getAxis('left').setLabel('Current', units=unit)
        
        # Cargamos los datos en las curvas
        self.curve_raw.setData(x_data, i_ma * mult)
        self.curve_filtered.setData(x_data, i_filt_ma * mult)
        
        self.plot.autoRange(padding=0.05)

    def export_image(self, file_path):
        """Exports graph to image """
        exporter = pg.exporters.ImageExporter(self.plot.plotItem)
        exporter.export(file_path)
    
    def clear_graph(self):
        """Clears curves """
        self.curve_raw.setData([], [])
        self.curve_filtered.setData([], [])