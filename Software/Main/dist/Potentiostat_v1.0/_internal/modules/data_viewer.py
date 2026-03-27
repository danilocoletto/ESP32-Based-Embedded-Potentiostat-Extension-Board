import os
import csv
import numpy as np
import pyqtgraph as pg
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QPushButton,
    QFileDialog, QDialog, QListWidget, QListWidgetItem,
    QDialogButtonBox, QLabel
)
import pyqtgraph as pg
import pyqtgraph.exporters  # ← agregar esta línea
from PyQt6.QtCore import Qt
from PyQt6.QtGui import QColor

# Paleta de colores para las curvas
CURVE_COLORS = [
    '#0078D4', '#D83B01', '#107C10', '#6B2D8B',
    '#FFB900', '#00B7C3', '#E74856', '#498205',
    '#FF8C00', '#0099BC', '#7A7574', '#C239B3'
]


class DataViewerWindow(QWidget):
    """Ventana independiente para visualización y comparación de datos CSV."""

    def __init__(self, on_close_callback=None):
        super().__init__()
        self.setWindowTitle("Data Viewer")
        self.resize(900, 650)
        self.setWindowFlags(Qt.WindowType.Window)  # ventana independiente

        # Registro de curvas: {nombre: {curve, color, data}}
        self._curves = {}
        self._color_index = 0
        self._unsaved_changes = False
        self._on_close_callback = on_close_callback  # ← guardar la referencia

        self.setStyleSheet("""
        QWidget {
            background-color: #F0F4F9;
            font-family: 'Segoe UI';
            color: #1A1A1A;
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
        """)
        
        self._build_ui()


    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(8)

        # ── Barra de botones ──────────────────────────────────────────
        btn_layout = QHBoxLayout()

        self.btn_add = QPushButton("📂 Add Dataset")
        self.btn_add.setFixedHeight(32)
        self.btn_add.setStyleSheet("""
            QPushButton {
                background-color: #FFFFFF; border: 1px solid #B0BCC7;
                border-radius: 4px; padding: 4px 14px;
            }
            QPushButton:hover { border: 1px solid #0078D4; background-color: #E6EBF0; }
        """)
        self.btn_add.clicked.connect(self._add_dataset)

        self.btn_remove = QPushButton("🗑️ Remove Dataset")
        self.btn_remove.setFixedHeight(32)
        self.btn_remove.setStyleSheet("""
            QPushButton {
                background-color: #FFFFFF; border: 1px solid #B0BCC7;
                border-radius: 4px; padding: 4px 14px;
            }
            QPushButton:hover { border: 1px solid #D83B01; background-color: #FEE2E2; }
        """)
        self.btn_remove.clicked.connect(self._remove_dataset)
        self.btn_remove.setEnabled(False)

        self.btn_save = QPushButton("💾 Save Graph")
        self.btn_save.setFixedHeight(32)
        self.btn_save.clicked.connect(self._save_graph_viewer)  # ← no _save_graph
        self.btn_save.setEnabled(False)  # se habilita cuando hay datos cargados

        btn_layout.addWidget(self.btn_add)
        btn_layout.addWidget(self.btn_remove)
        btn_layout.addWidget(self.btn_save)  # ← agregar acá
        btn_layout.addStretch()
        layout.addLayout(btn_layout)

        # ── Gráfico ───────────────────────────────────────────────────
        self.plot_widget = pg.PlotWidget()
        self.plot_widget.setBackground('w')
        self.plot_widget.showGrid(x=True, y=True, alpha=0.3)

        axis_style = {
            'color': '#1A1A1A', 'font-size': '13px',
            'font-family': 'Segoe UI', 'font-weight': 'bold'
        }
        self.plot_widget.setLabel('bottom', 'Potential', units='mV', **axis_style)
        self.plot_widget.setLabel('left',   'Current',   units='µA', **axis_style)

        for axis in ('left', 'bottom'):
            self.plot_widget.getAxis(axis).setPen('k')
            self.plot_widget.getAxis(axis).setTextPen('k')
            self.plot_widget.getAxis(axis).enableAutoSIPrefix(False)

        # Leyenda
        self.legend = self.plot_widget.addLegend(offset=(10, 10))
        self.legend.setLabelTextColor('k')

        layout.addWidget(self.plot_widget)

    # ── Helpers ───────────────────────────────────────────────────────

    def _next_color(self):
        color = CURVE_COLORS[self._color_index % len(CURVE_COLORS)]
        self._color_index += 1
        return color

    def _load_csv(self, file_path):
        try:
            with open(file_path, newline='') as f:
                reader = csv.DictReader(f)
                fieldnames = set(reader.fieldnames or [])

                # CPE: tiene Charge (C) — columna exclusiva
                if 'Charge (C)' in fieldnames:
                    x, i = [], []
                    for row in reader:
                        x.append(float(row['Timestamp (s)']))
                        i.append(float(row['Current_filtered (mA)']))
                    return np.array(x), np.array(i), 'CPE'

                # SWV/LSV/DPV: tiene v_applied_mv — columna exclusiva
                elif 'V_applied (mV)' in fieldnames:
                    x, i = [], []
                    for row in reader:
                        x.append(float(row['V_applied (mV)']))
                        i.append(float(row['Current_filtered (mA)']))
                    return np.array(x), np.array(i), 'VOLTAMMETRY'

                return None
        except Exception:
            return None

    def _autorange(self):
        self.plot_widget.autoRange(padding=0.05)

    # ── Acciones ──────────────────────────────────────────────────────


    def _plot_dataset(self, name, x, i_ma, tipo):
        """Plotea una curva aplicando auto-escala de unidades y configura ejes según tipo."""
        # Auto-escala de corriente
        max_val = np.max(np.abs(i_ma)) if len(i_ma) > 0 else 0
        if max_val < 0.001:
            mult, unit = 1e6, "nA"
        elif max_val < 1.0:
            mult, unit = 1e3, "µA"
        else:
            mult, unit = 1.0, "mA"

        color = self._next_color()
        curve = self.plot_widget.plot(
            x, i_ma * mult,
            pen=pg.mkPen(color=color, width=2),
            name=name
        )
        self._curves[name] = {'curve': curve, 'color': color, 'tipo': tipo}

        # Configurar ejes según técnica
        axis_style = {
            'color': '#1A1A1A', 'font-size': '13px',
            'font-family': 'Segoe UI', 'font-weight': 'bold'
        }
        if tipo == 'CPE':
            self.plot_widget.setLabel('bottom', 'Time', units='s', **axis_style)
        else:
            self.plot_widget.setLabel('bottom', 'Potential', units='mV', **axis_style)

        self.plot_widget.setLabel('left', 'Current', units=unit, **axis_style)

    def _add_dataset(self):
        file_paths, _ = QFileDialog.getOpenFileNames(
            self, "Open Dataset", "", "CSV Files (*.csv);;All Files (*)"
        )
        if not file_paths:
            return

        for file_path in file_paths:
            name = os.path.splitext(os.path.basename(file_path))[0]

            base_name = name
            counter = 2
            while name in self._curves:
                name = f"{base_name} ({counter})"
                counter += 1

            result = self._load_csv(file_path)
            if result is None:
                from PyQt6.QtWidgets import QMessageBox
                QMessageBox.warning(self, "Invalid File",
                    f"'{os.path.basename(file_path)}' does not have the expected format and was skipped.\n\n"
                    "Required columns: v_applied_mv + current_filtered_ma  OR  Timestamp (s) + Current_filtered (mA)")
                continue

            x, i, tipo = result
            self._plot_dataset(name, x, i, tipo)

        if self._curves:
            self.btn_remove.setEnabled(True)
            self.btn_save.setEnabled(True)
            self._unsaved_changes = True

        self._autorange()

    def _remove_dataset(self):
        if not self._curves:
            return

        # Diálogo de selección múltiple
        dialog = _RemoveDialog(list(self._curves.keys()), self._curves, self)
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return

        selected = dialog.selected_names()
        for name in selected:
            if name in self._curves:
                self.plot_widget.removeItem(self._curves[name]['curve'])
                del self._curves[name]

        if not self._curves:
            self.btn_remove.setEnabled(False)
            self.btn_save.setEnabled(False)  # ← agregar
            self._color_index = 0  # reset colores cuando queda vacío

        self._unsaved_changes = True
        self._autorange()

    # ── API pública — llamada desde el controller ─────────────────────

    def load_and_show(self, file_path):
        """Abre un archivo directamente sin diálogo (llamada inicial desde el menú)."""
        result = self._load_csv(file_path)
        if result is None:
            from PyQt6.QtWidgets import QMessageBox
            QMessageBox.warning(self, "Invalid File",
                "The selected file does not have the expected format.")
            return

        name = os.path.splitext(os.path.basename(file_path))[0]
        base_name = name
        counter = 2
        while name in self._curves:
            name = f"{base_name} ({counter})"
            counter += 1

        x, i, tipo = result
        self._plot_dataset(name, x, i, tipo)

        self.btn_remove.setEnabled(True)
        self.btn_save.setEnabled(True)   # ← agregar
        self._unsaved_changes = True
        self._autorange()
        self.show()
        self.raise_()

    def _save_graph_viewer(self):
        """Guardado específico del viewer — actualiza el flag de cambios pendientes."""
        file_path, _ = QFileDialog.getSaveFileName(
            self, "Save Viewer Graph", "", "PNG Image (*.png);;JPEG Image (*.jpg)"
        )
        if file_path:
            try:
                exporter = pg.exporters.ImageExporter(self.plot_widget.plotItem)
                exporter.export(file_path)
                self._unsaved_changes = False  # ← solo se resetea acá
            except Exception as e:
                from PyQt6.QtWidgets import QMessageBox
                QMessageBox.warning(self, "Export Error", f"Could not save image:\n{e}")

    def closeEvent(self, event):
        """Al cerrar: ofrecer guardar el gráfico y limpiar todo de memoria."""
        if self._curves and self._unsaved_changes:
            from PyQt6.QtWidgets import QMessageBox
            reply = QMessageBox.question(
                self, "Close Data Viewer",
                "Do you want to save the current graph before closing?",
                QMessageBox.StandardButton.Yes |
                QMessageBox.StandardButton.No  |
                QMessageBox.StandardButton.Cancel,
                QMessageBox.StandardButton.Cancel
            )

            if reply == QMessageBox.StandardButton.Cancel:
                event.ignore()
                return

            if reply == QMessageBox.StandardButton.Yes:
                self._save_graph_viewer()  # ← no _save_graph

        # Limpiar todas las curvas y datos
        self._clear_all()

        # Notificar al controller para que libere la referencia
        if self._on_close_callback:
            self._on_close_callback()

        event.accept()


    def _clear_all(self):
        """Elimina todas las curvas y libera memoria."""
        for entry in self._curves.values():
            self.plot_widget.removeItem(entry['curve'])
        self._curves.clear()
        self._color_index = 0
        self.btn_remove.setEnabled(False)
        self.btn_save.setEnabled(False)  # ← agregar

