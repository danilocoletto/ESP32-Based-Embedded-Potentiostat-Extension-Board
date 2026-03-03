
SCALES_CONFIG = {
    "action100_mA": {
        "name": "up to 100 mA",
        "id_hardware": 0,
        "gain_label": "NO GAIN",
        "multiplier": 1
    },
    "action40_mA": {
        "name": "up to 40 mA",
        "id_hardware": 1,
        "gain_label": "100 Ω",
        "multiplier": 100
    },
    "action1_mA": {
        "name": "up to 1 mA",
        "id_hardware": 2,
        "gain_label": "3 kΩ",
        "multiplier": 3000
    },
    "action100_uA": {
        "name": "up to 100 uA",
        "id_hardware": 3,
        "gain_label": "30 kΩ",
        "multiplier": 30000
    },
    "action10_uA": {
        "name": "up to 10 uA",
        "id_hardware": 4,
        "gain_label": "300 kΩ",
        "multiplier": 300000
    },
    "action1_uA": {
        "name": "up to 1 uA",
        "id_hardware": 5,
        "gain_label": "3 MΩ",
        "multiplier": 3000000
    },
    "action100_nA": {
        "name": "up to 100 nA",
        "id_hardware": 6,
        "gain_label": "30 MΩ",
        "multiplier": 30000000
    },
    "action40_nA": {
        "name": "up to 40 nA",
        "id_hardware": 7,
        "gain_label": "100 MΩ",
        "multiplier": 100000000
    }
}

# Mapeo de encabezados a su nombre y su formato de estructura binaria
# h: short (2 bytes), f: float (4 bytes), i: int (4 bytes), d: double (8 bytes)
EXPERIMENT_CONFIG = {
    b'\xaa\xbb': {
        "nombre": "Square Wave Voltammetry",
        "format": "<hff",   # Index, Forward, Reverse
        "size": 10,          # 2 + 4 + 4
        "suffix_size": 1 # Agregamos una marca para el byte de fin
    },
    b'\xcc\xdd': {
        "nombre": "Linear/Cyclic Voltammetry",
        "format": "<ff",    # Index, Current
        "size": 8,           # 4
        "suffix_size": 1 # Agregamos una marca para el byte de fin
    },
    b'\xee\xff': {
        "nombre": "Controlled Potential Electrolysis",
        "format": "<hff",   # Index, Voltage, Current
        "size": 10,
        "suffix_size": 1 # Agregamos una marca para el byte de fin
    },
    b'\xab\xab': {
        "nombre": "Chronoamperometry",
        "format": "<ff",    # Time_Internal, Current
        "size": 8
    },
    b'\xcd\xcd': {
        "nombre": "Potentiometry",
        "format": "<f",     # Voltage_Only
        "size": 4
    }
}

EXPERIMENT_PAGES = {
    "NO_EXPERIMENT": {"index": 0, "nombre": "None", "page_name": "EmptyPage"},
    "actionSWV": {"index": 1, "nombre": "Square Wave Voltammetry", "page_name": "page1_SWV"},
    "actionLSV/CV": {"index": 2, "nombre": "Linear/Cyclic Voltammetry", "page_name": "page2_LSV_CV"},
    "actionCPE": {"index": 3, "nombre": "Controlled Potential Electrolysis", "page_name": "page3_CPE"}
}


COMANDOS_SISTEMA = {                                # VER SI ES NECESARIO PONERLES EL \n PARA INTERPRETARLOS COMO COMANDOS
    "OK":           "ESP: Operation Succesful",                      # QUE VIENEN DESDE LA ESP
    "ER":           "ESP: Hardware Error",
    "ID":           "POTENTIOSTAT_V2.0",
    "STOP_OK":      "ESP: Abortion Activated",
    "CONF_OK":      "ESP: Exp Config OK",
    "READY_ACK":    "ESP: Ready_UP Order Received",
    "CONF_GAIN_OK": "ESP: Gain Configuration OK",
    "CONF_ERR":     "ESP: Configuration Error",
    "WAITING":      "ESP: Waiting State",
    "PREPARING":    "ESP: Awaiting Configuration",
    "EXECUTING":    "ESP: Experiment Executing",
    "FINISHED":     "ESP: Experiment Finished",
    "RESETTING":    "ESP: Device Resetting",
    "CELL1":        "CELL ENGAGED",
    "CELL0":        "CELL DISENGAGED",
    "GAIN0":        "GAIN1. Scale 100mA",
    "GAIN1":        "GAIN100. Scale 40mA",
    "GAIN2":        "GAIN3k. Scale 1mA",
    "GAIN3":        "GAIN30k. Scale 100uA",
    "GAIN4":        "GAIN300k. Scale 10uA",
    "GAIN5":        "GAIN3M. Scale 1uA",
    "GAIN6":        "GAIN30M. Scale 100nA",
    "GAIN7":        "GAIN100M. Scale 40nA"

}

COMANDOS_UART = {
    "GET_ID":       "?ID\n",
    "GET_STATE":    "?STATE\n",
    "GET_STATUS":   "?STATUS\n",
    "PREPARE":      "READY_UP\n",
    "START":        "START_EXP\n",
    "STOP":         "ABORT\n",           # Prioritario
    "RESET":        "RST\n",
    "ELEC_STATUS":  "E_STATUS\n",
    "SET_GAIN":     "SET_GAIN:"
    #"ENGAGE_CELL": "CELL1\n",   # Conectar electrodos
    #"DISENGAGE_CELL": "CELL0\n", # Desconectar electrodos
}

PAQUETES_CONFIG = {
    "actionSWV": {
        "header": "CONF_SWV",
        "params": [
            # Posición 1 a 6: SWV Core
            "swv_initial_pot", "swv_final_pot", "swv_pulse_freq", 
            "swv_pulse_amplitude", "swv_step_pot", "swv_quiet_t",
            # Posición 7: FLAG ACTIVACIÓN
            "precond_on_off", 
            # Posición 8 a 11: Parámetros Precond
            "stir_on_off", "deposition_pot", "deposition_time", "precond_quiet_t"
        ]
    },
    "actionLSV/CV": {
        "header": "CONF_LSV",
        "params": [
            "lsv_initial_pot", "lsv_switch_pot1", "lsv_switch_pot2", 
            "lsv_final_pot","lsv_segments", "lsv_scan_rate", "lsv_quiet_time"
            #,
            # #"params_precond": None # La CV podría no tenerlo o tener otros
        ]
    }

}