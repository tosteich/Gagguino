# ESP32 pump timer diagnostic (Arduino-ESP32 3.3.12)

This is a **standalone test sketch** and is deliberately separate from Gagguino firmware. It exercises the GPIO-interrupt-to-one-shot-timer path used in the draft `pump.ino`.

## Wiring, power, and uploading

1. Disconnect the ESP32 from the coffee machine. Remove **all** connections to the pump, dimmer, relay, sensor circuits, mains power, and other peripherals.
2. With the USB cable disconnected, connect **GPIO23 to GPIO19** using a short insulated jumper wire. Both pins are on the same ESP32 Dev Module; **do not** connect 3.3 V/5 V power pins to each other.
3. Leave **GPIO18 unconnected**.
4. Open `pump_timer_diagnostic.ino` in Arduino IDE as its own sketch, select **ESP32 Dev Module**, Core **3.3.12**, the correct USB serial port, and upload.
5. Open Serial Monitor at **115200 baud**.

The GPIO23 waveform imitates a zero-cross detector at about 100 rising edges/sec for 6 seconds, then stays low for 4 seconds. The test automatically rotates the requested pump power through **0%, 25%, 50%, 75%, 100%** (one level every 10 seconds, a 50-second cycle). The GPIO19 ISR schedules a single timer pulse 5.015 ms after each rising edge. GPIO18 goes high for nominally 40 us per event. Phase delays (theoretical): 9800, 7407.4, 5015, 2622.4 and 230 microseconds. At 0%, the original control formula still produces a gate event near the half-cycle end; it does not mean a zero pulse count.

## Expected results

- At startup, `gate=0` until the first GPIO19 rising edge.
- While `ACTIVE`, `zc` and `gate` grow by roughly 100 each second at every power level.
- During `SILENT`, both counters should stop increasing once any previously scheduled pulse completes; the last active second can straddle a transition.
- `unexpected` should remain zero; `overwritten` should normally remain zero.
- `delayUs(min/max)` is a software-only min/max estimate from ISR timestamps, reset at every power level. It is not a hardware waveform measurement and should not be interpreted as a precision timing validation.
- A crash, watchdog reset, freeze, or rapidly growing `unexpected`/`overwritten` indicates a test failure. Save the full serial log.

These counters verify callback execution, **not** accurate signal timing, pulse width, electrical isolation, or safe operation on mains voltage. **Do not connect a dimmer or pump based solely on this test.**

## Diagnostic limitations

The Arduino Core 3 timer API is based on the GPTimer driver. Calling `timerAlarm()` from a GPIO ISR still needs board-level verification. If it fails, preserve the failing log; do not connect mains hardware. For production, a dedicated GPTimer/ISR strategy may be necessary.

The reported counts are cumulative and are snapshots taken outside interrupt context. At transitions they may differ by one; compare readings after the signal has been silent for at least a full second.
