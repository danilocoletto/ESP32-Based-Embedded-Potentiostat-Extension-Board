# modules/utils.py
import os
import sys

def get_resource_dir():
    """Directorio de recursos de solo lectura (UI, logos, .ui files)."""
    if getattr(sys, 'frozen', False):
        return sys._MEIPASS
    
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def get_writable_dir():
    """Directorio escribible para archivos generados por la app."""
    if getattr(sys, 'frozen', False):
        return os.path.dirname(sys.executable)
    
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))