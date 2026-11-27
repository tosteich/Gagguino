#pragma once
// Select the boiler PID implementation at firmware build time.
// Experimental mode affects ONLY boiler heater regulation, never pump PID.
// Revert this define to return to the previous proven controller.
// Requires an intact independent thermal cutout and supervised validation.
#define GAGGUINO_EXPERIMENTAL_BOILER_PID

// Enable only on an isolated USB-powered ESP32. NEVER on installed equipment.
// #define GAGGUINO_LMT_BENCH_TEST

// Archive-only test generator: uncomment ONLY on an isolated USB ESP32.
// It generates files without operating the pump or heater.
// #define GAGGUINO_SHOT_ARCHIVE_TEST

// USB-only replacement of individual LittleFS web assets, no formatting.
// Enable solely on a disconnected, USB-powered ESP32 development board.
// #define GAGGUINO_USB_WEB_UPDATE
