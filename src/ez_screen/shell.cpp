#include "ez_screen/ui.hpp"

#include "EZ-Template/api.hpp"
#include "ez_screen/detail/kit.hpp"
#include "ez_screen/detail/pages.hpp"
#include "ez_screen/detail/state.hpp"
#include "flappy.hpp"

#include <cstdarg>
#include <cstdio>

namespace ez_screen {
namespace {

lv_obj_t* header_title;
lv_obj_t* current_label;
lv_obj_t* rail_btns[kRailPages + 1];
lv_obj_t* rail_icons[kRailPages + 1];
lv_obj_t* page_objs[kPageCount];
lv_obj_t* sheet_backdrop = nullptr;
lv_obj_t* flappy_root = nullptr;

void exec_bg_opa(void* obj, int32_t v) { lv_obj_set_style_bg_opa((lv_obj_t*)obj, (lv_opa_t)v, 0); }

void view_show(int v) {
  lv_label_set_text(header_title, kPages[v].title);
  for (int i = 0; i < kRailPages; i++) {
    bool act = i == v;
    lv_obj_set_style_bg_opa(rail_btns[i], act ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(rail_icons[i], lv_color_hex(act ? color::pink : color::text_dim), 0);
  }
  for (int i = 0; i < kPageCount; i++) lv_obj_add_flag(page_objs[i], LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(page_objs[v], LV_OBJ_FLAG_HIDDEN);
  kPages[v].on_show();
  cascade(page_objs[v]);
  lv_obj_invalidate(lv_screen_active());
}

void view_clicked(lv_event_t* e) { view_show((int)(intptr_t)lv_event_get_user_data(e)); }

// Overlays are dismissed from their own click callbacks, so deletion must
// be deferred until event processing finishes.
void flappy_close_clicked(lv_event_t*) {
  flappy::stop();
  if (flappy_root != nullptr) {
    lv_obj_delete_async(flappy_root);
    flappy_root = nullptr;
  }
  lv_obj_invalidate(lv_screen_active());
}

void flappy_open() {
  if (layout::portrait) {
    log(color::amber, "apps", "flappy bird is landscape-only");
    return;
  }
  if (flappy_root != nullptr) return;
  flappy_root = panel(lv_screen_active());
  lv_obj_set_size(flappy_root, layout::screen_w, layout::screen_h);
  lv_obj_set_pos(flappy_root, 0, 0);
  flappy::start(flappy_root);

  lv_obj_t* x = panel(flappy_root);
  lv_obj_set_size(x, 30, 30);
  lv_obj_set_pos(x, layout::screen_w - 38, 8);
  lv_obj_set_style_radius(x, 10, 0);
  lv_obj_set_style_bg_color(x, lv_color_hex(color::surface_hi), 0);
  lv_obj_set_style_bg_opa(x, LV_OPA_COVER, 0);
  lv_obj_add_flag(x, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(x, flappy_close_clicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* xl = label(x, LV_SYMBOL_CLOSE, &lv_font_montserrat_14, color::text_dim);
  lv_obj_center(xl);

  enter(flappy_root, 0, 0);
  lv_obj_invalidate(lv_screen_active());
}

void sheet_close(lv_event_t*) {
  if (sheet_backdrop != nullptr) {
    lv_obj_delete_async(sheet_backdrop);
    sheet_backdrop = nullptr;
  }
  lv_obj_invalidate(lv_screen_active());
}

void sheet_entry_cb(lv_event_t* e) {
  int which = (int)(intptr_t)lv_event_get_user_data(e);
  sheet_close(nullptr);
  if (which == 0)
    view_show(4);  // Tune
  else
    flappy_open();
}

void sheet_open(lv_event_t*) {
  if (sheet_backdrop != nullptr) return;
  sheet_backdrop = panel(lv_screen_active());
  lv_obj_set_size(sheet_backdrop, layout::screen_w, layout::screen_h);
  lv_obj_set_pos(sheet_backdrop, 0, 0);
  lv_obj_set_style_bg_color(sheet_backdrop, lv_color_hex(0x000000), 0);
  lv_obj_add_flag(sheet_backdrop, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(sheet_backdrop, sheet_close, LV_EVENT_CLICKED, nullptr);
  {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, sheet_backdrop);
    lv_anim_set_duration(&a, motion::base_ms);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_50);
    lv_anim_set_exec_cb(&a, exec_bg_opa);
    lv_anim_start(&a);
  }

  lv_obj_t* sheet = panel(sheet_backdrop);
  lv_obj_set_size(sheet, 220, 128);
  lv_obj_align(sheet, LV_ALIGN_BOTTOM_MID, 0, -(8 + layout::bar_h));
  lv_obj_set_style_radius(sheet, 16, 0);
  lv_obj_set_style_bg_color(sheet, lv_color_hex(color::surface_hi), 0);
  lv_obj_set_style_bg_opa(sheet, LV_OPA_COVER, 0);
  lv_obj_add_flag(sheet, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* t = label(sheet, "More", &lv_font_montserrat_12, color::text_dim);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 8);

  const char* icons[2] = {kPages[4].icon, LV_SYMBOL_VIDEO};
  const char* names[2] = {kPages[4].title, "Flappy Bird"};
  for (int i = 0; i < 2; i++) {
    lv_obj_t* row = panel(sheet);
    lv_obj_set_size(row, 196, 40);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 28 + i * 45);
    lv_obj_set_style_radius(row, 12, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(color::surface), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, sheet_entry_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    bool disabled = layout::portrait && i == 1;  // flappy is landscape-only
    lv_obj_t* ri = label(row, icons[i], &lv_font_montserrat_14, disabled ? color::text_dim : color::pink);
    lv_obj_align(ri, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_t* rt = label(row, disabled ? "Flappy (landscape)" : names[i], &lv_font_montserrat_14,
                         disabled ? color::text_dim : color::text);
    lv_obj_align(rt, LV_ALIGN_LEFT_MID, 42, 0);
  }

  enter(sheet, 0, 140);
}

void ui_build() {
  // A dedicated screen object: display.cpp's portrait llemu screen can load
  // whatever it wants during rotation setup; EZ-Screen loads its own last.
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(color::bg), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  if (layout::portrait) {
    // bottom tab bar
    lv_obj_t* bar = panel(scr);
    lv_obj_set_size(bar, layout::screen_w, layout::bar_h);
    lv_obj_set_pos(bar, 0, layout::screen_h - layout::bar_h);
    lv_obj_set_style_bg_color(bar, lv_color_hex(color::rail), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);

    for (int i = 0; i <= kRailPages; i++) {
      lv_obj_t* b = panel(bar);
      lv_obj_set_size(b, 38, 38);
      lv_obj_align(b, LV_ALIGN_LEFT_MID, 8 + i * 45, 0);
      lv_obj_set_style_radius(b, 12, 0);
      lv_obj_set_style_bg_color(b, lv_color_hex(color::surface_hi), 0);
      lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
      const char* icon = i < kRailPages ? kPages[i].icon : "...";
      lv_obj_t* ic = label(b, icon, &lv_font_montserrat_16, color::text_dim);
      lv_obj_center(ic);
      if (i < kRailPages)
        lv_obj_add_event_cb(b, view_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);
      else
        lv_obj_add_event_cb(b, sheet_open, LV_EVENT_CLICKED, nullptr);
      rail_btns[i] = b;
      rail_icons[i] = ic;
    }

    header_title = label(scr, kPages[0].title, &lv_font_montserrat_16, color::text);
    lv_obj_set_pos(header_title, 8, 6);

    lv_obj_t* current = pill(scr, 128, color::pink_deep);
    lv_obj_set_pos(current, layout::screen_w - 134, 4);
    current_label = label(current, "no auton", &lv_font_montserrat_12, color::pink);
    lv_obj_center(current_label);
  } else {
    lv_obj_t* rail = panel(scr);
    lv_obj_set_size(rail, layout::rail_w, layout::screen_h);
    lv_obj_set_style_bg_color(rail, lv_color_hex(color::rail), 0);
    lv_obj_set_style_bg_opa(rail, LV_OPA_COVER, 0);

    lv_obj_t* logo = label(rail, "EZ", &lv_font_montserrat_18, color::pink);
    lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, 6);

    for (int i = 0; i <= kRailPages; i++) {
      lv_obj_t* b = panel(rail);
      lv_obj_set_size(b, 36, 36);
      lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 36 + i * 40);
      lv_obj_set_style_radius(b, 12, 0);
      lv_obj_set_style_bg_color(b, lv_color_hex(color::surface_hi), 0);
      lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
      const char* icon = i < kRailPages ? kPages[i].icon : "...";
      lv_obj_t* ic = label(b, icon, &lv_font_montserrat_16, color::text_dim);
      lv_obj_center(ic);
      if (i < kRailPages)
        lv_obj_add_event_cb(b, view_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);
      else
        lv_obj_add_event_cb(b, sheet_open, LV_EVENT_CLICKED, nullptr);
      rail_btns[i] = b;
      rail_icons[i] = ic;
    }

    header_title = label(scr, kPages[0].title, &lv_font_montserrat_18, color::text);
    lv_obj_set_pos(header_title, layout::page_x + 2, 7);

    lv_obj_t* current = pill(scr, 158, color::pink_deep);
    lv_obj_set_pos(current, 240, 6);
    current_label = label(current, "no auton", &lv_font_montserrat_12, color::pink);
    lv_obj_center(current_label);

    lv_obj_t* sd = pill(scr, 64, color::pink_deep);
    lv_obj_set_pos(sd, 406, 6);
    lv_obj_t* sd_txt = label(sd, LV_SYMBOL_SD_CARD " SD", &lv_font_montserrat_12,
                             state::sd_ok ? color::pink : color::text_dim);
    lv_obj_center(sd_txt);
  }

  for (int i = 0; i < kPageCount; i++) {
    page_objs[i] = panel(scr);
    lv_obj_set_size(page_objs[i], layout::page_w, layout::page_h);
    lv_obj_set_pos(page_objs[i], layout::page_x, layout::header_h);
    lv_obj_remove_flag(page_objs[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(page_objs[i], LV_OBJ_FLAG_HIDDEN);
    kPages[i].build(page_objs[i]);
  }

  view_show(0);
  lv_screen_load(scr);
  repaint_schedule();
}

}  // namespace

void shell_current_set(const char* text) {
  lv_label_set_text_fmt(current_label, LV_SYMBOL_OK "  %s", text);
}

// ---- public API ----

int auton_add(const char* name, const char* desc, int seconds, uint32_t color_, void (*fn)()) {
  state::Auton a;
  a.name = name;
  a.desc = desc;
  a.dur_s = seconds;
  a.color = color_;
  a.fn = fn;
  state::autons.push_back(a);
  return (int)state::autons.size() - 1;
}

void auton_setting_step(int auton, const char* name, int def, int mn, int mx, const char* unit) {
  if (auton < 0 || auton >= (int)state::autons.size()) return;
  state::autons[auton].settings.push_back({name, SType::STEP, def, mn, mx, unit, {}});
}

void auton_setting_enum(int auton, const char* name, std::vector<const char*> opts, int def) {
  if (auton < 0 || auton >= (int)state::autons.size()) return;
  state::autons[auton].settings.push_back({name, SType::ENUM, def, 0, (int)opts.size() - 1, "", opts});
}

void auton_setting_toggle(int auton, const char* name, bool def) {
  if (auton < 0 || auton >= (int)state::autons.size()) return;
  state::autons[auton].settings.push_back({name, SType::TOGGLE, def ? 1 : 0, 0, 1, "", {}});
}

void auton_run() {
  if (state::autons.empty()) return;
  const state::Auton& a = state::autons[state::selected_auton];
  log(color::pink, "auton", "running %s", a.name.c_str());
  if (a.fn) a.fn();
}

int setting_get(const char* name, int fallback) {
  if (state::autons.empty()) return fallback;
  for (auto& s : state::autons[state::selected_auton].settings)
    if (strcmp(s.name, name) == 0) return s.v;
  return fallback;
}

void port_add(const char* role, int port, bool reversed, int cartridge) {
  state::ports.push_back({role, port, reversed, cartridge, false});
}

void health_add(int port, const char* name, Dev type) {
  state::health_devs.push_back({name, port, (int)type});
}

void log(uint32_t color_, const char* tag, const char* fmt, ...) {
  char msg[64];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);
  state::log_push(color_, tag, msg);
}

void init(ez::Drive* chassis, int rotation) {
  state::chassis = chassis;

  if (rotation == 90 || rotation == 180 || rotation == 270) {
    ez::screen_rotation_set(rotation);
    layout::set_portrait(rotation == 90 || rotation == 270);
  }

  // Mirror autons registered the stock way so old projects render unchanged.
  auto& stock = ez::as::auton_selector.Autons;
  state::stock_auton_count = (int)stock.size();
  for (size_t i = 0; i < stock.size(); i++) {
    state::Auton a;
    std::string full = stock[i].Name;
    size_t split = full.find("\n\n");
    a.name = split == std::string::npos ? full : full.substr(0, split);
    a.desc = split == std::string::npos ? "" : full.substr(split + 2);
    for (auto& c : a.name)
      if (c == '\n') c = ' ';
    for (auto& c : a.desc)
      if (c == '\n') c = ' ';
    a.fn = stock[i].auton_call;
    a.stock_idx = (int)i;
    state::autons.push_back(a);
  }

  // Drive motors and the chassis IMU join the health page automatically.
  if (chassis != nullptr) {
    int n = 1;
    for (auto& m : chassis->left_motors) health_add(abs(m.get_port()), ("LEFT " + std::to_string(n++)).c_str());
    n = 1;
    for (auto& m : chassis->right_motors) health_add(abs(m.get_port()), ("RIGHT " + std::to_string(n++)).c_str());
    if (chassis->imu != nullptr) health_add(chassis->imu->get_port(), "IMU", Dev::IMU);
  }

  controls_locked = []() { return state::comp_locked(); };

  state::sd_load();
  ui_build();

  lv_timer_create([](lv_timer_t*) { health_poll(); }, 1000, nullptr);
  lv_timer_create([](lv_timer_t*) { console_poll(); }, 250, nullptr);

  log(color::pink, "ez", "EZ-Screen ready (%d autons)", (int)state::autons.size());
  if (!state::sd_ok) log(color::amber, "sd", "no sd card - settings won't persist");
}

}  // namespace ez_screen
