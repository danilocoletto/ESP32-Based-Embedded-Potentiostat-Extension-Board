# ESP32-Based Real-Time Embedded Potentiostat Extension Board

![Status](https://img.shields.io/badge/Status-Work_in_Progress-yellow)
![Platform](https://img.shields.io/badge/Platform-ESP32-blue)
![License](https://img.shields.io/badge/License-Open_Source-green)

> **⚠️ PROJECT STATUS: ONGOING**
> This project is currently under active development. The hardware design and firmware are subject to changes. Please check the `dev` branch for the latest updates.

## Key Features

- Real-time embedded architecture using **ESP32 + FreeRTOS**
- **24-bit precision ADC (ADS1255)** for nanoampere current measurements
- **16-bit DAC waveform generation** for electrochemical techniques
- Modular **analog front-end (AFE)** design
- Designed for **voltammetry and amperometry**
- **KiCad open hardware design**
- Firmware based on **ESP-IDF**


## 📖 Introduction

This repository contains the open-source hardware and firmware for a **Real-Time Embedded Potentiostat**. Designed as a high-precision **modular extension board**, this instrument allows for advanced electrochemical analysis (Voltammetry, Amperometry) in field applications.

A defining feature of this design is its **controller-agnostic modularity**. The board functions as a dedicated analog "backpack," decoupling the sensitive electrochemical front-end from the digital control logic. This architecture allows the board to be driven by various devices depending on the application needs:

* **ESP32 Implementation (Current Focus):** The primary implementation utilizes an **ESP32** microcontroller. This was chosen to provide **hard real-time execution** via FreeRTOS and to enable **wireless connectivity** (Wi-Fi/Bluetooth) for remote field monitoring in the future development.
* **External Instrumentation:** The header layout and signal routing were specifically designed to allow direct interfacing with test equipment such as the **Analog Discovery 2** (or similar oscilloscopes/logic analyzers). This allows user flexibility to to have alternative control strategies such as an ESP32, ATMEGA, STM32 or even an Analog Discovery.

Electrochemical Cell
      │
      ▼
Analog Front-End
(TIA + Control Amplifier)
      │
      ▼
ADS1255 24-bit ADC
      │
      ▼
ESP32 (FreeRTOS)
 ├─ DAC Control
 ├─ Data Acquisition
 └─ WiFi / Bluetooth
      │
      ▼
Host Interface / Data Logging

### 💡 The Evolution: From Single-Board Computer to Real-Time Embedded
This project represents the **second generation** of the "All-in-One" potentiostat concept.

* **Previous Design (Generation 1):** Based on a **Raspberry Pi 3B+**. While it successfully integrated the user interface and control logic, the reliance on a non-real-time Operating System (Linux) introduced latency challenges for high-speed techniques. The usage of the ADS1110 (240 SPS) also limited the sweep speed.
* **Current Design (Generation 2):** Transitions to an **Embedded Real-Time Architecture**.
    * **Why?** To achieve **deterministic timing** and modular flexibility.
    * **Improvements:** Modular hardware design suitable for extension, significantly lower power consumption, reduced noise floor (nanoampere-level), and the ability to perform fast electrochemical screening without a heavy OS overhead.

## 🛠️ Hardware & Components

The hardware is designed to support nanoampere-level current measurements with a robust analog front-end.

* **MCU:** **ESP32-WROOM/WROVER** (Dual-core, 240 MHz). Handles waveform generation (DAC control), data acquisition (ADC readings), and wireless communication.
* **Analog Front-End (AFE):**
    * **Potentiostat Circuit:** Improved topology using **High-Precision Rail-to-Rail OpAmps** (e.g., LMP7702/MAX series) for the Transimpedance Amplifier (TIA) and Control Amplifier.
    * **DAC:** 16-bit Low-Power DAC with I2C/SPI interface for precise potential steps.
    * **ADC:** Upgraded High-Speed ADC (replacing the slower ADS1110 of the previous version for an ADS1255) to support faster sweep rates (>1000 mV/s) and filter line noise at low current measurements.
    * **Anti-Aliasing:** Active 4th-order Sallen-Key filtering to smooth signal steps and reduce noise.
* **Power Management:** On-board LDOs and precision voltage references (2.500V / 5.000V) to ensure stable operation in field conditions.
## 📂 Repository Structure

The repository is organized as follows:

```
├── Diseno_3D/   # Fusion360 project and .stl files for 3D case design
├── Hardware/    # KiCad project files (Schematics, PCB Layout, BOM)
├── Software/ 
│   ├── ESP32_Firmware/ 
│   │   ├── PotentiostatV2_Firmware/    # ESP-IDF C/C++ source code
│   │   │   ├── main/   # Core logic (FreeRTOS tasks, State Machine)
│   │   │   ├── components/     # Drivers for DAC, ADC, and Potentiostat control
│   │   ├── Main/               # GUI and Python Backend modules
│   │   │   ├── modules/        # Python backend modules
│   │   │   ├── logos/          # logos folder
│   │   │   ├── gui/            # Graphic User Interface .ui file
├── Información/   # Datasheets and design notes
└── README.md   # This file
```

## 🔌 Pinout & Interface

*> **Note:** As the PCB layout is being finalized, please refer to the schematic in the `Hardware` folder for the most up-to-date netlist. Below is the current configuration for the ESP32 implementation.*

### ⚡ Analog Front-End Connections
* **WE (Working Electrode):** Input for current measurement.
* **RE (Reference Electrode):** Feedback input (Shielded).
* **CE (Counter Electrode):** Current injection output.

### 🧩 ESP32 GPIO Mapping

| Component | Function | ESP32 GPIO | Description |
| :--- | :--- | :--- | :--- |
| **ADS1255 (ADC)** | **MISO** (DOUT) | **19** | SPI Data Input |
| | **MOSI** (DIN) | **23** | SPI Data Output |
| | **SCLK** | **18** | SPI Clock |
| | **CS** | **5** | Chip Select |
| | **DRDY** | **4** | Data Ready Interrupt |
| | **CLOCK** | **16** | (Optional) ADS1255 System Clock |
| **MAX5217 (DAC)** | **SDA** | **21** | I2C Data |
| | **SCL** | **22** | I2C Clock |
| | **NAUX** | **32** | Aux/Load Control |
| **MAX4617 (Mux)** | **SEL A** | **27** | Range/Gain Select Bit A |
| | **SEL B** | **26** | Range/Gain Select Bit B |
| | **SEL C** | **25** | Range/Gain Select Bit C |
| **MAX4737 (Switches)**| **CR SHORT EN**| **14** | Counter-Reference Short Enable |
| | **RE FB EN** | **33** | Reference Feedback Enable |
| | **WE EN** | **13** | Working Electrode Enable |
| | **CE EN** | **15** | Counter Electrode Enable |
| **Misc** | **GREEN LED** | **2** | Onboard Status LED (Green) |

## 📚 References & Context

For context on the theoretical background and the first generation of this device, please refer to our published article on the Raspberry Pi-based version:

> **(All-in-One) Open Source Potentiostat for Field Analysis Based on Raspberry Pi**
> *Hardware* 2024, 3(4), 17; [https://doi.org/10.3390/hardware3040017](https://www.mdpi.com/2813-6640/3/4/17)

*3D Printed Design by [Boris Diaduch](https://www.linkedin.com/in/borisdiaduch/)*
---
*Maintained by [Ing. Danilo Coletto Gallego](www.linkedin.com/in/danilocoletto)*
