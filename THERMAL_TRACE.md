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
`pump_percent`, `phase` (pre, brew or post).

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