class _RemoveDialog(QDialog):
    """Diálogo para seleccionar una o varias curvas a eliminar."""

    def __init__(self, names, curves_dict, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Remove Dataset")
        self.setMinimumWidth(320)

        self.setStyleSheet("""
            QDialog {
                background-color: #F0F4F9;
                font-family: 'Segoe UI';
            }
            QLabel {
                color: #1A1A1A;
                font-size: 13px;
            }
            QListWidget {
                background-color: #FFFFFF;
                border: 1px solid #D1D9E0;
                border-radius: 4px;
                padding: 3px;
                font-size: 13px;
            }
            QListWidget::item:selected {
                background-color: #CCE4F7;
                color: #1A1A1A;
            }
        """)
        layout = QVBoxLayout(self)

        layout.addWidget(QLabel("Select datasets to remove:"))

        self.list_widget = QListWidget()
        self.list_widget.setSelectionMode(
            QListWidget.SelectionMode.ExtendedSelection  # permite selección múltiple
        )

        for name in names:
            item = QListWidgetItem(name)
            color = curves_dict[name]['color']
            item.setForeground(QColor(color))
            self.list_widget.addItem(item)

        layout.addWidget(self.list_widget)

        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok |
            QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def selected_names(self):
        return [item.text() for item in self.list_widget.selectedItems()]
    

    