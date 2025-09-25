# Minor Smart Things — Weather Station Design Specification

## Overview

This document specifies the functional requirements, sensing approaches, mechanical guidance, electronics and firmware interface for the "Minor Smart Things" weather station.

Measured quantities:
- Temperature (°C) — DHT22
- Relative humidity (%RH) — DHT22
- Wind speed (m/s) — 3-cup anemometer with Hall-effect pulse detection
- Wind direction (°) — Gray-code optical disk read by phototransistors (preferred) or dual analog Hall sin/cos alternative
- Light intensity (lux) — BH1750 / TSL2561 digital lux sensor (I2C)
- Sunshade actuator position — hobby PWM servo (degrees 0–180)

## Functional requirements and targets

- Temperature: range -20 … +60 °C, target accuracy ±0.5 °C
- Humidity: range 0 … 100 %RH, target accuracy ±2–5 %RH
- Wind speed: 0 … 30 m/s, target accuracy ±0.5 m/s (after calibration)
- Wind direction: 0 … 359°, target accuracy ±10° (after calibration)
- Light intensity (lux): 0 … 120000 lux, target accuracy ±10–20% (sensor dependent)
- Sunshade actuator: position 0–180° (reporting as 0–100% optional), positional repeatability ±5° (servo-dependent)

## Sensing approaches and tradeoffs

### Wind speed — 3-cup anemometer (recommended)
- Mechanically simple, robust, low friction when well designed.
- Mount 1–2 neodymium magnets in the rotor hub and detect each pass using a digital Hall-effect switch (e.g., A3144, SS441A). Use 2 magnets for increased resolution (pulses_per_rev = 2).
- Pros: digital pulse, easy counting with interrupts, low susceptibility to contamination.
- Cons: mechanical wear, requires bearings and water protection, must calibrate k (m per revolution).

### Wind direction — Gray-code optical disk (preferred)
- Use a 6-bit Gray-code disk (64 positions ≈ 5.625° resolution) printed on a thin disk (30–50 mm diameter).
- Read with 6 phototransistors/photodiodes positioned in a fixed sensor array; use external 10 kΩ pull-ups to 3.3 V for each phototransistor.
- Pros: absolute reading on power-up, simple mapping to degrees, robust to interruptions.
- Cons: requires multiple sensors and careful masking and light shielding.

### Light intensity (lux) — digital I2C lux sensor (recommended)
- Recommended sensors: BH1750 (0x23 default) or TSL2561 (I2C, selectable address). BH1750 is simple and returns lux directly; TSL2561 can provide different gains and integration times for low-light scenarios.
- Pros: digital, factory-calibrated to lux units, easy I2C integration with ESP32, no analog front-end required.
- Cons: field-of-view and spectral response differ from human perception; mounting and shading must be considered.
- Mount sensor in a small weatherproof window with unobstructed sky view and minimal shading from the station itself.

### Sunshade actuator — hobby PWM servo (recommended)
- Use a hobby PWM servo (e.g., SG90 or MG90S for higher torque) for simple up/down motion of the shade.
- Pros: simple control via PWM, built-in positional feedback (inside servo), easy to integrate.
- Cons: torque limited; for larger shades use a larger servo, geared motor, or linear actuator.
- Power: servo requires a stable 5 V supply capable of 1–2 A peaks depending on servo. DO NOT power servo from ESP32 3.3 V regulator.

## Mechanical design guidance (parametric)

(Keep anemometer/vane guidance as in original spec.)
- Cup anemometer defaults: cup_diameter 30–60 mm (default 40 mm), arm_length 40–60 mm (default 50 mm) etc.
- Vane encoder disk: 30–50 mm diameter, Gray-code 6 bits recommended.
- Sunshade mechanical: include a servo mount with 3 mounting holes spaced (pattern recommended), allow servo horn clearance and adjust for mechanical advantage; design shade linkage with 1.5–3x safety factor on torque.

## Environmental & enclosure notes
- Print moving parts in PETG or ASA for outdoor durability.
- Mount lux sensor behind a clear, UV-stable window (PMMA) or use a small dome; keep window clean.
- Route shade servo cable through a weatherproof gland and use sealed connectors where possible.

## Electronics & circuits (component-level guidance)
- Logic domain: 3.3 V for ESP32 I/O.
- Decoupling: 0.1 µF ceramic close to each IC Vcc pin and a 100 µF bulk cap on the 5 V servo rail.

### Hall sensor (wind speed)
- Use digital Hall switch (A3144 / SS441A). Vcc → 3.3 V, GND → GND, OUT → GPIO with 10 kΩ pull-up.
- Add 0.1 µF decoupling.

### Gray-code phototransistors (wind direction)
- Phototransistor outputs → ESP32 input pins with external 10 kΩ pull-ups to 3.3 V. Add 0.1 µF RC for anti-chatter per line.

