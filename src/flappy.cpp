#include "flappy.hpp"

#include "liblvgl/lvgl.h"
#include "pros/misc.hpp"
#include "pros/rtos.hpp"

#include <cmath>
#include <cstdlib>

namespace flappy {
namespace {

// ---------------------------------------------------------------------------
// Geometry. The V5 panel is 480x272, but VEXos owns the top strip for the
// program header, so liblvgl hands you 480x240. Everything below is in that
// space, y growing downward.
// ---------------------------------------------------------------------------
constexpr int kScreenW = 480;
constexpr int kScreenH = 240;

constexpr int kBirdX = 90;  // bird never moves horizontally
constexpr int kBirdSize = 18;

constexpr int kPipeW = 46;
constexpr int kGapH = 96;      // vertical opening
constexpr int kPipeCount = 3;  // pool size; 3 covers the screen with margin
constexpr int kPipeSpacing = 200;
constexpr int kGapMargin = 26;  // keep openings off the very top/bottom

// ---------------------------------------------------------------------------
// Physics, in pixels and seconds. These are tuned, not guessed: the thing that
// makes a flappy clone feel fair is the ratio of one flap's rise height to the
// free vertical band inside the gap (kGapH - kBirdSize = 78 px here).
//
//   rise = kFlapV^2 / (2 * kGravity) = 240^2 / 1800 = 32 px
//   ratio = 32 / 78 = 0.41
//
// At a ratio near 0.7 a single tap eats most of the gap and the game is
// miserable. At 0.55 it is twitchy. At or below ~0.45 it is controllable.
// If you change kGravity or kFlapV, re-check that ratio.
// ---------------------------------------------------------------------------
constexpr float kDt = 0.020f;       // 20 ms tick == 50 Hz
constexpr float kGravity = 900.0f;  // px/s^2
constexpr float kFlapV = -240.0f;   // px/s, instantaneous on tap
constexpr float kMaxFall = 540.0f;  // terminal velocity, px/s
constexpr float kScrollV = 125.0f;  // px/s the world moves left

// ---------------------------------------------------------------------------
// Animation. Tilt tracks vertical velocity, mapped so a fresh flap pitches
// the bird up ~25 deg and terminal velocity points it ~65 deg down — the
// classic flappy silhouette. The wing pops up for a few frames per flap, and
// on the ready screen the bird bobs on a slow sine.
// ---------------------------------------------------------------------------
constexpr float kTiltPerV = 0.12f;   // deg of pitch per px/s of velocity
constexpr float kTiltUpMax = -25.0f;
constexpr float kTiltDownMax = 65.0f;
constexpr int kWingFlapTicks = 6;    // frames the wing stays raised after a tap
constexpr float kBobAmp = 6.0f;      // px, ready-screen bob
constexpr float kBobSpeed = 0.10f;   // rad per tick

// ---------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------
constexpr uint32_t kSky = 0x1B2838;
constexpr uint32_t kPipe = 0x2ECC71;
constexpr uint32_t kBird = 0xF5C542;
constexpr uint32_t kWing = 0xD9A82F;
constexpr uint32_t kEye = 0x1B2838;
constexpr uint32_t kText = 0xFFFFFF;

enum class State { Ready, Playing, Dead };

struct Pipe {
  lv_obj_t* top = nullptr;
  lv_obj_t* bottom = nullptr;
  float x = 0.0f;
  int gap_y = 0;
  bool scored = false;
};

// All of this is touched only from LVGL's daemon task (the lv_timer callback
// and the press event callback both run there), so no mutex is needed. Do not
// poke at it from a pros::Task.
lv_timer_t* g_timer = nullptr;
lv_obj_t* g_root = nullptr;  // parent container owned by the caller
lv_obj_t* g_bird = nullptr;
lv_obj_t* g_wing = nullptr;
lv_obj_t* g_score_label = nullptr;
lv_obj_t* g_banner = nullptr;
Pipe g_pipes[kPipeCount];

State g_state = State::Ready;
float g_bird_y = 0.0f;
float g_bird_v = 0.0f;
int g_score = 0;
int g_high = 0;
bool g_tap = false;  // set by input, consumed by the tick
bool g_built = false;
uint32_t g_ticks = 0;
int g_wing_hold = 0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// A plain filled rectangle. LVGL base objects arrive with a border, padding,
// a radius and scrolling enabled, all of which you want gone for a sprite.
lv_obj_t* make_rect(lv_obj_t* parent, uint32_t color, int w, int h, int radius) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(o, 0, LV_PART_MAIN);
  lv_obj_set_style_outline_width(o, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(o, radius, LV_PART_MAIN);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  // Critical: if sprites stay clickable they swallow the touch and the root
  // never sees LV_EVENT_PRESSED.
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}

void bird_tilt_set(float deg) {
  // LVGL 9 transform rotation is in 0.1 degree units around the pivot.
  lv_obj_set_style_transform_rotation(g_bird, (int32_t)(deg * 10.0f), LV_PART_MAIN);
}

int random_gap_y() {
  const int lo = kGapMargin;
  const int hi = kScreenH - kGapH - kGapMargin;  // 118 with these constants
  return lo + (std::rand() % (hi - lo + 1));
}

void place_pipe(Pipe& p) {
  const int x = static_cast<int>(p.x);
  lv_obj_set_pos(p.top, x, 0);
  lv_obj_set_size(p.top, kPipeW, p.gap_y);
  lv_obj_set_pos(p.bottom, x, p.gap_y + kGapH);
  lv_obj_set_size(p.bottom, kPipeW, kScreenH - (p.gap_y + kGapH));
}

void reset_game() {
  g_bird_y = kScreenH / 2.0f - kBirdSize / 2.0f;
  g_bird_v = 0.0f;
  g_score = 0;
  g_wing_hold = 0;
  for (int i = 0; i < kPipeCount; i++) {
    g_pipes[i].x = static_cast<float>(kScreenW + i * kPipeSpacing);
    g_pipes[i].gap_y = random_gap_y();
    g_pipes[i].scored = false;
    place_pipe(g_pipes[i]);
  }
  lv_obj_set_pos(g_bird, kBirdX, static_cast<int>(g_bird_y));
  bird_tilt_set(0.0f);
  lv_obj_set_pos(g_wing, 2, 9);
  lv_label_set_text(g_score_label, "0");
  lv_label_set_text(g_banner, "TAP OR PRESS A");
  lv_obj_remove_flag(g_banner, LV_OBJ_FLAG_HIDDEN);
  g_state = State::Ready;
}

bool bird_hits(const Pipe& p) {
  const float bl = kBirdX;
  const float br = kBirdX + kBirdSize;
  const float pl = p.x;
  const float pr = p.x + kPipeW;
  if (br <= pl || bl >= pr) return false;                   // no horizontal overlap
  if (g_bird_y < p.gap_y) return true;                      // clipped the top pipe
  if (g_bird_y + kBirdSize > p.gap_y + kGapH) return true;  // clipped the bottom
  return false;
}

void wing_animate(bool flapped) {
  if (flapped) g_wing_hold = kWingFlapTicks;
  if (g_wing_hold > 0) {
    g_wing_hold--;
    lv_obj_set_pos(g_wing, 2, 5);  // raised
  } else {
    lv_obj_set_pos(g_wing, 2, 9);  // resting
  }
}

// ---------------------------------------------------------------------------
// The game loop. One lv_timer at 20 ms. Running the simulation on an lv_timer
// instead of a pros::Task is the whole trick: LVGL is NOT thread safe, and PROS
// already runs the lvgl handler on its own daemon. A pros::Task calling
// lv_obj_set_pos races that daemon and will eventually corrupt the display
// list. lv_timer callbacks execute inside the daemon, so they are safe by
// construction.
// ---------------------------------------------------------------------------
void tick(lv_timer_t* /*t*/) {
  static pros::Controller ctrl(pros::E_CONTROLLER_MASTER);
  g_ticks++;

  // Never call ctrl.set_text() in a loop like this. Constant controller writes
  // saturate the radio link.
  if (ctrl.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_A)) g_tap = true;

