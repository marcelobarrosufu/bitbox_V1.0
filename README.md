# Modular Data Logger based on ESP32-S3

## Overview

This project presents the development of a modular data logger based on the ESP32-S3, designed for reliable data acquisition, local storage, and network communication. The system is built with a layered firmware architecture to ensure stability, scalability, and ease of maintenance in long-term operation scenarios.

The platform is capable of handling continuous data streams, storing them efficiently in binary format, and providing communication interfaces for monitoring and integration with external systems.

---

## Features

* Modular firmware architecture
* Decoupled interrupt and main processing routines
* Efficient binary logging to SD card
* Network communication support
* Scalable design for new protocols and peripherals
* Designed for long-term continuous operation

---

## Project Structure

```
.
├── hardware/         # Schematics, PCB design, BOM
├── firmware/         # Embedded firmware (ESP32-S3)
├── software/         # Host tools (Python scripts, monitoring, etc.)
├── HOW_TO_BITBOX.md  # Documentation and diagrams
└── README.md
```

---

## Hardware

The hardware includes custom-designed boards for data acquisition and system integration. It supports multiple interfaces and is designed for robustness in embedded environments.

All hardware design files (schematics, PCB, and related assets) are available in the `/hardware` directory.

---

## Firmware

The firmware is developed for the ESP32-S3 and follows a layered architecture, separating hardware abstraction, data processing, and communication.

Key aspects:

* Interrupt-safe data acquisition
* Queue and buffer management
* SD card logging with structured binary frames
* Network stack integration

---

## Software Tools

Supporting tools were developed in Python to assist with:

* Command interface
* System monitoring
* Binary log decoding

These tools are available in the `/software` directory.

---

## Future Work

Future developments include the implementation of Over-The-Air (OTA) updates, including an OTA bridge for distributed firmware updates, and the expansion of protocol decoding to support additional physical interfaces such as I2C and SPI.

---

## License

* Firmware and software: MIT License
* Hardware design: CERN-OHL-P v2

---

## Disclaimer

This project is provided "as is", without warranty of any kind. The authors are not responsible for any misuse or damage caused by the use of this system.

---

## Author

Developed by Luiz Pedro Bittencourt Souza

---
