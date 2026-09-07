#include "ez_screen/detail/kit.hpp"

namespace ez_screen {

namespace layout {
bool portrait = false;
int screen_w = 480;
int screen_h = 240;
int rail_w = 46;
int bar_h = 0;
int header_h = 34;
int page_x = 56;
int page_w = 416;
int page_h = 206;
int col_x = 162;
int col_w = 254;

void set_portrait(bool on) {
  portrait = on;
  if (on) {
    screen_w = 240;
    screen_h = 480;
    rail_w = 0;
    bar_h = 44;
    header_h = 30;
    page_x = 8;
    page_w = 224;
    page_h = screen_h - header_h - bar_h - 2;  // 404
    col_x = 0;
    col_w = page_w;
  } else {
    screen_w = 480;
    screen_h = 240;
    rail_w = 46;
    bar_h = 0;
    header_h = 34;
    page_x = 56;
    page_w = 416;
    page_h = screen_h - header_h;
    col_x = 162;
    col_w = page_w - col_x;
  }
}
}  // namespace layout

namespace {

void exec_ty(void* obj, int32_t v) { lv_obj_set_style_translate_y((lv_obj_t*)obj, v, 0); }
void exec_tx(void* obj, int32_t v) { lv_obj_set_style_translate_x((lv_obj_t*)obj, v, 0); }
void exec_opa(void* obj, int32_t v) { lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0); }

lv_anim_t anim_base(lv_obj_t* o, int ms, int delay_ms) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, o);
  lv_anim_set_duration(&a, ms);
  lv_anim_set_delay(&a, delay_ms);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  return a;
}

}  // namespace

bool (*controls_locked)() = nullptr;

void anim_slide_x(lv_obj_t* o, int32_t from, int32_t to, int ms) {
  lv_anim_t a = anim_base(o, ms, 0);
  lv_anim_set_values(&a, from, to);
  lv_anim_set_exec_cb(&a, exec_tx);
  lv_anim_start(&a);
}

void enter(lv_obj_t* o, int delay_ms, int from) {
  lv_obj_set_style_translate_y(o, from, 0);
  lv_obj_set_style_opa(o, LV_OPA_TRANSP, 0);
  lv_anim_t a = anim_base(o, motion::base_ms, delay_ms);
  lv_anim_set_values(&a, from, 0);
  lv_anim_set_exec_cb(&a, exec_ty);
  lv_anim_start(&a);
  lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
  lv_anim_set_exec_cb(&a, exec_opa);
  lv_anim_start(&a);
}

void cascade(lv_obj_t* parent) {
  uint32_t n = lv_obj_get_child_count(parent);
  for (uint32_t i = 0; i < n; i++) enter(lv_obj_get_child(parent, (int32_t)i), (int)i * motion::step_ms);
}

// A page loaded from a user task can race the display daemon's flush and
// leave stale bands; one extra invalidate from an lv_timer lands on the
// daemon's own task after the dust settles.
void repaint_schedule() {
  lv_timer_t* t = lv_timer_create([](lv_timer_t*) { lv_obj_invalidate(lv_screen_active()); }, 80, nullptr);
  lv_timer_set_repeat_count(t, 1);
}

lv_obj_t* panel(lv_obj_t* parent) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  return o;
}

lv_obj_t* label(lv_obj_t* parent, const char* txt, const lv_font_t* font, uint32_t c) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(c), 0);
  return l;
}

lv_obj_t* pill(lv_obj_t* parent, int w, uint32_t border) {
  lv_obj_t* o = panel(parent);
  lv_obj_set_size(o, w, 22);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(o, 1, 0);
  lv_obj_set_style_border_color(o, lv_color_hex(border), 0);
  return o;
}

lv_obj_t* list_column(lv_obj_t* parent, int w, int h) {
  lv_obj_t* list = panel(parent);
  lv_obj_set_size(list, w, h > 0 ? h : layout::page_h - 10);
  lv_obj_set_pos(list, 0, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 5, 0);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  return list;
}

lv_obj_t* list_row(lv_obj_t* parent, const char* name, const char* right, lv_event_cb_t cb, int idx) {
  lv_obj_t* row = panel(parent);
  lv_obj_set_size(row, lv_pct(100), 34);
  lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(row, lv_color_hex(color::surface), 0);
  lv_obj_set_style_radius(row, 10, 0);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, (void*)(intptr_t)idx);
  lv_obj_t* n = label(row, name, &lv_font_montserrat_14, color::text);
  lv_obj_align(n, LV_ALIGN_LEFT_MID, 10, 0);
  if (right != nullptr) {
    lv_obj_t* r = label(row, right, &lv_font_montserrat_12, color::text_dim);
    lv_obj_align(r, LV_ALIGN_RIGHT_MID, -10, 0);
  }
  return row;
}

void list_select(std::vector<lv_obj_t*>& rows, int sel) {
  for (size_t i = 0; i < rows.size(); i++) {
    bool s = (int)i == sel;
    lv_obj_set_style_bg_color(rows[i], lv_color_hex(s ? color::pink : color::surface), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(rows[i], 0), lv_color_hex(s ? color::ink : color::text), 0);
    if (lv_obj_get_child(rows[i], 1) != nullptr)
      lv_obj_set_style_text_color(lv_obj_get_child(rows[i], 1), lv_color_hex(s ? color::ink : color::text_dim), 0);
  }
}