  const bool tapped = g_tap;
  g_tap = false;

  switch (g_state) {
    case State::Ready: {
      // Idle bob on a slow sine so the start screen feels alive.
      const float base = kScreenH / 2.0f - kBirdSize / 2.0f;
      lv_obj_set_pos(g_bird, kBirdX, (int)(base + kBobAmp * std::sin(g_ticks * kBobSpeed)));
      wing_animate(false);
      if (tapped) {
        g_state = State::Playing;
        g_bird_v = kFlapV;
        g_wing_hold = kWingFlapTicks;
        lv_obj_add_flag(g_banner, LV_OBJ_FLAG_HIDDEN);
      }
      return;
    }

    case State::Dead:
      if (tapped) reset_game();
      return;

    case State::Playing:
      break;
  }

  if (tapped) g_bird_v = kFlapV;
  wing_animate(tapped);

  g_bird_v += kGravity * kDt;
  if (g_bird_v > kMaxFall) g_bird_v = kMaxFall;
  g_bird_y += g_bird_v * kDt;

  bool dead = (g_bird_y < 0.0f) || (g_bird_y + kBirdSize > kScreenH);

  float rightmost = 0.0f;
  for (const Pipe& p : g_pipes) {
    if (p.x > rightmost) rightmost = p.x;
  }

