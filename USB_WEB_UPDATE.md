# USB-only web asset updates (experimental)

This is a BENCH-ONLY maintenance mechanism in `feature/dark-web-ui`.
It updates only `/index.html`, `/css/style.css`, or `/js/app.js` in mounted
LittleFS, without formatting or replacing the filesystem image.
It never accepts paths from the caller, and never writes under `/shots`.

## First use
1. Disconnect all mains, pump, heater, dimmer and relays. Use an isolated
   ESP32 Dev Module on USB power. Do not enable this firmware on a working
   espresso machine.
2. In `BenchTestConfig.h`, uncomment
   `#define GAGGUINO_USB_WEB_UPDATE`. Keep this definition disabled in
   regular builds. Compile and upload the firmware only, preserving the
   existing LittleFS partition and partition layout.
3. Install Python serial support on the PC:
   `py -m pip install pyserial`.
4. Close Arduino Serial Monitor (only one program can own COM port).
5. From the repository directory, run for example:

   `py tools/upload_web_asset.py --port COM5 --asset js --file data/js/app.js`

   Replace `COM5` with your connected ESP32 port. Run separately for
   `--asset html --file data/index.html` or
   `--asset css --file data/css/style.css`.

6. Wait for `WEBUPDATE OK COMMIT`. Hard-refresh the browser
   (Ctrl+Shift+R) and inspect the archive via `/api/shots`.

## Behavior
- USB serial at 9600 baud, acknowledged chunks of 64 bytes.
- File limit 64 KiB. CRC32, length and strict destination whitelist validated.
- Writes `/webupdate.tmp`, then renames existing target to a `.bak` file
  and installs the new target. The immediately previous asset is kept as a
  backup. A reboot after an interrupted rename restores a missing asset from
  its backup. CRC failure/timeout discards the temporary file.
- Serialized against the archive writer by its LittleFS mutex. During upload,
  archive API reads may return HTTP 503, and the archive writer waits; **never
  operate the machine during this maintenance**.
- The script uploads files already checked out locally. Nothing is fetched
  from the network by the firmware.
- This protects against accidental bulk LittleFS formatting, but does not
  guarantee recovery from a failed flash chip, corrupted filesystem or loss
  of power during flash I/O. Back up important shot records independently.
- The updater is not a general firmware OTA mechanism and has no network
  upload endpoint. It is compiled out completely unless enabled.

## Testing
Compare the count/IDs from `/api/shots` before and after uploading JS.
Verify the web UI still renders and the previous shot opens.
Repeat with CSS and HTML, then restart the board and verify shot retention.
Disable the compile-time flag again before using the board in a real machine.

Compilation and on-device tests are still required.
