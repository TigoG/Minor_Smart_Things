# Minor Smart Things

Repository for two embedded PlatformIO projects: a shade actuator and a weather station used for the Minor Smart Things assignment.

Quick summary

- Actuator: controls a shade motor and exposes simple commands. Code and PlatformIO project are in [`Actuator/`](Actuator/:1) — see [`Actuator/include/README`](Actuator/include/README:1) for wiring, build, and command reference.
- Weather station: reads sensors and displays values, and sends data over ESP-NOW. Code is in [`weatherStation/`](weatherStation/:1) — see [`weatherStation/include/README`](weatherStation/include/README:1) for wiring and calibration.

Folder structure

- [`Actuator/`](Actuator/:1) — PlatformIO project for the shade actuator (source in [`Actuator/src/`](Actuator/src:1) and headers in [`Actuator/include/`](Actuator/include:1)).
- [`weatherStation/`](weatherStation/:1) — PlatformIO project for the weather station (source in [`weatherStation/src/`](weatherStation/src:1) and headers in [`weatherStation/include/`](weatherStation/include:1)).
- [`design/`](design/:1) — design documents, diagrams, BOM and calibration notes (e.g., [`design/description/`](design/description:1)).
- [`firmware/`](firmware/:1) — miscellaneous firmware sketches (e.g., [`firmware/weather_station.ino`](firmware/weather_station.ino:1)).

Quick start

Requirements:

- PlatformIO (VS Code extension or CLI) installed on your machine.

Build and upload (PlatformIO CLI)

- Build actuator:

  1. Open a terminal at the repository root and run:
     - `cd Actuator && pio run`
     - To upload: `cd Actuator && pio run -t upload`

- Build weather station:

  1. Open a terminal at the repository root and run:
     - `cd weatherStation && pio run`
     - To upload: `cd weatherStation && pio run -t upload`

Serial monitor:

- Use `pio device monitor -p <port>` or `pio run -t monitor` inside the project folder.

Where to find wiring and configuration

- Actuator wiring and notes: [`Actuator/include/README`](Actuator/include/README:1) and [`Actuator/src/macAddress.txt`](Actuator/src/macAddress.txt:1).
- Weather station wiring and calibration: [`weatherStation/include/README`](weatherStation/include/README:1) and [`design/description/kalibratiecurve.tex`](design/description/kalibratiecurve.tex:1).

Important files

- [`Actuator/src/main.cpp`](Actuator/src/main.cpp:1) — actuator entry point.
- [`Actuator/include/ShadeController.h`](Actuator/include/ShadeController.h:1) — shade controller interface.
- [`weatherStation/src/main.cpp`](weatherStation/src/main.cpp:1) — weather station entry point.
- [`weatherStation/include/SensorManager.h`](weatherStation/include/SensorManager.h:1) — sensor manager interface.

Calibration & testing

- See [`design/description/kalibratiecurve.tex`](design/description/kalibratiecurve.tex:1) and [`design/testing/test.tex`](design/testing/test.tex:1) for calibration details and test plans.