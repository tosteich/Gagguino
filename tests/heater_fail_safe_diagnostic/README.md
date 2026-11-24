# Heater fail-safe diagnostic (USB-only)

This test exercises a copy of the production **TemperatureGuard** and **Boiler** classes, using the same ESP-IDF5 PCNT configuration as the firmware. The copies are in this sketch directory because Arduino IDE standalone sketches must be self-contained. Keep them synchronized with production if modifying either class.

**Important:** GPIO14 WILL briefly be driven HIGH during successful test stages. **GPIO14 must remain physically unconnected.** The test must run on the separate bare ESP32 with USB power only, **not on a wired coffee machine**. No relay, mains, pump, boiler, dimmer or any other load is allowed.

1. Disconnect USB.
2. Install one jumper **GPIO23 → GPIO25** on the same ESP32 (remove any old jumper to GPIO19).
3. Check that GPIO14 has no connections.
4. Open `heater_fail_safe_diagnostic.ino` (the directory also contains local `TemperatureGuard.h` and `Boiler.h`).
5. Upload with ESP32 Dev Module / Arduino-ESP32 **3.3.12**; Serial Monitor **115200**.

Expected checks for each cycle:
- `NO_SENSOR`: GPIO14 LOW
- `FIRST_FRAME`: GPIO14 LOW (one frame is not trusted)
- `SECOND_FRAME`: GPIO14 HIGH (two consistent 1200-pulse frames)
- `STILL_FRESH`: GPIO14 HIGH
- `SENSOR_LOST`: GPIO14 LOW (after >1s without frames)
- `RECOVERY_FIRST`: GPIO14 LOW (requires requalification)
- `RECOVERY_SECOND`: GPIO14 HIGH
- `INVALID_FRAME`: GPIO14 LOW (out-of-range pulse count)

Each line should end in `PASS`. `END cycle=... pass=... fail=0` is expected. These are expected outcomes only; actual hardware results require user testing.

**Limits:** Tests only logical GPIO output (using `digitalRead`), not voltage/relay polarity or mains isolation, actual boiler behavior, real LMT01 waveforms, boot glitches, ISR stalls or thermal cutoff. The fixed command is deliberately 100% to detect that the guard can override a nonzero heat request.
