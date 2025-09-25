# Calibration & Test Plan — Minor Smart Things Weather Station

Version: 1.0
Date: 2025-09-04

Purpose

This document provides step-by-step calibration procedures, CSV logging formats, analysis instructions and acceptance tests for wind speed, wind direction, particulate matter (PM) and solar irradiance sensors.

Required equipment

- Assembled weather station with ESP32, ADS1115 and sensors connected.
- Reference instruments: handheld anemometer or calibrated wind source, reference pyranometer (for solar), reference PM sensor (optional).
- Multimeter, PC for logging, compass/GPS heading tool.

Recommended logging CSV format (one row per sample)

timestamp_iso,temp_c,rh_pct,pulses,window_s,pulse_hz,revs_per_s,wind_ref_mps,wind_calc_mps,dir_raw_code,dir_deg_ref,dir_deg_calc,pm_adc,pm_ref_ugm3,solar_adc,solar_ref_w_m2

General notes

- Clean optical windows before each run. Record ambient temperature and humidity. Use stable mounting (tripod/mast).
- For PM and solar, record dark/baseline values (LED off or covered) to subtract background signal.

1) Wind speed calibration (derive k_speed)

Objective: find k_speed (m/rev) such that wind_mps = k_speed * revs_per_sec.

Setup:

- Confirm pulses_per_rev (default 2) in firmware matches installed magnets.
- Use a fixed logging window (window_s), recommend 10 s per sample.

Procedure:

1) Place DIY anemometer adjacent (<30 cm) to reference anemometer, on same horizontal plane.
2) For each reference wind speed (covering 0.5–10 m/s typical), record pulses over window_s and the reference wind reading. Collect 5–10 samples per speed if possible.
3) Save CSV with revs_per_s = (pulses/window_s)/pulses_per_rev and wind_ref_mps.

Analysis:

- Fit linear model wind_ref = k * revs + b. Use least-squares (e.g., numpy.polyfit(revs, wind_ref, 1)).
- If intercept b is negligible, use k = sum(revs_i * wind_i) / sum(revs_i^2).

Example:

revs = [0.2,0.5,1.0,2.0]; wind_ref = [0.44,1.1,2.2,4.4] => k ≈ 2.2 m/rev

Low-speed behavior:

- Determine minimum reliable rev rate revs_min where pulses are detectable; set wind=0 below this threshold.

Store constants:

JSON example to store in NVS or LittleFS:

{"k_speed":2.2,"pulses_per_rev":2,"wind_min_revs_per_s":0.05}

Acceptance:

- Error ≤ 0.5 m/s at 3 m/s and 10 m/s testing points.

2) Wind direction calibration (Gray-code)

Objective: compute dir_offset_deg mapping encoder code to absolute heading.

Setup:

- Level the station and align vane to a known heading (use compass or GPS).

Procedure:

1) Read Gray-code bits -> convert Gray→binary -> code_index (0..2^N-1).
2) raw_deg = code_index * (360 / 2^N).
3) dir_offset_deg = (ref_heading - raw_deg) normalized into [0,360).
4) Store dir_offset_deg in NVS.
5) Verify at 90°,180°,270°; record errors.

Smoothing:

- Use circular mean over a sample buffer (convert deg→unit vectors, average, atan2).

Alternative analog Hall method:

- Rotate vane through 360° and record two analog channels; compute offsets and gains; use atan2 for angle.

Acceptance:

- Direction error ≤ 10° at test headings.

3) PM sensor calibration (relative to reference)

Objective: map TIA output (V or ADC) to µg/m³.

Setup:

- Run PM sampling fan; ensure sensor and reference sample same air.
- Measure dark baseline with LED off to obtain V_dark.

Procedure:

1) Collect V_signal averaged over windows (10–30 s) while simultaneously reading pm_ref_ugm3.
2) Compute V_corr = V_signal - V_dark.
3) Repeat across multiple concentration levels (background, incense, dust).
4) Save CSV rows (timestamp,V_signal,V_dark,V_corr,pm_ref).

Analysis:

- Fit linear model pm_ref = a * V_corr + b. Save a,b as pm_scale and pm_offset.
- If nonlinear, consider polynomial or log transform.

Storage example:

{"pm_scale":125.0,"pm_offset":0.0}

Notes:

- Optical scattering depends on particle size/composition; expect approximate conversion and periodic recalibration.
- Clean optics regularly; changed optics alter calibration.

Acceptance:

- Correlation R² ≥ 0.8 and error ≤ 20% vs reference in similar particle conditions.

4) Solar irradiance calibration

Objective: compute scale/offset to convert ADC->W/m².

Setup:

- Co-locate DIY sensor with reference pyranometer on same plane; choose clear-sky midday for best SNR.

Procedure:

1) Record V_tia or ADC and W_ref across multiple points (10–20+).
2) Subtract V_dark if necessary: V_corr = V - V_dark.
3) Fit W_ref = a * V_corr + b (or force b=0 if dark-subtracted).
4) Save solar_scale (a) and solar_offset (b).

Storage example:

{"solar_scale":1200.0,"solar_offset":0.0}

Acceptance:

- Target ±10–20% agreement with reference pyranometer under clear-sky conditions.

5) Combined validation and acceptance checklist

- Run a 30–60 minute validation log with all sensors on.
- Compare wind speed vs reference, direction vs compass/reference, PM and solar vs references.
- Compute percent errors, R², and record pass/fail per acceptance criteria above.

6) Data analysis recommendations

- Use Python (pandas, numpy, matplotlib) or Excel. Apply least-squares for linear fits and circular statistics for direction.
- Suggested analysis steps: compute revs_per_s, run np.polyfit(revs, wind_ref, 1), compute residuals and RMSE.

7) Firmware storage & interfaces

- Store calibration constants in NVS or LittleFS JSON. Example:

{"k_speed":2.2,"pulses_per_rev":2,"dir_offset_deg":0.0,"pm_scale":125.0,"pm_offset":0.0,"solar_scale":1200.0,"solar_offset":0.0}

- Provide a serial or HTTP endpoint to set and persist calibration constants (e.g., /calibrate).

8) Maintenance schedule

- Clean optical windows weekly (or as required).
- Check bearings and magnet seating every 3–6 months.
- Recalibrate PM and solar sensors every 6–12 months or after major maintenance.

9) Example CSV snippet for wind calibration (one sample row)

2025-09-04T10:00:00Z,12.5,45.3,22,10,2.2,1.1,2.4,2.42,17,90,92,12345,15.2,20480,800

10) Acceptance test summary

- Wind speed: error <= 0.5 m/s at 3 m/s and 10 m/s.
- Wind direction: error <= 10° at cardinal headings.
- PM: R² >= 0.8 and error <=20% after calibration.
- Solar: error <= 20% vs reference at midday.

End of calibration & test plan