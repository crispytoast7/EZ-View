#pragma once

#include "liblvgl/lvgl.h"

// Flappy bird mini-game, robodash-style "app". Builds into the parent
// container you hand it and simulates on an lv_timer (LVGL's daemon task),
// never a pros::Task — LVGL is not thread safe.
namespace flappy {

void start(lv_obj_t* parent);  // build + run inside parent (480x240)
void stop();                   // kill the timer and the game's widgets
int high_score();

}  // namespace flappy
