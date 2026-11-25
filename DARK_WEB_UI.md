# Dark Minimal web UI (ESP32 Arduino Core 3.3.12)

The existing **fix/esp32-timer-and-heater-safety** branch remains unchanged.
This is a separate, experimental UI-only branch. The heater, pump and PID
algorithms are not modified in this branch.

## Files

- `data/index.html` — offline dashboard and settings
- `data/css/style.css` — responsive dark theme
- `data/js/app.js` — native Canvas charts, WebSocket client and existing HTTP settings
- `webserver.ino` — mounts LittleFS, serves these assets, falls back to old HTML
- `01_setup.ino` — initializes LittleFS before `server.begin()`

The new UI uses **no external CSS/JS, CDN, Bootstrap or Highcharts**.
It uses `/ws` and existing `/targetTemp`, `/changePidKp`,
`/changePidKi`, `/changePidKd` endpoints. These endpoints currently accept
validated updates and queue them for processing in the main loop. A 202
response means *queued*, not yet applied. WebSocket messages confirm values.

No browser control of pump, steam mode, shot profiling or machine power has
been added. Reboot uses the existing WebSocket `reboot` command and confirms
before sending.

## Install the filesystem (Arduino IDE 2.x)

1. **Back up the existing firmware and settings**. Keep the tested branch
   available; do not install this experiment into the machine before verifying.
2. Select board **ESP32 Dev Module** and board package **3.3.12**.
3. Select a **partition scheme that includes LittleFS/SPIFFS data space**.
   Avoid changing the partition layout without considering existing flash data:
   repartitioning and filesystem upload can erase stored files.
4. Compile the sketch normally.
5. Install an **ESP32 LittleFS filesystem uploader compatible with your IDE
   and installed ESP32 core**, or use PlatformIO `uploadfs` with a matching
   partition table. Upload the contents of the `data/` directory as an ESP32
   LittleFS filesystem image to the filesystem partition. A normal **Upload**
   button only uploads the firmware sketch and does not upload `data/`.
6. Open `http://<ESP32_IP>/`, and check browser Network/Console and the
   Serial Monitor. A healthy mount logs `Dark UI available from LittleFS`.
   Verify `/css/style.css` and `/js/app.js` load (HTTP 200).
7. Turn off Internet access while staying on the local Wi-Fi network and
   refresh. Dashboard and live charts should continue to work.
8. Test changing target temperature and PID only when safe for your machine.
   Verify the displayed values returned by WebSocket.

**Safety:** no software UI replaces the independent boiler thermal cutoff.
Flash or filesystem operations should be performed with the machine's mains
circuits disconnected, following appropriate electrical safety practices.

## Fallback behavior

If LittleFS is not available, cannot mount, or its assets are missing, the
firmware serves the previous embedded `webpage.h` page at `/`.
The legacy page still references external CDNs and is **not** fully offline.
To enjoy offline operation the LittleFS files must be uploaded.

The current prototype shows the last 60 telemetry samples since opening the
page; it does not persist a shot archive. The chart is drawn locally and is
intentionally lightweight.

## Current status

Static source implementation only. Arduino compilation, LittleFS upload,
browser integration and on-device visual verification are still required.
