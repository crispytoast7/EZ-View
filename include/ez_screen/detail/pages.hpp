#pragma once

#include "liblvgl/lvgl.h"

// The shell owns the rail, header, and page switching; each page owns its
// content. build() runs once at startup, on_show() every time the page is
// brought forward.
namespace ez_screen {

struct PageDef {
  const char* icon;   // rail / sheet icon
  const char* title;  // header text
  void (*build)(lv_obj_t* page);
  void (*on_show)();
};

// 0-3 sit on the rail, the rest live in the "..." sheet (plus Flappy Bird).
constexpr int kRailPages = 4;
constexpr int kPageCount = 5;
extern const PageDef kPages[kPageCount];

// Periodic refresh hooks, driven by the shell's lv_timers.
void health_poll();   // ~1 s: motor temps / presence
void console_poll();  // ~250 ms: drain the log ring into the page

// Header chip showing the auton that would run; owned by the shell,
// written by the auton page.
void shell_current_set(const char* text);

}  // namespace ez_screen
