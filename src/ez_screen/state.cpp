#include "ez_screen/detail/state.hpp"

#include "EZ-Template/api.hpp"
#include "pros/misc.hpp"
#include "pros/rtos.hpp"

#include <cstdio>
#include <cstring>

namespace ez_screen {
namespace state {

ez::Drive* chassis = nullptr;
std::vector<Auton> autons;
int selected_auton = 0;
int stock_auton_count = 0;

PidDef pids[4] = {
    {"Drive", 20.0f, 0.0f, 100.0f, 0},
    {"Turn", 3.0f, 0.05f, 20.0f, 15},
    {"Swing", 6.0f, 0.0f, 65.0f, 0},
    {"Heading", 11.0f, 0.0f, 20.0f, 0},
};

std::vector<PortDev> ports;
std::vector<HealthDev> health_devs;

bool comp_locked() {
  return pros::competition::is_connected() && !pros::competition::is_disabled();
}

void select_auton(int idx) {
  if (idx < 0 || idx >= (int)autons.size()) return;
  selected_auton = idx;
  // Keep the stock selector pointed at the same routine so a stock
  // autonomous() (ez::as::auton_selector.selected_auton_call()) still runs
  // what the screen shows.
  if (autons[idx].stock_idx >= 0) ez::as::auton_selector.auton_page_current = autons[idx].stock_idx;
  sd_save();
}

void pid_apply(int idx) {
  if (chassis == nullptr) return;
  const PidDef& p = pids[idx];
  switch (idx) {
    case 0: chassis->pid_drive_constants_set(p.kp, p.ki, p.kd, p.start_i); break;
    case 1: chassis->pid_turn_constants_set(p.kp, p.ki, p.kd, p.start_i); break;
    case 2: chassis->pid_swing_constants_set(p.kp, p.ki, p.kd, p.start_i); break;
    case 3: chassis->pid_heading_constants_set(p.kp, p.ki, p.kd, p.start_i); break;
  }
}

// ---- console ring ----

namespace {
constexpr int kRing = 16;
LogLine ring[kRing];
int ring_n = 0;
int ring_head = 0;
bool ring_dirty = false;
pros::Mutex ring_mx;
}  // namespace

void log_push(uint32_t color, const char* tag, const char* msg) {
  ring_mx.take();
  LogLine& l = ring[ring_head];
  l.ms = pros::millis();
  l.color = color;
  snprintf(l.tag, sizeof(l.tag), "%s", tag);
  snprintf(l.msg, sizeof(l.msg), "%s", msg);
  ring_head = (ring_head + 1) % kRing;
  if (ring_n < kRing) ring_n++;
  ring_dirty = true;
  ring_mx.give();
}

bool log_drain(std::vector<LogLine>& out) {
  ring_mx.take();
  if (!ring_dirty) {
    ring_mx.give();
    return false;
  }
  out.clear();
  int count = ring_n < kLogMax ? ring_n : kLogMax;
  for (int i = count; i > 0; i--) {
    int idx = (ring_head - i + kRing) % kRing;
    out.push_back(ring[idx]);
  }
  ring_dirty = false;
  ring_mx.give();
  return true;
}

// ---- persistence ----

bool sd_ok = false;

void sd_save() {
  FILE* f = fopen("/usd/ez_screen.txt", "w");
  if (f == nullptr) {
    sd_ok = false;
    return;
  }
  sd_ok = true;
  fprintf(f, "auton %d\n", selected_auton);
  for (size_t a = 0; a < autons.size(); a++)
    for (size_t s = 0; s < autons[a].settings.size(); s++)
      fprintf(f, "set %d %d %d\n", (int)a, (int)s, autons[a].settings[s].v);
  for (int i = 0; i < 4; i++)
    fprintf(f, "pid %d %f %f %f %d\n", i, (double)pids[i].kp, (double)pids[i].ki, (double)pids[i].kd, pids[i].start_i);
  for (size_t i = 0; i < ports.size(); i++)
    fprintf(f, "port %d %d %d %d %d\n", (int)i, ports[i].port, ports[i].reversed ? 1 : 0, ports[i].cartridge,
            ports[i].removed ? 1 : 0);
  fclose(f);
}

void sd_load() {
  FILE* f = fopen("/usd/ez_screen.txt", "r");
  if (f == nullptr) {
    sd_ok = false;
    return;
  }
  sd_ok = true;
  char line[96];
  while (fgets(line, sizeof(line), f) != nullptr) {
    int a, s, v, p, rev, cart, rm;
    float kp, ki, kd;
    if (sscanf(line, "auton %d", &a) == 1) {
      if (a >= 0 && a < (int)autons.size()) selected_auton = a;
    } else if (sscanf(line, "set %d %d %d", &a, &s, &v) == 3) {
      if (a >= 0 && a < (int)autons.size() && s >= 0 && s < (int)autons[a].settings.size()) {
        Setting& st = autons[a].settings[s];
        if (st.t == SType::ENUM) {
          if (v >= 0 && v < (int)st.opts.size()) st.v = v;
        } else if (v >= st.mn && v <= st.mx) {
          st.v = v;
        }
      }
    } else if (sscanf(line, "pid %d %f %f %f %d", &a, &kp, &ki, &kd, &v) == 5) {
      if (a >= 0 && a < 4) {
        pids[a] = {pids[a].name, kp, ki, kd, v};
        pid_apply(a);
      }
    } else if (sscanf(line, "port %d %d %d %d %d", &a, &p, &rev, &cart, &rm) == 5) {
      if (a >= 0 && a < (int)ports.size()) {
        ports[a].port = p;
        ports[a].reversed = rev != 0;
        ports[a].cartridge = cart;
        ports[a].removed = rm != 0;
      }
    }
  }
  fclose(f);
  if (!autons.empty() && autons[selected_auton].stock_idx >= 0)
    ez::as::auton_selector.auton_page_current = autons[selected_auton].stock_idx;
}

}  // namespace state
}  // namespace ez_screen
