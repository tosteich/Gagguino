# Boiler PWM duty diagnostic (USB-only)

Standalone, repeated integration test of the **production Boiler class** on an isolated spare ESP32 Dev Module.

**DANGER:** GPIO14 intentionally becomes HIGH, including a continuous HIGH output during 100% power. **GPIO14 MUST BE PHYSICALLY DISCONNECTED.** Do not connect a relay, boiler, triac, pump, mains, sensor, coffee machine, or any load. Use only a bare USB-powered ESP32. The GPIO23->GPIO25 jumper used in LMT01 tests is not required; disconnect it while USB is unplugged.

1. Power the spare ESP32 down. Verify GPIO14 is physically unconnected.
2. Open `tests/boiler_power_diagnostic/boiler_power_diagnostic.ino` in Arduino IDE. The local `Boiler.h` is an exact copy of the production header at creation; resynchronize it if Boiler.h changes.
3. Select **ESP32 Dev Module**, Arduino-ESP32 **3.3.12**. Upload over USB.
4. Open Serial Monitor at **115200 baud** and capture one complete `BEGIN cycle=...` through `END cycle=...` segment (about 35 seconds).

The sketch tests 0%, 10%, 50%, 90% and 100% requests using 1,000ms PWM periods. Each setpoint is measured over ~5 seconds with 1ms polling, prints `DUTY requested=... measured=... transitions=... PASS/FAIL`, and then forces an immediate OFF command and verifies it remains OFF. Finally checks that changing the period to 0 forces OFF.

A healthy run should contain `fail=0`. Slight deviations from target duty can occur due to Arduino loop scheduling. Do not interpret this test as a guarantee of switching timing or safe control of AC power; it does not characterize GPIO behavior during reset/boot, electrical relay polarity, actual heater thermodynamics, or independent thermal limits.

**Bench flag:** This sketch is independent from `GAGGUINO_LMT_BENCH_TEST` in the main firmware and overwrites the main sketch on the test board when uploaded.
