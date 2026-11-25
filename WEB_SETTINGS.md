# Full web settings editor (feature/dark-web-ui)

All 14 Nextion settings appear in the dark web UI Settings tab:

| Category | Keys | Web units |
|---|---|---|
| Temperature | targetTemp, steamTemp | Celsius, integer |
| Boiler PID | pidKp, pidKi, pidKd | decimal steps of 0.1 |
| Pump PID | pumpKp, pumpKi, pumpKd | decimal steps of 0.1 |
| Preinfusion | preinfDur, preinfPress | seconds / bar |
| Hold | holdDur, holdPress | seconds / bar |
| Decline | declineDur, declinePress | seconds / final bar |

The firmware stores PID/pressure as tenths and seconds/temperature as integers.
The new endpoints are GET /api/settings (snapshot of 14 values and revision)
and POST /api/settings (application/x-www-form-urlencoded with ALL 14 fields).
POST returns 202 (queued); the browser checks GET until the intended values
appear. The main loop serializes application to Preferences, the control PIDs,
and Nextion. Changes are rejected during an active shot, and the UI shows
success only after a subsequent read confirms values. Nextion trigger0 also
publishes updated settings for the web snapshot.

Web form validation:
- Brew temperature 20..105 C; steam temperature 100..144 C.
- PID Kp/Ki/Kd 0..100, increments 0.1.
- Pressure 0..12 bar, increments 0.1.
- Preinfuse and hold duration 0..600 seconds; decline 0..60 seconds.
The existing ESP32 sanitizer is the final authority on stored maximum values.

## Update and test
1. Test on an isolated USB-powered ESP32; no actuators/mains connected.
2. Pull branch feature/dark-web-ui, upload firmware only (no LittleFS erase).
3. If the USB web asset updater was previously enabled on the test board,
   upload **both** data/index.html and data/js/app.js and data/css/style.css
   individually using tools/upload_web_asset.py; hard-refresh browser.
4. Open /api/settings and compare values to Nextion or Preferences.
5. Change one safe value, save all, verify HTTP 202 and /api/settings value,
   reload page, reboot, and verify it persists. Verify legacy settings are
   visible, including zeros that are rejected on saving until corrected.
6. On real hardware, verify Nextion synchronization and profile behavior
   under supervised conditions before deployment. No actual full Arduino
   compile or hardware-in-loop test has been performed by the assistant.

Security: Existing HTTP settings APIs are unauthenticated on the local LAN.
Avoid exposing the device outside a trusted network. HTTP editing is not
intended for use during an active brew.
