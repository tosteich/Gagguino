# GPIO23 to GPIO25 line verification

This simple test isolates the physical jumper and PCNT from FreeRTOS LEDC, temperature safety, and WebSocket.

On a **separate bare ESP32 USB-powered board**, with USB disconnected first, connect only **GPIO23 → GPIO25**. Leave GPIO14, GPIO18, GPIO19 and all other hardware unconnected. Never connect mains, relays, pump, or boiler.

Open `gpio_line_diagnostic.ino` as a standalone Arduino sketch on ESP32 Dev Module with Arduino-ESP32 3.3.12. Serial Monitor **115200**.

It holds GPIO23 LOW for 5 seconds (`LOW_A`), emits precisely 100 falling edges spaced 10 ms apart (`PULSES`), then holds LOW for 5 seconds (`LOW_B`). It repeats.

Expected: `LOW_A` and `LOW_B` show `in=0 count=0` at every second; `PULSES expected=100 before=0 counted=100 input=0 PASS`.

If LOW shows any counts, inspect physical jumper, pin labels, and GPIO interference. If PULSES fails, do not proceed to full-firmware heater tests. This does not verify LMT01 or actual relay logic.
