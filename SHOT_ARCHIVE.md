# Local shot archive (experimental)

Implemented on `feature/dark-web-ui`. The verified firmware branch and FTP configuration remain untouched.

## Design
- Start/stop detection: existing `StopWatch.getIsFlow()`.
- Duration check: `StopWatch.getTimerTime()` at stop, at least **15,000 ms** inclusive.
- Sampling: existing 200 ms sensor cadence (**5 Hz**, no new high-frequency interrupt).
- Capacity: at most **60 completed shots**, oldest file deleted upon overflow.
- Each shot: capped at **600 samples** (up to 120 seconds of sampled data).
  A longer physical shot is recorded with its actual duration but the samples
  after 120 seconds are not retained.
- Binary `/shots/00000001.bin`, etc., within ESP32 LittleFS.
- Writer uses a separate low-priority FreeRTOS task. The main control loop
  only stores packed measurements in a bounded RAM buffer and queues the
  completed shot. If both buffers are occupied, a shot is dropped rather than
  blocking the boiler or pump control loop.
- Temporary file `/shots/pending.tmp` is renamed after the full write.
  An interrupted write may leave a temporary file, which is not exposed
  as a completed shot.
- No flash write on each 200ms sensor update. One write per completed shot,
  plus oldest-file removal when needed.

## Binary v1 (little endian)
Header 16 bytes:
`uint32 magic(0x31485347), uint16 version(1), uint16 count,
uint32 durationMs, uint32 startedUnix(0 reserved)`.
Each sample 8 bytes:
`uint16 tick(200ms), int16 temperature*10, uint16 pressure*100,
uint8 heatPercent, uint8 pumpPercent`.

## Web
- `GET /api/shots`: list of shots in JSON.
- `GET /api/shot?id=N`: bounded binary archive record.
- The **Графики** tab lists the stored shots and previews the selected shot
  on the same combined temperature/pressure chart.
- **Скачать CSV** converts the binary data locally in the browser.
- DEMO is browser-only, does not write shot records to LittleFS.

## Important caveats
- **Uploading a new LittleFS image via the IDE uploader may overwrite/erase
  the entire filesystem partition, including previously archived shots.**
  Keep backups before another filesystem upload. A normal firmware-only
  upload with the same partition layout does not intentionally touch it.
- No timestamp is captured yet; records have a stable sequential ID and
  duration. The v1 field `startedUnix` is reserved as zero.
- The format has a header and length validation, but no CRC. Flash corruption
  beyond structural validation is not yet detected.
- FTP remains enabled/unchanged; disable it only after on-device archive
  recording and reads are verified.
- Not verified with an Arduino compile or real shot yet. Test on an isolated
  ESP32 board first, then a hardware-in-the-loop setup with mains-safe wiring.
- Retention should be tested across boots and from 60 to 61 saved shots.
- LittleFS can block an asynchronous web handler briefly while reading.
  File operations never occur in the control loop, but a performance
  measurement on the actual board remains necessary.

## Synthetic archive test generator (USB bench only)
- Set `#define GAGGUINO_SHOT_ARCHIVE_TEST` in `BenchTestConfig.h` (it is **disabled by default**).
- Compile/upload firmware to an **isolated, USB-powered ESP32**, with no pump, heater,
  relay, dimmer or mains-connected circuitry attached.
- After LittleFS mounts and `/api/shots` returns `[]`, open
  `GET /api/test-shot?seconds=10` to exercise rejection below 15 seconds.
  Then verify that `/api/shots` remains `[]`.
- Open `GET /api/test-shot?seconds=20`. HTTP 202 means **queued**, not saved.
  After the low-priority writer finishes, `/api/shots` should contain one
  entry with `durationMs:20000` and `samples:100`; open the entry in the
  **Графики** tab or download CSV.
- The generator synthesizes 5 Hz measurements of temperature, pressure,
  heater and pump *values*. It does NOT change heater or pump control signals,
  interact with the stopwatch or simulate physical GPIO button presses.
- Repeat for 60/61 entries and reboot to inspect retention. A new filesystem
  upload can erase the archive; normal firmware-only upload does not intentionally
  overwrite LittleFS with an unchanged partition layout.
- The request is processed by the main loop, never by the AsyncWebServer
  callback; the existing background writer commits the binary file.
- Disable the flag before connecting the firmware to installed coffee-machine
  hardware. The endpoint is absent entirely when disabled.
- No real hardware compilation or bench run has been performed yet.

## Sequential batch stress test (bench flag must remain enabled)
- Call `GET /api/test-shots?count=65&seconds=20`. A 202 response
  means the batch was queued. The main loop generates a 100-sample synthetic
  record, waits for the archive writer's success counter, then generates
  the next record. It does not block the PID/main loop for 65 shots.
- Poll `GET /api/test-shots/status` for `state` (queued/running/completed/failed),
  `total`, `queued`, `saved`, and `failed`. Do not issue another batch
  while running (HTTP 409). If a write fails, the batch stops.
- After `state=completed` and `saved=65`, refresh `/api/shots`:
  expect exactly 60 records, with the oldest five removed *relative to the
  archive contents before the test*. Existing records also participate in
  pruning, so save any important records first.
- No filesystem asset upload required: only firmware files changed.
- The synthetic archive test endpoints are accessible on the local network
  while the bench flag is enabled, and they modify LittleFS. Disable the flag
  before connecting to machine hardware.