namespace {

struct StepCtx {
  Setting* s;
  ControlGroup* g;
};

void step_cb(lv_event_t* e) {
  if (controls_locked != nullptr && controls_locked()) return;
  auto* ctx = (StepCtx*)lv_event_get_user_data(e);
  int dir = (int)(intptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_target(e));
  Setting* s = ctx->s;
  if (s->t == SType::ENUM) {
    int n = (int)s->opts.size();
    s->v = (s->v + dir + n) % n;
  } else {
    s->v += dir;
    if (s->v < s->mn) s->v = s->mn;
    if (s->v > s->mx) s->v = s->mx;
  }
  group_sync(*ctx->g);
  if (ctx->g->on_change) ctx->g->on_change();
}

void toggle_cb(lv_event_t* e) {
  if (controls_locked != nullptr && controls_locked()) return;
  auto* ctx = (StepCtx*)lv_event_get_user_data(e);
  ctx->s->v = !ctx->s->v;
  group_sync(*ctx->g);
  if (ctx->g->on_change) ctx->g->on_change();
}

// Contexts must outlive the widgets; rows are rebuilt but never freed
// mid-session, so a growing arena is fine for a mockup.
std::vector<StepCtx*> ctx_arena;

StepCtx* ctx_make(Setting* s, ControlGroup* g) {
  ctx_arena.push_back(new StepCtx{s, g});
  return ctx_arena.back();
}

}  // namespace

void control_row(lv_obj_t* parent, int y, Setting* s, ControlGroup& group) {
  lv_obj_t* n = label(parent, s->name, &lv_font_montserrat_14, color::text);
  lv_obj_set_pos(n, 0, y + 5);

  ControlGroup::Bound bv{s, nullptr, nullptr, nullptr, nullptr};
  StepCtx* ctx = ctx_make(s, &group);

  if (s->t == SType::TOGGLE) {
    lv_obj_t* track = panel(parent);
    lv_obj_set_size(track, 92, 26);
    lv_obj_align(track, LV_ALIGN_TOP_RIGHT, 0, y);
    lv_obj_set_style_radius(track, 13, 0);
    lv_obj_set_style_bg_color(track, lv_color_hex(color::surface_hi), 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_add_flag(track, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(track, toggle_cb, LV_EVENT_CLICKED, ctx);

    bv.thumb = panel(track);
    lv_obj_set_size(bv.thumb, 46, 26);
    lv_obj_align(bv.thumb, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_radius(bv.thumb, 13, 0);
    lv_obj_set_style_bg_color(bv.thumb, lv_color_hex(color::pink), 0);
    lv_obj_set_style_bg_opa(bv.thumb, LV_OPA_COVER, 0);

    bv.off_lbl = label(track, "OFF", &lv_font_montserrat_12, color::text_dim);
    lv_obj_align(bv.off_lbl, LV_ALIGN_LEFT_MID, 10, 0);
    bv.on_lbl = label(track, "ON", &lv_font_montserrat_12, color::text_dim);
    lv_obj_align(bv.on_lbl, LV_ALIGN_RIGHT_MID, -12, 0);
  } else {
    lv_obj_t* grp = panel(parent);
    lv_obj_set_size(grp, 118, 26);
    lv_obj_align(grp, LV_ALIGN_TOP_RIGHT, 0, y);
    lv_obj_set_style_radius(grp, 13, 0);
    lv_obj_set_style_border_width(grp, 1, 0);
    lv_obj_set_style_border_color(grp, lv_color_hex(color::surface_hi), 0);

    const char* lt = s->t == SType::ENUM ? "<" : "-";
    const char* rt = s->t == SType::ENUM ? ">" : "+";
    for (int d = 0; d < 2; d++) {
      lv_obj_t* b = panel(grp);
      lv_obj_set_size(b, 30, 26);
      if (d) lv_obj_align(b, LV_ALIGN_RIGHT_MID, 0, 0);
      lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_user_data(b, (void*)(intptr_t)(d ? 1 : -1));
      lv_obj_add_event_cb(b, step_cb, LV_EVENT_CLICKED, ctx);
      lv_obj_t* bl = label(b, d ? rt : lt, &lv_font_montserrat_14, color::text_dim);
      lv_obj_center(bl);
    }
    bv.val = label(grp, "", &lv_font_montserrat_14, color::text);
    lv_obj_center(bv.val);
  }

  lv_obj_t* line = panel(parent);
  lv_obj_set_size(line, lv_pct(100), 1);
  lv_obj_set_pos(line, 0, y + 30);
  lv_obj_set_style_bg_color(line, lv_color_hex(color::surface_hi), 0);
  lv_obj_set_style_bg_opa(line, LV_OPA_50, 0);

  group.items.push_back(bv);
}

void group_sync(ControlGroup& group) {
  for (auto& b : group.items) {
    if (b.s->t == SType::TOGGLE) {
      int32_t cur = lv_obj_get_style_translate_x(b.thumb, 0);
      int32_t tgt = b.s->v ? 46 : 0;
      if (cur != tgt) anim_slide_x(b.thumb, cur, tgt, motion::fast_ms);
      lv_obj_set_style_text_color(b.off_lbl, lv_color_hex(b.s->v ? color::text_dim : color::ink), 0);
      lv_obj_set_style_text_color(b.on_lbl, lv_color_hex(b.s->v ? color::ink : color::text_dim), 0);
    } else if (b.s->t == SType::ENUM) {
      lv_label_set_text(b.val, b.s->opts[b.s->v]);
    } else {
      lv_label_set_text_fmt(b.val, "%d%s", b.s->v, b.s->unit);
    }
  }
}

}  // namespace ez_screen
