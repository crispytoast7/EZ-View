#pragma once

#include "ez_screen/detail/kit.hpp"

#include <functional>
#include <string>
#include <vector>

namespace ez {
class Drive;
}

// Live data behind the UI: auton registry, PID table, port table, device
// health, the console ring, and the move recorder. The pages render this;
// the public API in ui.hpp mutates it.
namespace ez_screen {
namespace state {

struct Auton {
  std::string name;
  std::string desc;
  int dur_s = 15;
  uint32_t color = 0x777777;
  std::function<void()> fn;
  std::vector<Setting> settings;
  int stock_idx = -1;  // >= 0: mirrors ez::as::auton_selector.Autons[stock_idx]
};

extern ez::Drive* chassis;
extern std::vector<Auton> autons;
extern int selected_auton;
extern int stock_auton_count;  // entries mirrored from ez::as; selection syncs back for these

void select_auton(int idx);  // updates ez::as page + persists

struct PidDef {
  const char* name;
  float kp, ki, kd;
  int start_i;
};
extern PidDef pids[4];
void pid_apply(int idx);  // push one row's constants into the chassis

struct PortDev {
  std::string role;
  int port = 1;
  bool reversed = false;
  int cartridge = 1;  // 0 red, 1 green, 2 blue
  bool removed = false;
};
extern std::vector<PortDev> ports;

struct HealthDev {
  std::string name;
  int port;
  int type;  // matches ez_screen::Dev
};
extern std::vector<HealthDev> health_devs;

// True while a competition switch is connected and a match mode is active;
// selection and edits are refused so nothing changes mid-match.
bool comp_locked();

// ---- console ring ----
struct LogLine {
  uint32_t ms;
  uint32_t color;
  char tag[12];
  char msg[64];
};
constexpr int kLogMax = 16;
void log_push(uint32_t color, const char* tag, const char* msg);
// Copies the newest lines (oldest first) and clears the dirty flag.
// Returns false when nothing changed since the last call.
bool log_drain(std::vector<LogLine>& out);

// ---- persistence (/usd/ez_screen.txt; silently a no-op without an SD card) ----
extern bool sd_ok;
void sd_save();
void sd_load();

}  // namespace state
}  // namespace ez_screen
