#pragma once

#include <cstdint>
#include <vector>

namespace ez {
class Drive;
}

// EZ-Screen: a full-screen LVGL UI for EZ-Template — auton selector with
// per-auton options, live device health, console, SD-backed port table, and
// a manual PID tune page.
//
//   void initialize() {
//     // stock EZ registration still works and renders in EZ-Screen:
//     ez::as::auton_selector.autons_add({{"Safe\n\nno-risk route", safe_auton}});
//
//     // or register through EZ-Screen to get per-auton options:
//     int match = ez_screen::auton_add("Match", "main route", 15, 0xFF93D5, match_auton);
//     ez_screen::auton_setting_step(match, "Rings", 4, 0, 6);
//     ez_screen::auton_setting_toggle(match, "Ladder", true);
//
//     ez_screen::init(&chassis);  // replaces ez::as::initialize()
//   }
//
//   void autonomous() { ez_screen::auton_run(); }
//   // (a stock ez::as::auton_selector.selected_auton_call() also stays in
//   //  sync for autons registered the stock way)
//
// Inside an auton: int rings = ez_screen::setting_get("Rings");
namespace ez_screen {

// Build the UI, import stock ez::as autons, load /usd/ez_screen.txt, and
// start the health / console tasks. Call once at the end of initialize();
// do not also call ez::as::initialize().
//
// rotation: 0 or 180 keep the landscape layout (180 flips the panel for
// upside-down mounts); 90 / 270 rebuild EZ-Screen as a portrait UI with a
// bottom tab bar (flappy bird is unavailable in portrait).
void init(ez::Drive* chassis = nullptr, int rotation = 0);

// ---- autons ----
int auton_add(const char* name, const char* desc, int seconds, uint32_t color, void (*fn)());
void auton_setting_step(int auton, const char* name, int def, int mn, int mx, const char* unit = "");
void auton_setting_enum(int auton, const char* name, std::vector<const char*> opts, int def = 0);
void auton_setting_toggle(int auton, const char* name, bool def);

// Run the selected auton (call from autonomous()).
void auton_run();

// Value of a setting on the selected auton: STEP -> value, TOGGLE -> 0/1,
// ENUM -> option index. Returns fallback when the setting doesn't exist.
int setting_get(const char* name, int fallback = 0);

// ---- ports ----
// Register the devices shown on the Ports page; the table persists to SD
// and is meant to be read back at the next boot to construct devices.
void port_add(const char* role, int port, bool reversed, int cartridge_rpm_idx);

// ---- health ----
// Drive motors and the chassis IMU register automatically; add anything else.
enum class Dev { MOTOR, IMU, ROTATION, DISTANCE };
void health_add(int port, const char* name, Dev type = Dev::MOTOR);

// ---- console ----
void log(uint32_t color, const char* tag, const char* fmt, ...);

}  // namespace ez_screen
