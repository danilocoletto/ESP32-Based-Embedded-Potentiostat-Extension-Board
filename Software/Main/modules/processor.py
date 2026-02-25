import numpy as np
import pyqtgraph as pg
import pyqtgraph.exporters

class DataProcessor:
    def __init__(self, window_size=5):
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


class PotentiostatGraph:
    def __init__(self, plot_widget):
        self.plot = plot_widget
        # Fondo blanco sólido
        self.plot.setBackground('w') 
        
        # Configurar ejes para que sean visibles sobre fondo blanco
        self.plot.showGrid(x=True, y=True, alpha=0.3)
        self.plot.getAxis('left').setPen('k')
        self.plot.getAxis('bottom').setPen('k')
        self.plot.getAxis('left').setTextPen('k')
        self.plot.getAxis('bottom').setTextPen('k')

        # Curva para datos crudos (puntos azules tipo Windows 11) y filtrados (roja)
        # Usamos el color #7092BE que ya tienes en tu UI para coherencia visual
        self.curve_raw = self.plot.plot(pen=pg.mkPen(color='#7092BE', width=2), symbol=None)
        #self.curve_raw = self.plot.plot(pen=None, symbol='o', symbolSize=4, symbolBrush='#7092BE')
        self.curve_filtered = self.plot.plot(pen=pg.mkPen('r', width=2))
        self.plot.setFixedSize(606, 575) # Fuerza el tamaño exacto del diseño
        
    def update(self, history):
        if not history: return
        data = np.array(history)
        v_mv = data[:, 1]
        i_ma = data[:, 2]
        i_filt_ma = data[:, 3]

        # Auto-escala lógica: selecciona unidad y multiplicador
        max_val = np.max(np.abs(i_ma))
        if max_val < 0.001: 
            mult, unit = 1e6, "nA"
        elif max_val < 1.0: 
            mult, unit = 1e3, "uA"
        else: 
            mult, unit = 1.0, "mA"

        self.plot.setLabel('left', 'Current', units=unit)
        self.plot.setLabel('bottom', 'Potential', units='mV')
        
        # Actualizar curvas con la conversión de unidad solo para visualización
        self.curve_raw.setData(v_mv, i_ma * mult)
        self.curve_filtered.setData(v_mv, i_filt_ma * mult)
        self.plot.autoRange(padding=0.05)


    def export_image(self, file_path):
        """Exporta el contenido actual del gráfico a una imagen"""
        exporter = pg.exporters.ImageExporter(self.plot.plotItem)
        # Ajustar calidad o parámetros si es necesario
        exporter.export(file_path)
    
    def clear_graph(self):
        """Borra las líneas de las curvas en el widget de pyqtgraph."""
        self.curve_raw.setData([], [])
        self.curve_filtered.setData([], [])