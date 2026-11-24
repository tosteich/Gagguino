# Full-firmware LMT01 integration bench test

**Bench only, separate bare ESP32 powered by USB.** This mode drives GPIO14 HIGH when trusted; GPIO14 MUST remain physically unconnected. All mains, relays, pump, boiler, dimmer and machine wiring must be absent. This is not an authorization to use the modified firmware on a coffee machine.

1. Open the PR branch `fix/esp32-timer-and-heater-safety` in Arduino IDE.
2. Disconnect USB, install a single GPIO23 -> GPIO25 jumper, leave GPIO14/18/19 unconnected, reconnect USB.
3. Uncomment `#define GAGGUINO_LMT_BENCH_TEST` in root `BenchTestConfig.h`. **This setting is OFF by default.**
4. Compile and upload full Gagguino using ESP32 Dev Module and Arduino-ESP32 3.3.12.
5. Open Serial Monitor at **9600 baud** (the full firmware setting); observe `BENCH` logs through two or more 10-second cycles.
6. Observe WebSocket `updateInfo` for `currentTemp` and `currentPower`.

Each cycle sends simulated 1200-pulse LMT-like frames (~25 C) for six seconds, then silence for four seconds. Real production PCNT, TemperatureGuard, main loop, Boiler and WebSocket process these readings. In bench mode only, a trusted positive temperature requests 100% power, so GPIO14 goes HIGH. After >1 second of silence temperature becomes untrusted, heatPower becomes 0, GPIO14 LOW; recovery requires two valid frames. The displayed temperature may remain 25 C when the sensor has expired: rely on `trusted` and `heatPower` to detect the fault.

**After testing, comment out the #define again.** Do not upload bench-enabled builds to the machine. This test does not verify actual LMT01 electrical behavior, physical relay polarity, watchdog failures, independent thermal cutoff or live 230V behavior.
