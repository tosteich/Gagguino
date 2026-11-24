# LMT01 / PCNT standalone diagnostic

This sketch verifies the **ESP32 hardware pulse counter** and the pulse-to-temperature formula used in Gagguino. It does **not** validate the real LMT01 sensor or electrical interface.

## Wiring

Power off / unplug USB before changing wires. Remove the prior GPIO23 → GPIO19 wire. Connect **GPIO23 → GPIO25** with an insulated jumper on the **same test ESP32 board**. No other connections, 230 V mains, dimmer, pump or boiler. Power from USB only.

## Upload

Open `lmt01_pcnt_diagnostic.ino` as a **separate sketch** (do not include main Gagguino). In Arduino IDE select ESP32 Dev Module and esp32 by Espressif Systems **3.3.12**. Upload and open Serial Monitor at **115200**.

## Expected output

```text
TEST pulses=800 counted=800 temp=0.0000 before=0 stable=1 afterClear=0 PASS
TEST pulses=960 counted=960 temp=10.0000 before=0 stable=1 afterClear=0 PASS
TEST pulses=1120 counted=1120 temp=20.0000 before=0 stable=1 afterClear=0 PASS
TEST pulses=1200 counted=1200 temp=25.0000 before=0 stable=1 afterClear=0 PASS
TEST pulses=1440 counted=1440 temp=40.0000 before=0 stable=1 afterClear=0 PASS
TEST pulses=1600 counted=1600 temp=50.0000 before=0 stable=1 afterClear=0 PASS
```

These are examples, not measured results. This test repeats. Each burst uses 40 us HIGH and 40 us LOW pulses, counts falling edges, samples twice 5 ms apart, then clears PCNT and verifies zero. It tests the edge configuration and read/clear behavior, **not** real LMT01 pulse waveforms, open-wire fault detection, or the correctness of temperature sensing with a real sensor. Do not use this to authorize boiler operation.

If any test reports FAIL or panics, share the full log before attempting real sensor hookup.
