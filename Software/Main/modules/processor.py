import numpy as np
import pyqtgraph as pg
from collections import deque
from scipy.signal import butter, filtfilt


def aplicar_filtro_pasabajos(data_history, fs_estimada=None):
    """
    Filtro Butterworth pasabajos de orden 4, fc=50Hz.
    Aplicar sobre current_filtered_ma (columna 3) ya procesada por SG.
    """
    if len(data_history) < 20:
        return data_history
    
    data = np.array(data_history)
    
    # Estimar Fs desde los timestamps si no se provee
    if fs_estimada is None:
        dt = np.median(np.diff(data[:, 0]))  # columna timestamp
        fs = 1.0 / dt if dt > 0 else 5000.0
    else:
        fs = fs_estimada
    
    # Frecuencia de corte normalizada (fc / (fs/2))
    fc = 50.0  # Hz
    nyq = fs / 2.0
    
    if fc >= nyq:
        # Si fs es muy baja, el filtro no tiene sentido
        return data_history
    
    Wn = fc / nyq
    b, a = butter(4, Wn, btype='low')
    
    # Aplicar filtro zero-phase (no agrega lag) sobre la corriente filtrada por SG
    data[:, 3] = filtfilt(b, a, data[:, 3])
    
    return data.tolist()

class DataProcessor:
    def __init__(self, median_size=5, window_size=61, poly_order=3):
        # Asegurar que window_size sea impar para SG
        if window_size % 2 == 0: window_size += 1
        
        self.window_size = window_size
        self.poly_order = poly_order
        self.median_size = median_size
        
        # Coeficientes para el ÚLTIMO punto de la ventana (evita el lag)
        self._coeffs = self._calculate_savgol_coeffs_end(window_size, poly_order)
        
        self.data_history = [] 
        self._buffer_raw    = deque(maxlen=self.median_size)    # Buffer para el filtro de mediana
        self._buffer_median = deque(maxlen=self.window_size)    # Buffer para el filtro Savitzky-Golay

    def get_filtered_history(self, apply_lowpass=True, fc=50.0):
        """Retorna el historial con filtro pasabajos opcional sobre la corriente filtrada."""
        if not self.data_history or not apply_lowpass:
            return self.data_history
        return aplicar_filtro_pasabajos(self.data_history, fs_estimada=None)

    def _calculate_savgol_coeffs_end(self, window_size, order):
        """Calcula los coeficientes para el ÚLTIMO punto de la ventana (Causal)."""
        # Creamos la matriz de diseño de -window_size+1 hasta 0
        # El 0 representa el punto actual (el más reciente)
        t = np.arange(-window_size + 1, 1)

        b = np.array([[k**i for i in range(order + 1)] for k in t])

        # La última fila de la pseudo-inversa nos da los coeficientes 
        # para estimar el punto en t=0 (el extremo final)

        m = np.linalg.pinv(b)[0]
        return m

    def process_sample(self, timestamp, v_applied_mv, v_adc_v, gain_ohms):
        # 1. Conversión a Corriente
        raw_current_ma = (v_adc_v / gain_ohms) * 1000
        
        # 2. Filtro de Mediana — append() es O(1), maxlen descarta el viejo automáticamente
        self._buffer_raw.append(raw_current_ma)
        current_median = np.median(self._buffer_raw)
        
        # 3. Savitzky-Golay Causal — igual, O(1) por muestra
        self._buffer_median.append(current_median)
            
        if len(self._buffer_median) == self.window_size:
            # np.dot acepta deque directamente vía el protocolo de buffer
            current_filtered_ma = np.dot(self._coeffs, self._buffer_median)
        else:
            current_filtered_ma = current_median
            
        # 4. Guardar y Retornar
        sample = [timestamp, v_applied_mv, raw_current_ma, current_filtered_ma]
        self.data_history.append(sample)
        
        return sample
    
    
    def clear_data(self):
        """Limpia el historial y reinicia buffers."""
        self.data_history = []
        self._buffer_raw.clear()
        self._buffer_median.clear()

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

        if action_name in ["NO_EXPERIMENT", "actionSWV", "actionLSV/CV"]:
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