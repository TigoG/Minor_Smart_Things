# 3D Requirements — Minor Smart Things Weather Station

This document defines parametric CAD requirements, critical dimensions, tolerances and assembly notes for the 3D-printed parts used by the weather station. It complements [`design/spec.md`](design/spec.md:1) and [`design/wiring.md`](design/wiring.md:1).

Goals
- Provide printable, parametric parts for the anemometer rotor (cups + hub), vane + encoder disk, bearing / shaft housing and sensor mounts.
- Ensure parts are printable on common FDM printers and tolerant to outdoor use (PETG / ASA).

Deliverables for CAD subtask
- Parametric source (OpenSCAD or Fusion/FreeCAD) with named parameters described below.
- Exported STL files: `anemometer_cup.stl`, `anemometer_hub.stl`, `anemometer_rotor_assembly.stl`, `vane.stl`, `encoder_disk.stl`, `sensor_mounts.stl`.

Naming convention and parameters
- Use metric names and mm units in parameters.
- Primary parameters (defaults shown):
  - cup_diameter_mm: 40
  - arm_length_mm: 50
  - hub_outer_diameter_mm: 20
  - shaft_diameter_mm: 5
  - bearing_inner_dia_mm: 5
  - bearing_outer_dia_mm: 11
  - bearing_width_mm: 5
  - encoder_disk_diameter_mm: 40
  - encoder_disk_thickness_mm: 1.6
  - encoder_bits: 6
  - bit_ring_width_mm: 3.0
  - bit_slot_min_width_mm: 1.6
  - magnet_diameter_mm: 5
  - magnet_thickness_mm: 2
  - magnet_pocket_diam_mm: 5.2
  - magnet_pocket_depth_mm: 2.2
  - magnet_gap_mm: 1.5

Detailed part requirements

1) Anemometer cup
- Shape: hemispherical or semi-conical cup; inner hollow optional to reduce mass.
- Wall thickness: 1.2–2.0 mm (recommend 1.2 mm for PETG with 0.4 mm nozzle).
- Mounting boss: incorporate M2 boss or 2 mm hole for M2 screw to attach cup to arm; boss height 3–4 mm.
- Fillets: 1 mm fillet at cup-arm junction.
- Print orientation: cup opening downwards or angled to reduce supports; use tree supports if necessary.

2) Anemometer arms and hub
- Arms: 3 arms equally spaced, simple flat arms with chamfered trailing edge to reduce turbulence.
- Arm cross-section: 3–4 mm thick and 8–12 mm wide recommended.
- Hub: machined/printed hub with bearing seat(s) for two bearings in stack or opposing faces.
- Bearing seat diameter: match bearing_outer_dia_mm parameter; for press-fit use seat_diam = bearing_outer_dia_mm - 0.05 mm for interference fit or +0.1 mm for slip fit depending on printer calibration.
- Bearing shoulder depth: match bearing width (bearing_width_mm) with +/- 0.2 mm allowance.
- Shaft clearance: bore should match shaft_diameter_mm + 0.05 mm for a slip fit, or +0.2 mm if using sleeve bearings.
- Magnet pockets: provide location(s) in the rotor hub: pocket_diam = magnet_pocket_diam_mm, pocket_depth = magnet_pocket_depth_mm, with small locating chamfer. Provide both glue-pocket and press-fit alternative geometry (selectable by a parameter).
- Ensure rotor mass near radius minimized: arms and cups thin-walled.

3) Encoder disk (Gray-code)
- Disk diameter: encoder_disk_diameter_mm (default 40 mm).
- Thickness: encoder_disk_thickness_mm (1.6 mm recommended).
- Rings: create concentric rings for each bit. Use bit_ring_width_mm for ring radial thickness (default 3.0 mm). Inner radius = hub clearance (e.g., 6 mm) + 0.5 * total_ring_width.
- Slots/pattern: implement a transmissive OR reflective pattern:
  - Transmissive (preferred): slots cut through disk where bits = 1; place matching IR LED and phototransistor pairs on opposite sides. Slot minimum arc width at sensor radius must be >= bit_slot_min_width_mm (default 1.6 mm).
  - Reflective (alternate): disk surface has high-reflectance (white) and low-reflectance (matte black) regions; ensure surface finishes defined below.
- Bit angular resolution: each sector = 360 / (2^encoder_bits). For 6 bits -> sector = 5.625°.
- Provide registration holes for mounting the disk to the vane shaft; center hole diameter match shaft_diameter_mm with +0.05 mm clearance for slip fit.
- Masking: include small recessed ledges to accept a thin black tape or paint to create high/low reflectance areas if using reflective method.

