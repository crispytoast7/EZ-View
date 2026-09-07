#include "ez_screen/detail/pages.hpp"

#include "ez_screen/detail/kit.hpp"
#include "ez_screen/detail/state.hpp"
#include "ez_screen/ui.hpp"
#include "pros/distance.h"
#include "pros/imu.h"
#include "pros/motors.h"
#include "pros/rotation.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace ez_screen {
namespace {

using state::autons;
using state::pids;
using state::ports;

// ===========================================================================
// Auton page — the registered routines, each with its live options.
// The footer previews the persisted SD line.
// ===========================================================================

std::vector<lv_obj_t*> auton_rows;
lv_obj_t* auton_name;
lv_obj_t* auton_desc;
lv_obj_t* auton_settings;
lv_obj_t* auton_footer;
ControlGroup auton_group;

void auton_footer_sync() {
  if (autons.empty()) return;
  static char buf[160];
  const state::Auton& a = autons[state::selected_auton];
  int off = snprintf(buf, sizeof(buf), "%s  %s", state::sd_ok ? "/usd/ez_screen.txt" : "no sd card",
                     a.name.c_str());
  for (auto& s : a.settings) {
    char key[24];
    size_t k = 0;
    for (; k < sizeof(key) - 1 && s.name[k]; k++) key[k] = (char)tolower((unsigned char)s.name[k]);
    key[k] = 0;
    char val[24];
    if (s.t == SType::TOGGLE)
      snprintf(val, sizeof(val), "%s", s.v ? "on" : "off");
    else if (s.t == SType::ENUM)
      snprintf(val, sizeof(val), "%s", s.opts[s.v]);
    else
      snprintf(val, sizeof(val), "%d%s", s.v, s.unit);
    off += snprintf(buf + off, sizeof(buf) - off, "  %s=%s", key, val);
    if (off >= (int)sizeof(buf)) break;
  }
  lv_label_set_text(auton_footer, buf);
}

void auton_changed() {
  state::sd_save();
  auton_footer_sync();
}

void auton_detail_render() {
  if (autons.empty()) return;
  const state::Auton& a = autons[state::selected_auton];
  lv_label_set_text(auton_name, a.name.c_str());
  lv_label_set_text(auton_desc, a.desc.c_str());
  shell_current_set(a.name.c_str());

  auton_group.clear();
  lv_obj_clean(auton_settings);
  int y = 0;
  for (auto& s : autons[state::selected_auton].settings) {
    control_row(auton_settings, y, &s, auton_group);
    y += 33;
  }
  if (a.settings.empty()) {
    lv_obj_t* none = label(auton_settings, "no options for this auton", &lv_font_montserrat_12, color::text_dim);
    lv_obj_set_pos(none, 0, 8);
  }
  group_sync(auton_group);
  auton_footer_sync();
}

void auton_row_cb(lv_event_t* e) {
  if (state::comp_locked()) {
    log(color::amber, "comp", "selector locked during the match");
    return;
  }
  state::select_auton((int)(intptr_t)lv_event_get_user_data(e));
  list_select(auton_rows, state::selected_auton);
  auton_detail_render();
  enter(auton_name, 0, 8);
  enter(auton_settings, motion::step_ms, 8);
}

void auton_build(lv_obj_t* page) {
  auton_group.on_change = auton_changed;
  const bool pt = layout::portrait;
  lv_obj_t* list = pt ? list_column(page, layout::page_w, 140) : list_column(page, 150);

  if (autons.empty()) {
    lv_obj_t* none = label(page, "no autons registered -\nadd them before ez_screen::init()",
                           &lv_font_montserrat_14, color::text_dim);
    lv_obj_set_style_text_align(none, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(none, LV_ALIGN_CENTER, 0, 0);
  }

  for (size_t i = 0; i < autons.size(); i++) {
    char dur[12];
    snprintf(dur, sizeof(dur), "%ds", autons[i].dur_s);
    lv_obj_t* row = list_row(list, autons[i].name.c_str(), dur, auton_row_cb, (int)i);
    int name_w = (int)lv_text_get_width(autons[i].name.c_str(), (uint32_t)autons[i].name.size(),
                                        &lv_font_montserrat_14, 0);
    lv_obj_t* dot = panel(row);
    lv_obj_set_size(dot, 7, 7);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(autons[i].color), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_align(dot, LV_ALIGN_LEFT_MID, 10 + name_w + 8, 0);
    auton_rows.push_back(row);
  }

  auton_name = label(page, "", pt ? &lv_font_montserrat_20 : &lv_font_montserrat_24, color::pink);
  lv_obj_set_pos(auton_name, layout::col_x, pt ? 148 : 0);
  auton_desc = label(page, "", &lv_font_montserrat_12, color::text_dim);
  lv_obj_set_pos(auton_desc, layout::col_x, pt ? 172 : 28);

  auton_settings = panel(page);
  lv_obj_set_size(auton_settings, layout::col_w, pt ? layout::page_h - 190 - 20 : 132);
  lv_obj_set_pos(auton_settings, layout::col_x, pt ? 190 : 46);
  lv_obj_set_scrollbar_mode(auton_settings, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_scroll_dir(auton_settings, LV_DIR_VER);

  auton_footer = label(page, "", &lv_font_montserrat_10, color::text_dim);
  lv_obj_set_pos(auton_footer, 0, layout::page_h - 16);
  lv_obj_set_size(auton_footer, layout::page_w, 13);
  lv_label_set_long_mode(auton_footer, LV_LABEL_LONG_DOT);

  if (!autons.empty()) {
    list_select(auton_rows, state::selected_auton);
    auton_detail_render();
  }
}

void auton_show() {
  if (autons.empty()) return;
  list_select(auton_rows, state::selected_auton);
  group_sync(auton_group);
  auton_footer_sync();
}

// ===========================================================================
// Health page — live motor temperatures; a device that doesn't answer is
// shown as missing.
// ===========================================================================

std::vector<lv_obj_t*> health_tiles;
std::vector<lv_obj_t*> health_vals;
lv_obj_t* health_msg;

uint32_t temp_color(double t) {
  if (t < 0) return 0x2F4A66;  // no reading: cold blue-gray
  if (t < 40) return 0x34383C;
  if (t < 45) return 0x8A4A6B;
  if (t < 50) return 0xC76BA4;
  if (t < 58) return color::red;
  return 0xFF2E3E;
}

void health_build(lv_obj_t* page) {
  const bool pt = layout::portrait;
  const int tile_w = pt ? 108 : 100;
  const int step_x = pt ? 116 : 105;
  const int wrap_x = pt ? 116 : 330;
  int x = 0, y = 0;
  for (auto& d : state::health_devs) {
    lv_obj_t* tile = panel(page);
    lv_obj_set_size(tile, tile_w, 62);
    lv_obj_set_pos(tile, x, y);
    lv_obj_set_style_radius(tile, 12, 0);
    lv_obj_set_style_bg_color(tile, lv_color_hex(temp_color(-1)), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);

    lv_obj_t* n = label(tile, d.name.c_str(), &lv_font_montserrat_12, color::text);
    lv_obj_align(n, LV_ALIGN_TOP_LEFT, 8, 6);
    lv_obj_t* v = label(tile, "--", &lv_font_montserrat_18, color::text);
    lv_obj_align(v, LV_ALIGN_BOTTOM_LEFT, 8, -6);

    health_tiles.push_back(tile);
    health_vals.push_back(v);
    x += step_x;
    if (x > wrap_x) {
      x = 0;
      y += 68;
    }
  }

  if (state::health_devs.empty()) {
    lv_obj_t* none = label(page, "no devices registered -\npass a chassis to ez_screen::init()",
                           &lv_font_montserrat_14, color::text_dim);
    lv_obj_set_style_text_align(none, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(none, LV_ALIGN_CENTER, 0, -10);
  }

  health_msg = label(page, "cool < 40C  •  warm 45C  •  hot 55C+", &lv_font_montserrat_12, color::text_dim);
  lv_obj_set_pos(health_msg, 0, layout::portrait ? layout::page_h - 24 : 168);
  lv_obj_set_size(health_msg, layout::page_w, 16);
  lv_label_set_long_mode(health_msg, LV_LABEL_LONG_DOT);
}

void health_refresh() {
  int missing = 0;
  static char first_missing[24];
  for (size_t i = 0; i < state::health_devs.size(); i++) {
    const state::HealthDev& d = state::health_devs[i];
    bool ok = false;
    char val[16];
    uint32_t tile = 0x2F4A66;  // missing: cold blue-gray

    switch ((Dev)d.type) {
      case Dev::MOTOR: {
        double t = pros::c::motor_get_temperature(d.port);
        ok = t >= 0.0 && t < 200.0;  // PROS_ERR_F when the port is empty
        if (ok) {
          snprintf(val, sizeof(val), "%dC", (int)t);
          tile = temp_color(t);
        }
        break;
      }
      case Dev::IMU: {
        double h = pros::c::imu_get_heading(d.port);
        ok = h >= 0.0 && h < 1.0e9;
        if (ok) snprintf(val, sizeof(val), "%d", (int)h);
        break;
      }
      case Dev::ROTATION: {
        int32_t a = pros::c::rotation_get_angle(d.port);
        ok = a != INT32_MAX;
        if (ok) snprintf(val, sizeof(val), "%d", (int)(a / 100));
        break;
      }
      case Dev::DISTANCE: {
        int32_t mm = pros::c::distance_get(d.port);
        ok = mm != INT32_MAX;
        if (ok) snprintf(val, sizeof(val), "%dmm", (int)mm);
        break;
      }
    }

    if (ok && (Dev)d.type != Dev::MOTOR) tile = 0x34383C;  // present, no temp tier
    lv_obj_set_style_bg_color(health_tiles[i], lv_color_hex(tile), 0);
    lv_label_set_text(health_vals[i], ok ? val : "N/C");
    if (!ok) {
      if (missing == 0) snprintf(first_missing, sizeof(first_missing), "%s", d.name.c_str());
      missing++;
    }
  }
  if (state::health_devs.empty()) return;
  if (missing == 0)
    lv_label_set_text(health_msg, "all devices ok  •  cool < 40C  •  hot 55C+");
  else if (missing == 1)
    lv_label_set_text_fmt(health_msg, LV_SYMBOL_WARNING " %s not responding - check the cable", first_missing);
  else
    lv_label_set_text_fmt(health_msg, LV_SYMBOL_WARNING " %d devices not responding (first: %s)", missing,
                          first_missing);
}

// ===========================================================================
// Console page — the ez_screen::log ring, newest at the bottom.
// ===========================================================================

std::vector<lv_obj_t*> con_row_objs;
std::vector<lv_obj_t*> con_ts;
std::vector<lv_obj_t*> con_tag;
std::vector<lv_obj_t*> con_msg;
lv_obj_t* con_prompt;

void console_clear_cb(lv_event_t*) {
  for (auto* o : con_row_objs) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(con_prompt, 0, 0);
}

void console_build(lv_obj_t* page) {
  lv_obj_t* term = panel(page);
  lv_obj_set_size(term, layout::page_w, layout::page_h - 10);
  lv_obj_set_pos(term, 0, 0);
  lv_obj_set_style_radius(term, 12, 0);
  lv_obj_set_style_bg_color(term, lv_color_hex(0x0A0A0B), 0);
  lv_obj_set_style_bg_opa(term, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(term, 12, 0);
  lv_obj_set_scrollbar_mode(term, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_scroll_dir(term, LV_DIR_VER);

  for (int i = 0; i < state::kLogMax; i++) {
    lv_obj_t* row = panel(term);
    lv_obj_set_size(row, layout::page_w - 24, 20);
    lv_obj_set_pos(row, 0, i * 24);
    lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
    con_ts.push_back(label(row, "", &lv_font_montserrat_14, 0x6F6F6F));
    lv_obj_set_pos(con_ts.back(), 0, 0);
    con_tag.push_back(label(row, "", &lv_font_montserrat_14, color::text_dim));
    lv_obj_set_pos(con_tag.back(), layout::portrait ? 48 : 62, 0);
    con_msg.push_back(label(row, "", &lv_font_montserrat_14, color::text));
    lv_obj_set_pos(con_msg.back(), layout::portrait ? 98 : 132, 0);
    lv_obj_set_size(con_msg.back(), layout::portrait ? layout::page_w - 24 - 98 : layout::page_w - 24 - 132, 18);
    lv_label_set_long_mode(con_msg.back(), LV_LABEL_LONG_DOT);
    con_row_objs.push_back(row);
  }
  con_prompt = label(term, ">_", &lv_font_montserrat_14, color::pink);
  lv_obj_set_pos(con_prompt, 0, 0);

  lv_obj_t* clear = pill(term, 64, color::pink_deep);
  lv_obj_add_flag(clear, LV_OBJ_FLAG_FLOATING);  // stays put while the log scrolls
  lv_obj_align(clear, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_add_flag(clear, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(clear, console_clear_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* cl = label(clear, "Clear", &lv_font_montserrat_12, color::pink);
  lv_obj_center(cl);
}

void console_refresh() {
  static std::vector<state::LogLine> lines;
  if (!state::log_drain(lines)) return;
  int n = (int)lines.size();
  for (int i = 0; i < state::kLogMax; i++) {
    if (i < n) {
      lv_obj_remove_flag(con_row_objs[i], LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text_fmt(con_ts[i], "[%2u.%02u]", (unsigned)(lines[i].ms / 1000),
                            (unsigned)((lines[i].ms % 1000) / 10));
      lv_label_set_text(con_tag[i], lines[i].tag);
      lv_obj_set_style_text_color(con_tag[i], lv_color_hex(lines[i].color), 0);
      lv_label_set_text(con_msg[i], lines[i].msg);
    } else {
      lv_obj_add_flag(con_row_objs[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
  lv_obj_set_pos(con_prompt, 0, n * 24);
  lv_obj_scroll_to_view(con_prompt, LV_ANIM_ON);
}

// ===========================================================================
// Ports page — the persisted device table (applies at next boot).
// ===========================================================================

std::vector<lv_obj_t*> port_rows;
std::vector<lv_obj_t*> port_row_vals;
lv_obj_t* port_name;
lv_obj_t* port_free_line;
ControlGroup port_group;
Setting port_num = {"Port", SType::STEP, 1, 1, 21, "", {}};
Setting port_rev = {"Reversed", SType::TOGGLE, 0, 0, 1, "", {}};
Setting port_cart = {"Cartridge", SType::ENUM, 1, 0, 2, "", {"Red", "Green", "Blue"}};
int sel_port = 0;

void ports_free_sync() {
  bool used[22] = {false};
  for (auto& p : ports)
    if (!p.removed && p.port >= 1 && p.port <= 21) used[p.port] = true;
  static char buf[80];
  int off = snprintf(buf, sizeof(buf), "free ");
  for (int i = 1; i <= 21 && off < (int)sizeof(buf) - 4; i++)
    if (!used[i]) off += snprintf(buf + off, sizeof(buf) - off, " %d", i);
  lv_label_set_text(port_free_line, buf);
}

void ports_rows_sync() {
  for (size_t i = 0; i < ports.size(); i++) {
    lv_label_set_text_fmt(port_row_vals[i], "%d", ports[i].reversed ? -ports[i].port : ports[i].port);
    if (ports[i].removed)
      lv_obj_add_flag(port_rows[i], LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_remove_flag(port_rows[i], LV_OBJ_FLAG_HIDDEN);
  }
  ports_free_sync();
}

void port_detail_render() {
  if (ports.empty()) return;
  lv_label_set_text(port_name, ports[sel_port].role.c_str());
  port_num.v = ports[sel_port].port;
  port_rev.v = ports[sel_port].reversed ? 1 : 0;
  port_cart.v = ports[sel_port].cartridge;
  group_sync(port_group);
}

void port_row_cb(lv_event_t* e) {
  sel_port = (int)(intptr_t)lv_event_get_user_data(e);
  list_select(port_rows, sel_port);
  port_detail_render();
  enter(port_name, 0, 8);
}

void port_apply_cb(lv_event_t*) {
  if (ports.empty() || state::comp_locked()) return;
  ports[sel_port].port = port_num.v;
  ports[sel_port].reversed = port_rev.v != 0;
  ports[sel_port].cartridge = port_cart.v;
  ports_rows_sync();
  state::sd_save();
  log(color::amber, "ports", "%s -> port %d (next boot)", ports[sel_port].role.c_str(),
      ports[sel_port].reversed ? -ports[sel_port].port : ports[sel_port].port);
}

void port_remove_cb(lv_event_t*) {
  if (ports.empty() || state::comp_locked()) return;
  ports[sel_port].removed = true;
  ports_rows_sync();
  state::sd_save();
}

void ports_build(lv_obj_t* page) {
  const bool pt = layout::portrait;
  lv_obj_t* list = pt ? list_column(page, layout::page_w, 130) : list_column(page, 150);
  for (size_t i = 0; i < ports.size(); i++) {
    lv_obj_t* row = list_row(list, ports[i].role.c_str(), "0", port_row_cb, (int)i);
    port_rows.push_back(row);
    port_row_vals.push_back(lv_obj_get_child(row, 1));
  }

  if (ports.empty()) {
    lv_obj_t* none = label(page, "no port table -\nregister devices in code first",
                           &lv_font_montserrat_14, color::text_dim);
    lv_obj_set_style_text_align(none, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(none, LV_ALIGN_CENTER, 0, 0);
  }

  port_name = label(page, "", pt ? &lv_font_montserrat_20 : &lv_font_montserrat_24, color::pink);
  lv_obj_set_pos(port_name, layout::col_x, pt ? 138 : 0);

  lv_obj_t* cont = panel(page);
  lv_obj_set_size(cont, layout::col_w, 102);
  lv_obj_set_pos(cont, layout::col_x, pt ? 166 : 32);
  lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  control_row(cont, 0, &port_num, port_group);
  control_row(cont, 33, &port_rev, port_group);
  control_row(cont, 66, &port_cart, port_group);

  port_free_line = label(page, "", &lv_font_montserrat_12, color::text_dim);
  lv_obj_set_pos(port_free_line, layout::col_x, pt ? 274 : 138);
  lv_obj_set_size(port_free_line, layout::col_w, 16);
  lv_label_set_long_mode(port_free_line, LV_LABEL_LONG_DOT);
  lv_obj_t* note = label(page, "/usd/ez_screen.txt  •  applies at next boot", &lv_font_montserrat_12,
                         color::text_dim);
  lv_obj_set_pos(note, layout::col_x, pt ? 292 : 156);
  lv_obj_set_size(note, layout::col_w, 16);
  lv_label_set_long_mode(note, LV_LABEL_LONG_DOT);

  lv_obj_t* apply = panel(page);
  lv_obj_set_size(apply, 78, 28);
  lv_obj_set_pos(apply, pt ? 18 : layout::col_x + 88, pt ? 314 : 176);
  lv_obj_set_style_radius(apply, 14, 0);
  lv_obj_set_style_bg_color(apply, lv_color_hex(color::pink), 0);
  lv_obj_set_style_bg_opa(apply, LV_OPA_COVER, 0);
  lv_obj_add_flag(apply, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(apply, port_apply_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* al = label(apply, "Apply", &lv_font_montserrat_14, color::ink);
  lv_obj_center(al);

  lv_obj_t* rm = panel(page);
  lv_obj_set_size(rm, 78, 28);
  lv_obj_set_pos(rm, pt ? 128 : layout::col_x + 176, pt ? 314 : 176);
  lv_obj_set_style_radius(rm, 14, 0);
  lv_obj_set_style_border_width(rm, 1, 0);
  lv_obj_set_style_border_color(rm, lv_color_hex(color::red), 0);
  lv_obj_add_flag(rm, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(rm, port_remove_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* rl = label(rm, "Remove", &lv_font_montserrat_14, color::red);
  lv_obj_center(rl);

  if (!ports.empty()) {
    list_select(port_rows, sel_port);
    ports_rows_sync();
    port_detail_render();
  }
}

void ports_show() { port_detail_render(); }

// ===========================================================================
// Tune page — live PID constants (applied to the chassis immediately, saved
// to SD) and the recorded error curve of the last motion. Manual tuning
// only; this is not the auto tuner.
// ===========================================================================

std::vector<lv_obj_t*> pid_rows;
std::vector<lv_obj_t*> pid_row_vals;
lv_obj_t* tune_name;
lv_obj_t* tune_card_vals[4];
lv_obj_t* tune_footer;
int sel_pid = 0;



void label_set_float(lv_obj_t* l, const char* fmt, double v) {
  // liblvgl's text_fmt has no float support; go through newlib snprintf.
  char b[24];
  snprintf(b, sizeof(b), fmt, v);
  lv_label_set_text(l, b);
}

void tune_sync() {
  const state::PidDef& p = pids[sel_pid];
  lv_label_set_text_fmt(tune_name, "%s PID", p.name);
  label_set_float(tune_card_vals[0], "%.2f", p.kp);
  label_set_float(tune_card_vals[1], "%.3f", p.ki);
  label_set_float(tune_card_vals[2], "%.1f", p.kd);
  lv_label_set_text_fmt(tune_card_vals[3], "%d", p.start_i);
  for (int i = 0; i < 4; i++) label_set_float(pid_row_vals[i], "kP %g", (double)pids[i].kp);
  static char buf[96];
  snprintf(buf, sizeof(buf), "%s  %s  kp=%g ki=%g kd=%g start_i=%d",
           state::sd_ok ? "/usd/ez_screen.txt" : "no sd card", p.name, (double)p.kp, (double)p.ki,
           (double)p.kd, p.start_i);
  lv_label_set_text(tune_footer, buf);
}

void pid_row_cb(lv_event_t* e) {
  sel_pid = (int)(intptr_t)lv_event_get_user_data(e);
  list_select(pid_rows, sel_pid);
  tune_sync();
  enter(tune_name, 0, 8);
}

void tune_step_cb(lv_event_t* e) {
  if (state::comp_locked()) return;
  int code = (int)(intptr_t)lv_event_get_user_data(e);
  int card = code >> 1;
  float dir = (code & 1) ? 1.0f : -1.0f;
  state::PidDef& p = pids[sel_pid];
  switch (card) {
    case 0: p.kp = std::max(0.0f, p.kp + dir * 0.5f); break;
    case 1: p.ki = std::max(0.0f, p.ki + dir * 0.005f); break;
    case 2: p.kd = std::max(0.0f, p.kd + dir * 2.5f); break;
    case 3: p.start_i = std::max(0, p.start_i + (int)dir); break;
  }
  state::pid_apply(sel_pid);
  state::sd_save();
  tune_sync();
}

void tune_build(lv_obj_t* page) {
  const bool pt = layout::portrait;
  const int colx = pt ? 0 : 132;
  const int colw = layout::page_w - colx;
  lv_obj_t* list = pt ? list_column(page, layout::page_w, 118) : list_column(page, 120);
  for (int i = 0; i < 4; i++) {
    lv_obj_t* row = list_row(list, pids[i].name, "", pid_row_cb, i);
    pid_rows.push_back(row);
    pid_row_vals.push_back(lv_obj_get_child(row, 1));
  }

  tune_name = label(page, "", &lv_font_montserrat_20, color::pink);
  lv_obj_set_pos(tune_name, colx, pt ? 124 : 0);

  const char* card_names[4] = {"kP", "kI", "kD", "START I"};
  const int card_w = pt ? (layout::page_w - 8) / 2 : (colw - 15) / 4;
  const int card_h = pt ? 100 : 132;
  for (int c = 0; c < 4; c++) {
    lv_obj_t* card = panel(page);
    lv_obj_set_size(card, card_w, card_h);
    if (pt)
      lv_obj_set_pos(card, (c % 2) * (card_w + 8), 152 + (c / 2) * (card_h + 8));
    else
      lv_obj_set_pos(card, colx + c * (card_w + 5), 40);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(color::surface), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);

    lv_obj_t* n = label(card, card_names[c], &lv_font_montserrat_12, color::text_dim);
    lv_obj_align(n, LV_ALIGN_TOP_LEFT, 8, 8);
    tune_card_vals[c] = label(card, "", &lv_font_montserrat_18, color::text);
    lv_obj_align(tune_card_vals[c], LV_ALIGN_LEFT_MID, 8, -6);

    for (int d = 0; d < 2; d++) {
      lv_obj_t* b = panel(card);
      lv_obj_set_size(b, 28, 26);
      lv_obj_align(b, LV_ALIGN_BOTTOM_LEFT, 5 + d * 32, -8);
      lv_obj_set_style_radius(b, 11, 0);
      lv_obj_set_style_bg_color(b, lv_color_hex(color::surface_hi), 0);
      lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
      lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(b, tune_step_cb, LV_EVENT_CLICKED, (void*)(intptr_t)(c * 2 + d));
      lv_obj_t* bl = label(b, d ? "+" : "-", &lv_font_montserrat_14, color::text);
      lv_obj_center(bl);
    }
  }

  tune_footer = label(page, "", &lv_font_montserrat_10, color::text_dim);
  lv_obj_set_pos(tune_footer, colx, layout::page_h - 16);
  lv_obj_set_size(tune_footer, colw, 13);
  lv_label_set_long_mode(tune_footer, LV_LABEL_LONG_DOT);

  list_select(pid_rows, sel_pid);
  tune_sync();
}

void noop_show() {}

}  // namespace

void health_poll() { health_refresh(); }

void console_poll() { console_refresh(); }

const PageDef kPages[kPageCount] = {
    {LV_SYMBOL_LIST, "Auton", auton_build, auton_show},
    {LV_SYMBOL_CHARGE, "Health", health_build, noop_show},
    {LV_SYMBOL_KEYBOARD, "Console", console_build, noop_show},
    {LV_SYMBOL_USB, "Ports", ports_build, ports_show},
    {LV_SYMBOL_SETTINGS, "Tune", tune_build, noop_show},
};

}  // namespace ez_screen
