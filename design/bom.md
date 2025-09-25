# Bill of Materials — Minor Smart Things Weather Station

Version: 1.1  
Date: 2025-09-05

Reference: [`design/spec.md`](design/spec.md:1), [`design/wiring.md`](design/wiring.md:1)

Overview
This BOM lists recommended components, approximate quantities, example part names and ballpark costs (USD). Prices are estimates and will vary by supplier and country.

Summary cost estimate
- Low-end build (approx): $60 – $120
- Recommended build (good quality servo, enclosure): $100 – $220

Core electronics
- ESP32 dev board (ESP32-WROOM-32 DevKitC or similar) — qty 1 — Example: DOIT/ESP32 DevKitC — Estimated cost: $6–12
- DHT22 / AM2302 temperature & humidity sensor — qty 1 — Example: DHT22/AM2302 — $5–12
- Digital lux sensor (BH1750) — qty 1 — Example: BH1750 breakout — $2–8
- ADS1115 16-bit I2C ADC (optional) — qty 0–1 — Only if adding analog sensors later — $5–10
- Servo (hobby PWM) — qty 1 — Example: SG90 (plastic) or MG90S (metal gears) — $2–10
- 5 V power supply (capable of 2 A peak) — qty 1 — Example: USB power bank or 5 V switching regulator — $5–15
- LDO / 3.3 V regulator (for ESP32 and 3.3 V sensors) — qty 1 — Examples: TLV70033, MCP1700 — $0.5–2
- Logic-level N-MOSFET (if using for other actuator control) — qty 1 — Example: IRLML2502 — $0.2–1
- Resistors & capacitors assortment (kit) — qty 1 kit — $5–12
- Protoboard or small custom PCB — qty 1 — $2–8
- Connectors (JST-SM, JST XH) and wire housings — qty assorted — $3–8

Sensors & input components
- Hall-effect sensor (digital) A3144 / SS441A — qty 2 — $0.1–0.6 each
- Phototransistors (for Gray-code disk) — qty 6–8 — examples: generic NPN phototransistors — $0.1–0.5 each
- Neodymium disc magnets (5×2 mm or 6×2 mm, N42/N52) — qty 2–4 — $0.1–0.6 each

Mechanical & mounting
- Bearings: choose per shaft size; examples:
  - 5×11×5 mm bearings (if using 5 mm shaft) — qty 2 — $1–4 each
  - 608ZZ bearing (8×22×7 mm) if using 8 mm shaft — qty 2 — $1–4 each
- Stainless steel shaft (5 mm Ø × 100–150 mm) — qty 1 — $2–6
- Screws / nuts / washers assortment (M2, M3, M4) — qty assorted — $3–8
- 3D printing filament (PETG recommended) — qty 250–500 g — $10–25
- Enclosure (IP54 small box) or custom printed housing — qty 1 — $5–25
- GORE vent / breathable membrane — qty 1–2 — $2–6

Servo & sunshade hardware
- Servo mounting hardware (horns, screws) — qty 1 kit — $0.5–2
- Shade linkage parts (printed or small rods/hinges) — qty assorted — $3–10
- Cable gland / weatherproof gland for servo cable — qty 1 — $1–4

Miscellaneous and consumables
- Heat-shrink tubing, wire (22–28 AWG) — qty assorted — $3–8
- Epoxy / superglue for magnets and assembly — $2–6
- Solder, soldering accessories — $5–20 (if needed)
- Labeling, tape, black matte paint for encoder disk masking — $3–8

Optional / removed components (not required for current plan)
- PM optical scattering TIA components (op amps, photodiodes, sampling fan) — REMOVED — not required
- Solar thermopile or TIA front-end — REMOVED — replaced by BH1750 digital lux sensor
- ADS1115 and op-amps (only needed if re-adding analog sensors) — OPTIONAL

Optional upgrades
- Higher-torque servo or hobby-grade actuator for larger shades — $10–40
- Metal-geared servo (MG90S) for improved durability — $6–15
- Higher-precision bearings and sealed hubs — add $5–20

Tools & test equipment (recommended)
- Multimeter — $10–60
- Soldering iron + solder — $20–80
- Small drill/rotary tool (for post-processing prints) — $30–100
- Calibrated reference instruments (optional): handheld anemometer, handheld lux meter — varies

Suppliers & notes
- Fast, reliable suppliers: DigiKey, Mouser, RS Components, Farnell (Europe); Adafruit / SparkFun (modules); Amazon / eBay / AliExpress for low-cost parts.
- For communication modules and sensors prefer official breakout modules (BH1750 breakout with level-shifting if needed). For servos, buy from reputable sellers to avoid counterfeit low-quality units.

Estimated subtotal (components only)
- Budget build: ≈ $60–120
- Recommended build: ≈ $100–220

Packaging and PCB suggestions
- Consider making a small PCB for power distribution, servo connector and I2C pull-ups. Keep analog and digital sections separate.
- Keep wires for servo and power physically separated from sensitive sensor wiring.

End of BOM