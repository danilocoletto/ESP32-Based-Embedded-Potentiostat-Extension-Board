# -*- mode: python ; coding: utf-8 -*-
# PyInstaller spec file — Potentiostat v1.0
# Generado para el proyecto ESP32-Based Embedded Potentiostat
#
# USO:
#   1. Copiar este archivo a la carpeta Main/ del proyecto
#   2. Abrir terminal en Main/ y ejecutar:
#      pyinstaller potentiostat.spec
#   3. El ejecutable estará en Main/dist/Potentiostat_v1.0/

import os
from PyInstaller.utils.hooks import collect_submodules, collect_data_files

# ── Imports ocultos — módulos que PyInstaller no detecta automáticamente ──────
hidden_imports = [
    # Librería estándar — necesarios por scipy/numpy en Python 3.14
    'unittest',
    'unittest.case',
    'unittest.suite',
    'unittest.loader',
    'unittest.runner',
    'unittest.signals',

    # PyQt6
    'PyQt6',
    'PyQt6.QtCore',
    'PyQt6.QtGui',
    'PyQt6.QtWidgets',
    'PyQt6.uic',
    'PyQt6.uic.uiparser',
    'PyQt6.uic.load_ui',

    # PyQtGraph y su submodulo exporters
    'pyqtgraph',
    'pyqtgraph.exporters',
    'pyqtgraph.exporters.ImageExporter',
    'pyqtgraph.exporters.SVGExporter',
    'pyqtgraph.graphicsItems',
    'pyqtgraph.graphicsItems.PlotItem',
    'pyqtgraph.graphicsItems.LegendItem',

    # SciPy — solo los submodulos usados
    'scipy',
    'scipy.signal',
    'scipy.signal._filter_design',
    'scipy.fft',

    # NumPy
    'numpy',
    'numpy.core',

    # Serial
    'serial',
    'serial.tools',
    'serial.tools.list_ports',
    'serial.tools.list_ports_common',
    'serial.tools.list_ports_windows',

    # Módulos propios
    'modules',
    'modules.controller',
    'modules.processor',
    'modules.serial_manager',
    'modules.protocol_defs',
    'modules.data_viewer',
    'modules.utils',
]

# ── Recursos a incluir — (origen, destino_dentro_del_exe) ─────────────────────
datas = [
    # Archivo UI de Qt Designer
    (os.path.join('gui', 'main_windows_gui.ui'),    'gui'),

    # Logos e iconos
    (os.path.join('logos', 'logo_GIAQA.png'),       'logos'),
    (os.path.join('logos', 'logo_GIAQA_blanca.png'),'logos'),
    (os.path.join('logos', 'logo_GIAQA_negro.png'), 'logos'),
    (os.path.join('logos', 'logo_UNS_blanco.png'),  'logos'),
    (os.path.join('logos', 'logo_UNS_negro.png'),   'logos'),
    (os.path.join('logos', 'uns_logo.svg'),          'logos'),

    # Módulos Python propios (necesarios para uic.loadUi y imports dinámicos)
    (os.path.join('modules', '*.py'), 'modules'),
]

# ── Análisis del proyecto ──────────────────────────────────────────────────────
a = Analysis(
    ['main.py'],                        # punto de entrada
    pathex=['.'],                       # directorio raíz del proyecto
    binaries=[],
    datas=datas,
    hiddenimports=hidden_imports,
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[
        # Excluir lo que no usás para reducir el tamaño del ejecutable
        'tkinter',
        'matplotlib',
        'IPython',
        'jupyter',
        'notebook',
        'pytest',
        #'unittest',
    ],
    noarchive=False,
    optimize=0,
)

# ── Compilación ───────────────────────────────────────────────────────────────
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,              # modo --onedir: binarios separados
    name='Potentiostat_v1.0',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,                           # comprimir con UPX si está instalado
    console=False,                      # sin consola negra (windowed)
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon='logos/logo_GIAQA.ico',      # descomentar si tenés un .ico
)

# ── Colección final — carpeta dist/Potentiostat_v1.0/ ────────────────────────
coll = COLLECT(
    exe,
    a.binaries,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name='Potentiostat_v1.0',
)