### Light sensor (BH1750 / TSL2561)
- BH1750 VCC → 3.3 V (or 5 V if module requires; prefer 3.3 V to match ESP32 I/O), GND → GND, SDA → GPIO21, SCL → GPIO22.
- Default I2C address BH1750 = 0x23 (ADDR pin high changes address to 0x5C depending on board).
- Use 2 × 4.7 kΩ pull-ups to 3.3 V on SDA and SCL if the breakout does not include pull-ups.
- Configure measurement time and resolution in firmware as needed (e.g., 1 s or continuous mode).

### Servo control (sunshade)
- Servo VCC → 5 V (separate stable supply), GND → common GND with ESP32.
- Servo control (PWM) → ESP32 GPIO5 (or other PWM-capable GPIO). Use a small series resistor (100 Ω) on the signal line and an optional 10 kΩ pulldown to avoid floating during boot.
- Add a 100–470 µF electrolytic capacitor on the servo 5 V rail to smooth transients.
- Avoid powering the servo from the ESP32 regulator; use a dedicated 5 V supply (USB power bank or 5 V switching regulator).

### DHT22
- Vcc → 3.3 V. DATA → GPIO16 with 4.7–10 kΩ pull-up. Add 0.1 µF + 10 µF decoupling on Vcc.

### Power
- Recommended input: 5 V supply (USB or regulated). Use a 3.3 V regulator (LDO or buck converter) for ESP32 if necessary.
- 5 V rail for servo: capable of 2 A continuous headroom for servo peaks (1–2 A).

## ESP32 pin assignments (firmware_config)

| Pin (GPIO) | Purpose | Notes |
|---:|---|---|
| 21 | I2C SDA | BH1750 SDA (I2C) |
| 22 | I2C SCL | BH1750 SCL (I2C) |
| 16 | DHT22 DATA | 4.7–10 kΩ pull-up |
| 4 | Wind-speed Hall input | attachInterrupt-capable; 10 kΩ pull-up |
| 32 | Dir bit 0 (Gray-code) | external 10 kΩ pull-up |
| 33 | Dir bit 1 | " |
| 34 | Dir bit 2 (input-only) | " |
| 35 | Dir bit 3 (input-only) | " |
| 36 | Dir bit 4 (input-only) | " |
| 39 | Dir bit 5 (input-only) | " |
| 5 | Sunshade servo PWM | PWM control (signal) — servo Vcc must be 5 V |
| 2 | Status LED | 220 Ω series resistor |

## Measurement and signal processing

### Wind speed
- pulses_per_rev: default 2.
- revs_per_sec = pulse_rate_hz / pulses_per_rev.
- wind_mps = k_speed * revs_per_sec (k_speed in m/rev). Placeholder k_speed = 2.2 m/rev.
- Handle debounce and timeout for calm detection.

### Wind direction
- Read Gray-code bits, convert to binary and map to degrees: deg = code * (360 / 2^N).
- Smooth with circular averaging over recent M samples.

### Light intensity & shade control
- Read lux from BH1750 (or TSL2561) every 1–5 s (depending on responsiveness).
- Control logic (suggested):
  1. If wind_mps > wind_retract_threshold (default 10 m/s) → move shade to OPEN position (protect shade).
  2. Else if lux > lux_close_threshold (default 40000 lux) and temperature > temp_close_threshold (optional, default 25 °C) → move shade to CLOSED position.
  3. Else if lux < lux_open_threshold (default 30000 lux) → move shade to OPEN position.
  4. Apply hysteresis and minimum dwell time (e.g., 60 seconds) to avoid rapid oscillation.
- Shade position control: map target position to servo angle (0–180°). Store open/closed angle limits in NVS.

## Calibration summary
- Store calibration and control constants in NVS/LittleFS JSON:
{
"k_speed": 2.2,
"pulses_per_rev": 2,
"dir_offset_deg": 0,
"lux_close_threshold": 40000,
"lux_open_threshold": 30000,
"wind_retract_threshold": 10.0,
"temp_close_threshold": 25.0,
"shade_open_angle": 0,
"shade_closed_angle": 90
}

## JSON payload schema
- Example payload sent to server:
{
"ts": 1670000000,
"temp_c": 12.34,
"rh_pct": 56.7,
"wind_mps": 3.21,
"wind_deg": 225.0,
"lux": 54321,
"shade_angle": 90
}

## Acceptance criteria and test plan (summary)
- Wind speed: within ±0.5 m/s at 3 m/s and 10 m/s after calibration.
- Wind direction: within ±10° after calibration.
- Lux: BH1750 nominal accuracy adequate; verify values vs handheld lux meter; ensure shade logic responds appropriately.
- Shade control: repeatable movement to open/closed positions within ±5°.

## Next steps for Code and CAD teams
- Firmware: implement BH1750 driver (I2C), DHT22 driver, Hall pulse counting, Gray-code reader, servo control using LEDC channel, calibration storage, and JSON publish logic with shade automation rules.
- CAD: add servo mount and cable gland features in [`design/3d_requirements.md`](design/3d_requirements.md:1).

End of document.