4) Vane body
- Vane pivot interface: provide robust boss to mount on bearing or friction pin; include flat on shaft for set-screw or grub screw.
- Vane length: parameterizable; default 100 mm from pivot to tail.
- Tail area: ensure enough moment to align vane into wind; tail area approximate 2–3 times the head area.
- Cable routing: internal channel for wiring through the pivot to minimize wear; include strain-relief boss and grommet slot.

5) Sensor mounts and shrouds
- Phototransistor mounts: adjustable slot allowing +/- 0.5 mm radial and +/- 1.0 mm axial adjustment; provide M2 screw slots for fine positioning.
- Hall sensor mount: small flat surface on body with locating feature and 2 mm hole for mounting; sensor face must be aligned to be 1–3 mm from magnet pocket when rotor installed.
- TIA / photodiode window: for PM and solar sensors, provide small recessed window pockets for optical domes/diffusers; ensure outer surface flush with printed part and provide recess for clear PMMA piece or dome.
- Light baffle: include small shroud around phototransistor/photodiode to reduce stray light and improve SNR; length 6–10 mm.

Materials and print settings
- Material: PETG recommended for outdoor use; ASA recommended for best UV resistance.
- Layer height: 0.15–0.25 mm (0.2 mm typical).
- Nozzle: 0.4 mm typical.
- Wall/Perimeter: 3 perimeters (1.2 mm total wall thickness with 0.4 mm nozzle).
- Infill: 15–30% for structural parts; 5–10% for cups if hollow to reduce mass.
- Supports: minimal supports for overhangs; orient parts to reduce supports especially for cups.
- Post-processing: sand mating faces, clean cavities, apply black matte spray to encoder disk if using reflective detection; optionally coat parts with UV-protective spray.

Fits, tolerances and adjustments
- General tolerance guidance: design with ±0.2 mm tolerance for press-fit features. For slip-fit holes use +0.05 to +0.2 mm depending on fit.
- Bearing press-fit: If press-fitting bearing, initial test hole diam = bearing_outer_dia_mm - 0.05 mm (tight fit); if slip-fit, hole diam = bearing_outer_dia_mm + 0.1 mm (loose).
- Shaft slip-fit: hole diam = shaft_diameter_mm + 0.05 mm.
- Magnet pocket: pocket_diam_mm +0.1 mm for adhesive fit; pocket depth = magnet_thickness_mm - 0.1 mm to allow slight countersink and glue fillet.
- Phototransistor clearance: sensor face to disk ≈ 3–5 mm; design adjustment slots for fine alignment.

Optical disk specifics (Gray-code generation)
- Generate Gray-code bit patterns for N bits using standard Gray-code sequence (binary-to-Gray conversion). Each disk sector holds N concentric bit values read radially.
- Map Gray→binary in firmware using algorithm or lookup table. See [`design/spec.md`](design/spec.md:1) for mapping formula.
- Example: for N=6, create 64 sectors, each sector spans 5.625°. Ensure slot angular width accounts for sensor angular misalignment; include ±1° margin if possible.
- File export: provide both "solid" disk and "through-slotted" transmissive version for the CAD/SCAD author to evaluate.

Assembly & mounting notes
- Use M2 screws and nuts for small bosses; provide countersunk holes where flush assembly is required.
- Use cyanoacrylate (superglue) or epoxy to secure magnets in glue pockets; allow for 24h cure for epoxy.
- Consider using a stainless-steel shaft with small retaining rings or grub screws to prevent axial play.
- Add small drainage holes in hub center and cup bottoms (Ø 2–3 mm).

Validation prints and check-list
- Print a single cup to verify wall thickness and fit (dimension check for cup_diameter_mm).
- Print a bearing seat test piece: check bearing outer diam fit and adjust seat_diam until snug.
- Print encoder disk test pattern at full scale to verify slot width and optical detection works with chosen sensors (mask and paint test if reflective).
- Verify magnet pocket fit with actual magnets before final assembly.

Deliverable file naming & export
- Source parametric file: `anemometer_params.scad` or `anemometer_params.fcstd`
- STL exports (example): `anemometer_cup.stl`, `anemometer_hub.stl`, `anemometer_rotor.stl`, `vane.stl`, `encoder_disk_transmissive.stl`, `encoder_disk_reflective.stl`, `sensor_mounts.stl`
- Include a small README in CAD folder describing parameter defaults and how to change them.

Notes and trade-offs
- Larger encoder disks increase angular arc width of bits and ease optical detection but increase moment of inertia — balance disk size against vane torque.
- Press-fit bearings provide secure, repeatable alignment but require tighter print tolerances or light reaming; slip-fit with set-screws easier to assemble in makerspaces.
- Choose lightweight cups and arms to improve low-speed responsiveness.

References
- Mechanical guidance and sensor mapping in [`design/spec.md`](design/spec.md:1)
- Wiring and electrical considerations: [`design/wiring.md`](design/wiring.md:1)

End of 3D requirements document.