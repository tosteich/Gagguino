# Boiler thermal trace (RAM-only)

This is a read-only diagnostic. It does not modify heater PID, pump PID,
temperature setpoint, actuator scheduling, or the existing 60-shot archive.

## Captured on the existing 200 ms sensor tick

- 20 s before brew button activation (rolling in-memory buffer).
- Up to 120 s during an active shot.
- 60 s after brew button release.
- One completed trace retained until a new shot or an ESP32 restart.
- Data are **not persisted to flash**; no additional LittleFS writes.
- If a shot lasts more than 120 s, the first 120 s are retained; the 60 s
  post-roll is still collected.

Each record includes:
`time_s` (relative to first observed flow sample, negative in pre-roll),
`temperature_c`, `pressure_bar`, `heater_percent`,
`pump_percent`, `phase` (pre, brew or post), plus
`sensor_temp_precise_c` (0.01 C), `target_temp_c`, active
`active_p`, `active_i`, `active_d`, and the new algorithm's
`shadow_p`, `shadow_i`, `shadow_d`, `shadow_percent`.
Terms are exported with 0.1 precision and may be clipped outside the
16-bit storage range. `shadow_percent=-1` means unavailable.

### Active controller / opposite shadow

The selected boiler algorithm is controlled by
`GAGGUINO_EXPERIMENTAL_BOILER_PID` in `BenchTestConfig.h`.
This flag is **enabled** on `feature/dark-web-ui` as of 2026-11-27:

- `BoilerPid` drives boiler heat using the existing stored Kp/Ki/Kd.
- Original `GaggiaPid` is the non-actuating boiler shadow.
- Original `GaggiaPid` independently continues to control the pump.
- Comment out the flag and rebuild to restore legacy boiler control, while
  reversing the active/shadow assignments.

`active_p/i/d` and `heater_percent` describe the selected controller;
`shadow_p/i/d` and `shadow_percent` describe the opposite controller
receiving the same temperature measurements. Shadow output **never** drives
an actuator. It does not predict what boiler temperature the shadow
algorithm would produce in closed-loop operation.

The current boiler PID has a 1.5s low-pass derivative filter, D bounded
to +/-30 output points, integral bounded to 20 output points and a 5s
above-target integral decay time. The hardware and software thermal
cutoffs remain essential even after the selected trace showed improved
temperature stability.

Boiler PID settings, steam-mode transitions, and loss of qualified sensor
data reset historical PID state. This trace is for diagnostic observation
only and stores no extra data in LittleFS.

In the web UI: **System → Thermal diagnostics → Refresh / Download CSV**.

APIs:
- `GET /api/thermal-trace/status`: JSON state `idle | flow | post | ready`
  and captured sample count.
- `GET /api/thermal-trace.csv`: streamed CSV when ready; HTTP 409 until
  the 60 s post-roll completes.

The main loop takes the trace mutex with **zero timeout**, avoiding waits
for HTTP responses. HTTP readers snapshot a completed trace and release the
mutex before streaming. One sensor sample may be skipped during that brief
snapshot, but heating and pressure control are never delayed by it.

Native regression tests:
`bash tests/run_native_tests.sh`, including `thermal_trace_test.cpp`.

## Install

The firmware must be rebuilt and uploaded to ESP32 for the new APIs.
Update `data/index.html` and `data/js/app.js` in LittleFS using the
selective USB web-asset updater; do not overwrite the full partition if
you want to retain previous shot archive files. An updated Android app is
required for its native file picker to recognize thermal-trace CSV exports.

**Hardware safety:** this log is observational only. It does not substitute
for an independent boiler thermal cutoff, and it must not be used to justify
enabling experimental feed-forward heating without separate validation.