  for (Pipe& p : g_pipes) {
    p.x -= kScrollV * kDt;

    if (!p.scored && p.x + kPipeW < kBirdX) {
      p.scored = true;
      g_score++;
      if (g_score > g_high) g_high = g_score;
      lv_label_set_text_fmt(g_score_label, "%d", g_score);
    }

    if (p.x < -kPipeW) {
      // Recycle off the rightmost pipe, not off the screen edge, so spacing
      // stays exactly kPipeSpacing no matter how far x overshot this frame.
      rightmost += kPipeSpacing;
      p.x = rightmost;
      p.gap_y = random_gap_y();
      p.scored = false;
    }

    if (!dead && bird_hits(p)) dead = true;
    place_pipe(p);
  }

  if (g_bird_y < 0.0f) g_bird_y = 0.0f;
  if (g_bird_y + kBirdSize > kScreenH) g_bird_y = kScreenH - kBirdSize;
  lv_obj_set_pos(g_bird, kBirdX, static_cast<int>(g_bird_y));

  // Pitch follows velocity: flaps point up, dives point down.
  float tilt = g_bird_v * kTiltPerV;
  if (tilt < kTiltUpMax) tilt = kTiltUpMax;
  if (tilt > kTiltDownMax) tilt = kTiltDownMax;
  bird_tilt_set(tilt);

  if (dead) {
    g_state = State::Dead;
    bird_tilt_set(kTiltDownMax);  // nose-down for the crash pose
    lv_label_set_text_fmt(g_banner, "SCORE %d   BEST %d\nTAP TO RETRY", g_score, g_high);
    lv_obj_remove_flag(g_banner, LV_OBJ_FLAG_HIDDEN);
  }
}

void on_press(lv_event_t* /*e*/) { g_tap = true; }

void build_ui(lv_obj_t* parent) {
  g_root = parent;
  lv_obj_set_style_bg_color(g_root, lv_color_hex(kSky), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_root, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_remove_flag(g_root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(g_root, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_root, on_press, LV_EVENT_PRESSED, nullptr);

  for (Pipe& p : g_pipes) {
    p.top = make_rect(g_root, kPipe, kPipeW, 10, 4);
    p.bottom = make_rect(g_root, kPipe, kPipeW, 10, 4);
  }

  g_bird = make_rect(g_root, kBird, kBirdSize, kBirdSize, 5);
  // Rotate around the sprite's center, not its top-left corner.
  lv_obj_set_style_transform_pivot_x(g_bird, kBirdSize / 2, LV_PART_MAIN);
  lv_obj_set_style_transform_pivot_y(g_bird, kBirdSize / 2, LV_PART_MAIN);
  lv_obj_t* eye = make_rect(g_bird, kEye, 4, 4, 2);
  lv_obj_set_pos(eye, 11, 4);
  g_wing = make_rect(g_bird, kWing, 9, 5, 2);
  lv_obj_set_pos(g_wing, 2, 9);

  g_score_label = lv_label_create(g_root);
  lv_obj_set_style_text_color(g_score_label, lv_color_hex(kText), LV_PART_MAIN);
  lv_obj_set_style_text_font(g_score_label, &lv_font_montserrat_24, LV_PART_MAIN);
  lv_label_set_text(g_score_label, "0");
  lv_obj_align(g_score_label, LV_ALIGN_TOP_MID, 0, 12);

  g_banner = lv_label_create(g_root);
  lv_obj_set_style_text_color(g_banner, lv_color_hex(kText), LV_PART_MAIN);
  lv_obj_set_style_text_align(g_banner, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(g_banner, "TAP OR PRESS A");
  lv_obj_align(g_banner, LV_ALIGN_CENTER, 0, 60);

  g_built = true;
}

}  // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void start(lv_obj_t* parent) {
  if (g_timer != nullptr) return;
  std::srand(static_cast<unsigned>(pros::millis()));
  if (!g_built) build_ui(parent);
  reset_game();
  g_timer = lv_timer_create(tick, static_cast<uint32_t>(kDt * 1000.0f), nullptr);
}

void stop() {
  if (g_timer != nullptr) {
    lv_timer_delete(g_timer);
    g_timer = nullptr;
  }
  if (g_built) {
    // The caller owns g_root and deletes it (which also drops on_press);
    // just forget our widgets so a relaunch rebuilds from scratch.
    g_bird = g_wing = g_score_label = g_banner = nullptr;
    for (Pipe& p : g_pipes) {
      p.top = nullptr;
      p.bottom = nullptr;
    }
    g_root = nullptr;
    g_built = false;
  }
  g_state = State::Ready;
}

int high_score() { return g_high; }

}  // namespace flappy
