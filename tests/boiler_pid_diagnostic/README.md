# Synthetic boiler PID diagnostic

A standalone Arduino sketch for an isolated, USB-powered **ESP32 Dev Module** with Arduino-ESP32 **3.3.12**, using a synchronized copy of production `GaggiaPid.h`.

1. Disconnect the test ESP32 from **all coffee machine wiring, mains, relays, GPIO14, sensors, and loads**. This test requires only USB; GPIO23/GPIO25 jumper is unnecessary.
2. Open `tests/boiler_pid_diagnostic/boiler_pid_diagnostic.ino` in Arduino IDE.
3. Upload the sketch; open Serial Monitor at **115200 baud**.
4. Capture all lines from `PID SYNTHETIC DIAGNOSTIC` through `PID_DIAGNOSTIC_DONE`.

The test applies artificial input sequences (80, 85, 90, 94, 95, 96, 100 °C, target 95 °C), prolonged positive and negative error, loss-of-trust reset, steam-to-brew reset, NaN/infinite measurements, and the current derivative response. It uses illustrative PID gains; these **are not recommended physical tuning values**.

Expected: `PID_DIAGNOSTIC_DONE ... fail=0`. Failure means the algorithm diverges from the current regression expectations. PASS does not prove stable real-boiler regulation, absence of overshoot or electrical/thermal safety.

**Derivative distinction:** the boiler uses negative derivative-on-measurement, opposing rising temperature; the pump retains its historical positive sign. The diagnostic checks both. This is a behavior change for boiler setups with nonzero Kd; do not reuse existing Kd tuning on a real machine without further validation.

The standalone test does not invoke production `loop()` or `TemperatureGuard`; it simulates the application's PID `resetState()` calls. It does not simulate a closed-loop boiler thermal plant.
