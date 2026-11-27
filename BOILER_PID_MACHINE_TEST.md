# Boiler PID selection, monitoring and rollback

**Decision recorded 2026-11-27:** keep the new `BoilerPid` active for the
boiler, without further gain changes. In one supervised approximately 45s
brew at target 93 C, measured boiler temperature during brew was
92.44..93.13 C. A post-brew dip is acceptable for the current operating
objective; it is not a reason to add feed-forward or increase Ki.

**State on `feature/dark-web-ui`: `BoilerPid` enabled for the boiler only.**
The existing `GaggiaPid` is unchanged for pump pressure regulation.

## Exact mode selection / rollback
In `BenchTestConfig.h`:
```cpp
#define GAGGUINO_EXPERIMENTAL_BOILER_PID
```
Comment out that one line to return to the legacy boiler controller, then rebuild/upload
the firmware. This is a **compile-time** choice, not a web setting.
The device exposes `GET /api/system/boiler-pid`: active=experimental/shadow=legacy
with `overtempTripped` boolean. In legacy mode, active=legacy/shadow=experimental.

The extended thermal CSV always logs P/I/D of the active PID in `active_*`,
and the opposite implementation in `shadow_*`. Values are **counterfactual**
outputs computed on the active controller's temperatures, not a prediction
of closed-loop temperatures.

## Before any machine-powered testing
- Verify that the machine retains its **independent, functional hardware thermal
  cut-out/fuse and pressure protection**. Software protection and rollback do
  not replace these devices.
- Compile and test on a disconnected USB-powered ESP32 with GPIO14 not driving
  a mains heater. Check thermosensor failure / reset modes, then restore the
  actual machine only after those checks pass.
- Keep the existing pump coefficients and pressure settings unchanged.
- Ensure the PID data and setpoint are valid, with live diagnostics visible.
- Keep someone present, with an immediately accessible way to **disconnect
  machine mains power**. Do not leave it unattended during warm-up, brewing,
  or the post-brew thermal recovery.

## Software backup cutoff in experimental mode
- A **latched** heater-off trip at `temperature >= target + 5 C`, or
  `temperature >= 140 C`, or an invalid target in the experimental safety gate.
- This latch persists until ESP32 restart; **never treat reboot as a safe
  recovery** before investigating the cause. Disconnect machine mains power
  if unexpected temperature rise, steam release, abnormal heating, or a trip occurs.
- Existing `TemperatureGuard` freshness checks still disable heating on
  unqualified LMT01 measurements.
- Software protection is not an independent cutoff and cannot guarantee
  safe operation if an SSR/relay fails closed, MCU crashes, sensor detaches,
  or wiring fails.

## Suggested first evidence collection
With the system checked by a competent person and no actuator wiring or safety
devices bypassed, first observe steady-temperature warm-up and stabilization,
then only if it behaves normally observe a single supervised brew.
After brew finishes, collect the RAM thermal CSV after its 60-second post-roll.
Look for unexpected overshoot, latched trips, sensor faults, reset events,
and actuator/command discrepancies.

Do not start by changing boiler gains or enabling feed-forward.
The active coefficients are the saved boiler settings. This experimental
algorithm has different dynamics even with the same coefficients.
The Android/web UI already provides the CSV download.
