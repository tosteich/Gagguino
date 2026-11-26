# Gagguino Android

Native Android WebView shell for the ESP32-hosted Gagguino web interface.

- No changes to ESP32 firmware or LittleFS assets.
- Connects over HTTP and ws:// to a user-selected RFC1918 private IPv4 address.
- WebView supports the existing Ukrainian / English UI, graphs, settings, archive and local storage.
- Stores the ESP32 address on the device; has reload, offline and change-address controls.
- Generates an installable **debug-signed APK** through GitHub Actions. This is for personal testing, not Play Store distribution.
- The app and ESP32 must share a trusted LAN. The firmware's HTTP API currently has no TLS/authentication.

## Cloud build

1. Open **Actions → Build Android APK** in GitHub.
2. Choose branch `feature/dark-web-ui` and **Run workflow**, if the manual trigger is visible.
3. Or push a change to `android/**` on that branch; the workflow runs automatically.
4. Open the successful workflow run, find **Artifacts**, download `gagguino-android-debug-apk` ZIP, and install `app-debug.apk`.

Note: GitHub only displays the manual trigger for workflows registered on the repository's default branch; until merged there, use the branch's automatic push trigger.

The workflow installs JDK, Android SDK, and Gradle in GitHub's Ubuntu runner. No Android Studio is required.
