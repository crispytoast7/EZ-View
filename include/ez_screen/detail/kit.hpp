#pragma once

#include "liblvgl/lvgl.h"

#include <vector>

// Internal building blocks shared by the shell and the pages.
namespace ez_screen {

// ---- theme ----------------------------------------------------------------
namespace color {
constexpr uint32_t bg = 0x161618;
constexpr uint32_t rail = 0x0F0F10;
constexpr uint32_t surface = 0x232326;
constexpr uint32_t surface_hi = 0x323236;
constexpr uint32_t pink = 0xFF93D5;
constexpr uint32_t pink_deep = 0xFF63C2;
constexpr uint32_t text = 0xF2F2F2;
constexpr uint32_t text_dim = 0x9E9E9E;
constexpr uint32_t ink = 0x1A1A1C;  // on pink
constexpr uint32_t red = 0xE84855;
constexpr uint32_t blue = 0x4895EF;
constexpr uint32_t green = 0x2ECC71;
constexpr uint32_t amber = 0xF4B942;
}  // namespace color

// ---- layout ---------------------------------------------------------------
// PROS gives LVGL 480x240 in landscape. At rotation 90/270 the logical
// canvas is 240x480 and set_portrait() swaps the whole grid: the rail
// becomes a bottom tab bar and pages stack single-column.
namespace layout {
extern bool portrait;
extern int screen_w;
extern int screen_h;
extern int rail_w;    // 0 in portrait (bottom bar instead)
extern int bar_h;     // bottom tab bar height, portrait only
extern int header_h;
extern int page_x;
extern int page_w;
extern int page_h;
extern int col_x;  // landscape: right column of list+detail pages
extern int col_w;
void set_portrait(bool on);
}  // namespace layout

// ---- motion ---------------------------------------------------------------
// One easing (ease-out), two durations, one stagger step. Everything that
// moves uses these.
namespace motion {
constexpr int fast_ms = 120;
constexpr int base_ms = 180;
constexpr int step_ms = 24;
constexpr int slide_px = 14;
}  // namespace motion

void anim_slide_x(lv_obj_t* o, int32_t from, int32_t to, int ms);
void enter(lv_obj_t* o, int delay_ms = 0, int from = motion::slide_px);
void cascade(lv_obj_t* parent);  // enter() each child, staggered
void repaint_schedule();

// ---- widgets --------------------------------------------------------------
lv_obj_t* panel(lv_obj_t* parent);
lv_obj_t* label(lv_obj_t* parent, const char* txt, const lv_font_t* font, uint32_t c);
lv_obj_t* pill(lv_obj_t* parent, int w, uint32_t border);

// Selectable list rows (solid pink when active).
lv_obj_t* list_column(lv_obj_t* parent, int w, int h = -1);
lv_obj_t* list_row(lv_obj_t* parent, const char* name, const char* right, lv_event_cb_t cb, int idx);
void list_select(std::vector<lv_obj_t*>& rows, int sel);

// One editable value: stepper, enum cycler, or toggle.
enum class SType { STEP, ENUM, TOGGLE };
struct Setting {
  const char* name;
  SType t;
  int v;
  int mn, mx;
  const char* unit;
  std::vector<const char*> opts;
};

// control_row() builds "Name ......... [control]" and registers the widgets
// in the group so group_sync() can push model values back to the screen.
// on_change fires after any edit.
struct ControlGroup {
  struct Bound {
    Setting* s;
    lv_obj_t* val;
    lv_obj_t* thumb;
    lv_obj_t* off_lbl;
    lv_obj_t* on_lbl;
  };
  std::vector<Bound> items;
  void (*on_change)() = nullptr;
  void clear() { items.clear(); }
};

void control_row(lv_obj_t* parent, int y, Setting* s, ControlGroup& group);
void group_sync(ControlGroup& group);

// When set and returning true, every control refuses edits (match lockout).
extern bool (*controls_locked)();

}  // namespace ez_screen
