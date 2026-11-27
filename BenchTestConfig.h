#pragma once
// Selected boiler temperature controller (2026-11-27).
// A supervised shot trace showed 92.44..93.13 C during a 45s extraction
// at a 93 C setpoint; this is a single field observation, not a safety proof.
// Selection affects ONLY boiler heater regulation; pump PID is unchanged.
// Comment out the next line to revert the BOILER to legacy GaggiaPid.
// Keep an independent functioning hardware thermal cutoff.
#define GAGGUINO_EXPERIMENTAL_BOILER_PID

// Enable only on an isolated USB-powered ESP32. NEVER on installed equipment.
// #define GAGGUINO_LMT_BENCH_TEST

// Archive-only test generator: uncomment ONLY on an isolated USB ESP32.
// It generates files without operating the pump or heater.
// #define GAGGUINO_SHOT_ARCHIVE_TEST

// USB-only replacement of individual LittleFS web assets, no formatting.
// Enable solely on a disconnected, USB-powered ESP32 development board.
// #define GAGGUINO_USB_WEB_UPDATE
