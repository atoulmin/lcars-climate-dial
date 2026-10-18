// LCARS air conditioner dial for the 240x240 round ONX2424G013.
// The whole screen is built and animated here; the YAML only feeds in state.
#pragma once
#include "lvgl.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>
#include <string>
#include <cctype>

namespace lcars_ac {

// LCARS palette
static const uint32_t ORANGE = 0xFF9900, AMBER = 0xF08A24, TAN = 0xCC7A2E, PEACH = 0xFFCC99, GOLD = 0xFFAA00,
                      RED = 0xCC4444, LILAC = 0xCC99FF, VIOLET = 0x9977DD, SKY = 0x66BBEE, DEEP_BLUE = 0x0E2236, DIM = 0x3A3344,
                      UNLIT = 0x1C1A24;

// LVGL angles: 0 = east, clockwise. The set temperature gauge is a horseshoe open at the bottom.
constexpr int GAUGE_START = 125;
constexpr int GAUGE_GAP = 4;
constexpr int GAUGE_BLOCKS = 8;
static const int GAUGE_SPAN[GAUGE_BLOCKS] = {30, 34, 30, 38, 38, 30, 34, 28};

// Fan speed bar
constexpr int FAN_BAR_W = 140, FAN_BAR_H = 20, FAN_BAR_GAP = 3;

struct Fonts {
  const lv_font_t *big, *med, *huge, *label, *fan;
};

enum Action : uint8_t { IDLE, HEATING, COOLING, OTHER };

// What the rest of the firmware tells the UI each frame
struct Input {
  float set_temp = 22, cur_temp = NAN;  // degrees; cur_temp NAN = unknown
  float min_temp = 16, max_temp = 32;
  bool on = false;                       // air con running
  Action action = IDLE;
  const char *mode = "AUTO";             // shown when idle
  bool adjusting = false;
  bool linked = true;                    // connected to Home Assistant
  bool fan = false;                      // fan speed page open
};

// Everything that just fades between its colour and dim with the lights
enum Part : uint8_t { ARC_MAIN, ARC_IND, BG, TEXT };
struct Fade {
  lv_obj_t *obj;
  Part part;
  uint32_t color;
};

struct Ui {
  std::vector<Fade> fades;
  lv_obj_t *gauge[GAUGE_BLOCKS];
  int gauge_start[GAUGE_BLOCKS];
  int gauge_sweep = 0;
  lv_color_t gauge_color[GAUGE_BLOCKS];
  lv_obj_t *status, *underline, *num, *now_lbl, *marker, *track, *sleep_layer, *sleep_lbl;
  bool asleep = false;
  float sleep_last = -999;
  float shown_cur = -1;
  lv_color_t c_track;
  float last_set = -999, last_cur = -999;
  int last_marker = -999;
  // Last colours set, so styles are only touched (and redrawn) when they change
  lv_color_t c_num, c_button, c_status, c_underline, c_scan[7];
  lv_obj_t *button, *icon_ring, *icon_bar;
  // Fan speed page (long press)
  lv_obj_t *fan_layer = nullptr, *fan_name = nullptr, *fan_bar_box = nullptr, *fan_lo = nullptr, *fan_hi = nullptr;
  lv_obj_t *fan_cap[2] = {};  // rounded LCARS end caps of the bar
  std::vector<lv_obj_t *> fan_bars;
  int fan_shown = -2;
  static constexpr int SCAN = 7;
  lv_obj_t *scan[SCAN];
  float scan_pos = 0, scan_level[SCAN] = {};
  float shown = 0, power = 0, last_power = -1, pulse = 0;
  uint32_t frame = 0;
  int last_num = -999;
  int boot_frames = 45;

  // Start-up build animation
  struct Reveal { lv_obj_t *obj; int at; bool flash; };
  std::vector<Reveal> reveals;
  std::vector<lv_obj_t *> flashing;
  int flash_left = 0;
  lv_obj_t *scanline = nullptr;
  bool booting = false;
  int boot_t = 0;
};

static Ui ui;

static lv_color_t ramp(float t) {
  // Temperature ramp: cold blue -> ice -> peach -> orange -> hot red
  static const uint32_t stops[] = {0x3355CC, 0x55AAEE, 0xAADDEE, PEACH, 0xFF8833, 0xDD3322};
  constexpr int n = sizeof(stops) / sizeof(stops[0]);
  if (t <= 0) return lv_color_hex(stops[0]);
  if (t >= 1) return lv_color_hex(stops[n - 1]);
  float p = t * (n - 1);
  int i = (int) p;
  uint8_t mix = (uint8_t) ((p - i) * 255);
  return lv_color_mix(lv_color_hex(stops[i + 1]), lv_color_hex(stops[i]), mix);
}

static lv_color_t dimmed(uint32_t c, uint8_t power) { return lv_color_mix(lv_color_hex(c), lv_color_hex(DIM), power); }

static lv_obj_t *plain(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  return o;
}

// An arc band of the given outer diameter and width between two angles
static lv_obj_t *band(lv_obj_t *parent, int size, int width, int from, int to, uint32_t color, bool fades = true) {
  lv_obj_t *a = lv_arc_create(parent);
  lv_obj_remove_style(a, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(a, size, size);
  lv_obj_center(a);
  lv_obj_set_style_pad_all(a, 0, 0);
  lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(a, 0, 0);
  for (auto part : {LV_PART_MAIN, LV_PART_INDICATOR}) {
    lv_obj_set_style_arc_width(a, width, part);
    lv_obj_set_style_arc_rounded(a, false, part);
  }
  lv_obj_set_style_arc_color(a, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_INDICATOR);
  lv_arc_set_bg_angles(a, from, to);
  if (fades) ui.fades.push_back({a, ARC_MAIN, color});
  return a;
}

static lv_obj_t *rect(lv_obj_t *parent, int w, int h, int x, int y, uint32_t color, int radius = 0) {
  lv_obj_t *r = plain(parent);
  lv_obj_set_size(r, w, h);
  lv_obj_align(r, LV_ALIGN_CENTER, x, y);
  lv_obj_set_style_radius(r, radius, 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(r, lv_color_hex(color), 0);
  ui.fades.push_back({r, BG, color});
  return r;
}

static lv_obj_t *text(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int x, int y, const char *t,
                      bool fades = true) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_label_set_text(l, t);
  lv_obj_align(l, LV_ALIGN_CENTER, x, y);
  if (fades) ui.fades.push_back({l, TEXT, color});
  return l;
}

// Text turned to run along a vertical band; angle in 0.1 deg
static lv_obj_t *vertical_text(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int x, const char *t,
                               int angle) {
  lv_obj_t *l = text(parent, font, color, x, 0, t);
  lv_obj_set_style_text_letter_space(l, 2, 0);
  lv_obj_update_layout(l);
  lv_obj_set_style_transform_pivot_x(l, lv_obj_get_width(l) / 2, 0);
  lv_obj_set_style_transform_pivot_y(l, lv_obj_get_height(l) / 2, 0);
  lv_obj_set_style_transform_rotation(l, angle, 0);
  return l;
}

static void polar(int r, float deg, int &x, int &y);

// Text bent round the dial: one turned label per letter, centred on `center` degrees.
// tops_out: reads clockwise with letter tops facing out (for the top and sides);
// otherwise reads anticlockwise with tops facing in, so text on the bottom stays upright.
// The letters' visual middle sits on `radius`. (LVGL's arclabel garbles ESPHome's
// font bitmaps, so it's done by hand.) Returns how many labels were written to `out`.
static int curved_text(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int radius, float center,
                       const char *t, int spacing, bool tops_out, lv_obj_t **out = nullptr) {
  int n = strlen(t);
  if (n > 32) n = 32;
  float widths[32];
  float total = 0;
  for (int i = 0; i < n; i++) {
    widths[i] = lv_font_get_glyph_width(font, t[i], t[i + 1]) + (i < n - 1 ? spacing : 0);
    total += widths[i];
  }

  // Offset from the label box middle to the visual middle of capitals/digits
  lv_font_glyph_dsc_t g;
  lv_font_get_glyph_dsc(font, &g, 'B', 0);
  int line_h = lv_font_get_line_height(font);
  float baseline = line_h - font->base_line;
  float drop = (baseline - g.ofs_y - g.box_h / 2.0f) - line_h / 2.0f;  // >0: glyphs sit low in the box
  float r = tops_out ? radius + drop : radius - drop;

  float deg_per_px = 180.0f / ((float) M_PI * r);
  float dir = tops_out ? 1 : -1;
  float ang = center - dir * total * deg_per_px / 2.0f;
  for (int i = 0; i < n; i++) {
    float glyph = widths[i] - (i < n - 1 ? spacing : 0);
    float mid = ang + dir * glyph / 2.0f * deg_per_px;
    char ch[2] = {t[i], 0};
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, ch);
    lv_obj_update_layout(l);
    int w = lv_obj_get_width(l), h = lv_obj_get_height(l);
    int x, y;
    polar((int) lroundf(r), mid, x, y);
    lv_obj_align(l, LV_ALIGN_CENTER, x, y);
    lv_obj_set_style_transform_pivot_x(l, w / 2, 0);
    lv_obj_set_style_transform_pivot_y(l, h / 2, 0);
    lv_obj_set_style_transform_rotation(l, (int) lroundf((mid + (tops_out ? 90 : -90)) * 10), 0);
    if (out) out[i] = l;
    ang += dir * widths[i] * deg_per_px;
  }
  return n;
}

static void polar(int r, float deg, int &x, int &y) {
  float a = deg * (float) M_PI / 180.0f;
  x = (int) lroundf(r * cosf(a));
  y = (int) lroundf(r * sinf(a));
}

inline void build(lv_obj_t *parent, const Fonts &f) {
  lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

  // Double LCARS frame, broken at the top for the status text. Ring B (inner)
  // carries the TEMPERATURE label and the scanner blocks.
  struct Seg { int from, to; uint32_t color; };
  const Seg ring_a[] = {
      {198, 248, 0xFFCC66}, {292, 340, 0x9999FF}, {150, 194, 0xCC6666}, {344, 30, ORANGE},
      {104, 146, PEACH},    {34, 76, 0xCC9966},   {80, 100, 0x9999FF},
  };
  std::vector<std::pair<int, lv_obj_t *>> outer, inner;  // (order key, object) for the start-up build
  for (auto &s : ring_a) outer.push_back({(s.from - 270 + 360) % 360, band(parent, 238, 15, s.from, s.to, s.color)});

  const Seg ring_b[] = {
      {148, 212, ORANGE}, {216, 244, LILAC}, {296, 324, PEACH},
      {36, 70, VIOLET},  {110, 144, VIOLET}, {74, 106, AMBER},
  };
  for (auto &s : ring_b) inner.push_back({(270 - s.from + 360) % 360, band(parent, 204, 16, s.from, s.to, s.color)});

  // Right side: a column of blocks a light sweeps up and down, LCARS sensor style
  // Each block's ring sits in a small clipping box around it, so recolouring a block
  // only redraws that box rather than the ring's whole screen-sized bounding box
  for (int i = 0; i < Ui::SCAN; i++) {
    int from = 328 + i * 9;
    int cx, cy;
    polar(94, from + 3.5f, cx, cy);
    lv_obj_t *clip = plain(parent);
    lv_obj_set_size(clip, 26, 26);
    lv_obj_align(clip, LV_ALIGN_CENTER, cx, cy);
    lv_obj_t *a = band(clip, 204, 16, from, from + 7, 0x1E2A3A, false);
    lv_obj_align(a, LV_ALIGN_CENTER, -cx, -cy);
    ui.scan[i] = a;
    ui.c_scan[i] = lv_color_hex(0x1E2A3A);
    inner.push_back({(270 - from + 360) % 360, clip});
  }

  // Labels running round the circle, black on their bands (tops facing out)
  lv_obj_t *letters[32];
  int n_letters = curved_text(parent, f.label, 0x000000, 94, 180, "TEMPERATURE", 1, true, letters);

  // Status text in the top gap with its underline bar
  ui.status = text(parent, f.med, SKY, 0, -105, "AC OFF", false);
  ui.underline = rect(parent, 44, 5, 0, -92, ORANGE);
  ui.fades.pop_back();  // animated separately

  // Set temperature gauge: thick horseshoe of blocks that fill along the cold-to-hot ramp
  int at = 0;
  for (int i = 0; i < GAUGE_BLOCKS; i++) at += GAUGE_SPAN[i] + (i ? GAUGE_GAP : 0);
  ui.gauge_sweep = at;
  at = 0;
  for (int i = 0; i < GAUGE_BLOCKS; i++) {
    ui.gauge_start[i] = at;
    ui.gauge_color[i] = ramp((at + GAUGE_SPAN[i] / 2.0f) / ui.gauge_sweep);
    lv_obj_t *a = band(parent, 148, 16, 0, GAUGE_SPAN[i], UNLIT, false);
    lv_arc_set_rotation(a, GAUGE_START + at);
    lv_arc_set_range(a, 0, 100);
    lv_arc_set_value(a, 0);
    lv_obj_set_style_arc_opa(a, LV_OPA_COVER, LV_PART_INDICATOR);
    ui.gauge[i] = a;
    at += GAUGE_SPAN[i] + GAUGE_GAP;
  }

  // Inner track: fills to the room temperature, with a white tip marking it
  ui.track = band(parent, 110, 5, 0, ui.gauge_sweep, UNLIT, false);
  lv_arc_set_rotation(ui.track, GAUGE_START);
  lv_arc_set_range(ui.track, 0, 1000);
  lv_arc_set_value(ui.track, 0);
  lv_obj_set_style_arc_opa(ui.track, LV_OPA_COVER, LV_PART_INDICATOR);

  ui.marker = band(parent, 114, 9, 0, 360, 0x000000, false);
  lv_obj_set_style_arc_opa(ui.marker, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(ui.marker, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(ui.marker, lv_color_white(), LV_PART_INDICATOR);
  lv_arc_set_angles(ui.marker, 0, 0);

  // Power button inside the horseshoe opening (pressing the knob works it)
  ui.button = rect(parent, 50, 24, 0, 58, ORANGE, 12);
  ui.fades.pop_back();  // animated separately
  ui.icon_ring = band(ui.button, 16, 3, 305, 235, 0x000000, false);
  lv_obj_set_style_arc_rounded(ui.icon_ring, true, LV_PART_MAIN);
  ui.icon_bar = plain(ui.button);
  lv_obj_set_size(ui.icon_bar, 3, 9);
  lv_obj_align(ui.icon_bar, LV_ALIGN_CENTER, 0, -4);
  lv_obj_set_style_radius(ui.icon_bar, 1, 0);
  lv_obj_set_style_bg_opa(ui.icon_bar, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(ui.icon_bar, lv_color_black(), 0);

  // Set temperature readout, with the room temperature under it
  ui.num = text(parent, f.big, PEACH, 0, -8, "--", false);
  ui.now_lbl = text(parent, f.med, LILAC, 0, 28, "--");
  lv_obj_set_style_text_letter_space(ui.now_lbl, 1, 0);

  // Start-up build order: outer frame clockwise from the top (flashing in), inner ring
  // and scanner anticlockwise, the label letter by letter, then the gauge and readouts
  std::sort(outer.begin(), outer.end(), [](auto &a, auto &b) { return a.first < b.first; });
  std::sort(inner.begin(), inner.end(), [](auto &a, auto &b) { return a.first < b.first; });
  for (size_t i = 0; i < outer.size(); i++) ui.reveals.push_back({outer[i].second, 4 + (int) i * 3, true});
  for (size_t i = 0; i < inner.size(); i++) ui.reveals.push_back({inner[i].second, 12 + (int) i, true});
  for (int i = 0; i < n_letters; i++) ui.reveals.push_back({letters[i], 24 + i, false});
  ui.reveals.push_back({ui.status, 6, false});
  ui.reveals.push_back({ui.underline, 20, false});
  for (int i = 0; i < GAUGE_BLOCKS; i++) ui.reveals.push_back({ui.gauge[i], 32 + i * 2, false});
  ui.reveals.push_back({ui.num, 40, false});
  ui.reveals.push_back({ui.now_lbl, 44, false});
  ui.reveals.push_back({ui.track, 46, false});
  ui.reveals.push_back({ui.marker, 48, false});
  ui.reveals.push_back({ui.button, 50, false});

  // The scan line that opens the start-up
  ui.scanline = plain(parent);
  lv_obj_set_size(ui.scanline, 0, 3);
  lv_obj_center(ui.scanline);
  lv_obj_set_style_bg_opa(ui.scanline, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(ui.scanline, lv_color_hex(PEACH), 0);
  lv_obj_add_flag(ui.scanline, LV_OBJ_FLAG_HIDDEN);

  // Fan speed page: covers the inside of the inner ring while it's open
  ui.fan_layer = plain(parent);
  lv_obj_set_size(ui.fan_layer, 172, 172);
  lv_obj_center(ui.fan_layer);
  lv_obj_set_style_radius(ui.fan_layer, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(ui.fan_layer, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(ui.fan_layer, lv_color_black(), 0);
  ui.fan_name = text(ui.fan_layer, f.fan, PEACH, 0, -26, "", false);
  // One LCARS bar: rounded end caps, a segment per fan speed in between
  ui.fan_bar_box = plain(ui.fan_layer);
  lv_obj_set_size(ui.fan_bar_box, FAN_BAR_W, FAN_BAR_H);
  lv_obj_align(ui.fan_bar_box, LV_ALIGN_CENTER, 0, 16);
  for (int k = 0; k < 2; k++) {
    lv_obj_t *c = plain(ui.fan_bar_box);
    lv_obj_set_size(c, FAN_BAR_H, FAN_BAR_H);
    lv_obj_set_pos(c, k ? FAN_BAR_W - FAN_BAR_H : 0, 0);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    ui.fan_cap[k] = c;
  }
  ui.fan_lo = text(ui.fan_layer, f.label, LILAC, -44, 38, "", false);
  ui.fan_hi = text(ui.fan_layer, f.label, LILAC, 44, 38, "", false);
  lv_obj_t *hint = text(ui.fan_layer, f.label, 0x7766AA, 0, 60, "TURN TO ADJUST", false);
  lv_obj_set_style_text_letter_space(hint, 1, 0);
  lv_obj_add_flag(ui.fan_layer, LV_OBJ_FLAG_HIDDEN);

  // Sleep screen: just the room temperature, large, on black. Drawn over everything.
  ui.sleep_layer = plain(parent);
  lv_obj_set_size(ui.sleep_layer, 240, 240);
  lv_obj_center(ui.sleep_layer);
  lv_obj_set_style_bg_opa(ui.sleep_layer, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(ui.sleep_layer, lv_color_black(), 0);
  ui.sleep_lbl = lv_label_create(ui.sleep_layer);
  lv_obj_set_style_text_font(ui.sleep_lbl, f.huge, 0);
  lv_label_set_text(ui.sleep_lbl, "--");
  lv_obj_center(ui.sleep_lbl);
  lv_obj_add_flag(ui.sleep_layer, LV_OBJ_FLAG_HIDDEN);
}

// Play the start-up build: everything disappears, then comes back piece by piece
inline void boot() {
  for (auto &r : ui.reveals) lv_obj_add_flag(r.obj, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(ui.scanline, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_size(ui.scanline, 0, 3);
  ui.booting = true;
  ui.boot_t = 0;
  ui.boot_frames = 0;
  ui.shown = 0;
  ui.last_set = -999;
}

// Switch between the full display and the sleep screen
inline void set_sleep(bool asleep) {
  if (asleep == ui.asleep) return;
  ui.asleep = asleep;
  ui.sleep_last = -999;
  if (asleep) lv_obj_remove_flag(ui.sleep_layer, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(ui.sleep_layer, LV_OBJ_FLAG_HIDDEN);
}

// Call on knob press so the status underline flashes
inline void flash() { ui.pulse = 1.0f; }

// "lowMedium" -> "LOW MEDIUM", "fan_only" -> "FAN ONLY"
inline std::string fan_label(const std::string &m) {
  std::string out;
  for (size_t i = 0; i < m.size(); i++) {
    char c = m[i];
    if (c == '_' || c == '-') c = ' ';
    else if (i && isupper((unsigned char) c) && islower((unsigned char) m[i - 1])) out += ' ';
    out += (char) toupper((unsigned char) c);
  }
  return out;
}

// Show or hide the fan speed page. Bars rise left to right, lit up to the current speed.
inline void set_fan(bool active, int index, const std::vector<std::string> &modes) {
  if (!ui.fan_layer) return;
  if (!active) {
    lv_obj_add_flag(ui.fan_layer, LV_OBJ_FLAG_HIDDEN);
    ui.fan_shown = -2;
    return;
  }
  int n = (int) modes.size();
  if ((int) ui.fan_bars.size() != n) {
    for (auto *b : ui.fan_bars) lv_obj_delete(b);
    ui.fan_bars.clear();
    // Segments fill the span between the centres of the end caps, so the caps round off
    // the first and last ones
    const int r = FAN_BAR_H / 2, span = FAN_BAR_W - 2 * r;
    for (int i = 0; i < n; i++) {
      int x0 = r + span * i / n + (i ? FAN_BAR_GAP / 2 : 0);
      int x1 = r + span * (i + 1) / n - (i < n - 1 ? (FAN_BAR_GAP + 1) / 2 : 0);
      lv_obj_t *b = plain(ui.fan_bar_box);
      lv_obj_set_size(b, x1 - x0, FAN_BAR_H);
      lv_obj_set_pos(b, x0, 0);
      lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
      ui.fan_bars.push_back(b);
    }
    if (n) {
      lv_label_set_text(ui.fan_lo, fan_label(modes.front()).c_str());
      lv_label_set_text(ui.fan_hi, fan_label(modes.back()).c_str());
    }
    ui.fan_shown = -2;
  }
  lv_obj_remove_flag(ui.fan_layer, LV_OBJ_FLAG_HIDDEN);
  if (index == ui.fan_shown) return;
  ui.fan_shown = index;
  // Lit up to the current speed in orange, the current one brighter, the rest unlit
  auto colour = [&](int i) {
    return lv_color_hex(i > index ? DIM : i == index ? PEACH : ORANGE);
  };
  for (int i = 0; i < n; i++) lv_obj_set_style_bg_color(ui.fan_bars[i], colour(i), 0);
  if (n) {
    lv_obj_set_style_bg_color(ui.fan_cap[0], colour(0), 0);
    lv_obj_set_style_bg_color(ui.fan_cap[1], colour(n - 1), 0);
  }
  std::string name = index >= 0 && index < n ? fan_label(modes[index]) : "--";
  lv_label_set_text(ui.fan_name, name.c_str());
}

// Advance the animation one frame (~33 ms)
inline void tick(const Input &in) {
  ui.frame++;

  // Asleep: only the room temperature needs keeping up to date
  if (ui.asleep) {
    float cur = std::isnan(in.cur_temp) ? -999 : in.cur_temp;
    if (cur != ui.sleep_last) {
      ui.sleep_last = cur;
      if (std::isnan(in.cur_temp)) {
        lv_label_set_text(ui.sleep_lbl, "--");
        lv_obj_set_style_text_color(ui.sleep_lbl, lv_color_hex(0x7766AA), 0);
      } else {
        lv_label_set_text_fmt(ui.sleep_lbl, "%d", (int) lroundf(in.cur_temp));
        float range = in.max_temp - in.min_temp;
        float t = range > 0 ? (in.cur_temp - in.min_temp) / range : 0.5f;
        t = t < 0 ? 0 : (t > 1 ? 1 : t);
        lv_obj_set_style_text_color(ui.sleep_lbl, ramp(t), 0);
      }
      // Centre the digits themselves, not the label box (the font's line height
      // leaves more room above the digits than below)
      const lv_font_t *font = lv_obj_get_style_text_font(ui.sleep_lbl, LV_PART_MAIN);
      lv_font_glyph_dsc_t g;
      lv_font_get_glyph_dsc(font, &g, '0', 0);
      int line_h = lv_font_get_line_height(font);
      float digit_mid = (line_h - font->base_line) - g.ofs_y - g.box_h / 2.0f;
      lv_obj_align(ui.sleep_lbl, LV_ALIGN_CENTER, 0, (int) lroundf(line_h / 2.0f - digit_mid));
    }
    return;
  }

  if (ui.booting) {
    int t = ui.boot_t++;
    // Scan line opens across the middle, then collapses
    if (t <= 9) {
      lv_obj_set_size(ui.scanline, t * 24, 3);
      lv_obj_center(ui.scanline);
    } else if (t <= 14) {
      lv_obj_set_size(ui.scanline, 216 - (t - 9) * 43, 3);
      lv_obj_center(ui.scanline);
    } else {
      lv_obj_add_flag(ui.scanline, LV_OBJ_FLAG_HIDDEN);
    }
    for (auto &r : ui.reveals) {
      if (r.at != t) continue;
      lv_obj_remove_flag(r.obj, LV_OBJ_FLAG_HIDDEN);
      if (r.flash && lv_obj_check_type(r.obj, &lv_arc_class)) {
        lv_obj_set_style_arc_color(r.obj, lv_color_white(), LV_PART_MAIN);
        ui.flashing.push_back(r.obj);
        ui.flash_left = 3;
      }
    }
    if (t == 32) ui.boot_frames = 40;  // gauge sweeps up to full and back as it comes online
    if (t >= 64) {
      ui.booting = false;
      for (auto &r : ui.reveals) lv_obj_remove_flag(r.obj, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui.scanline, LV_OBJ_FLAG_HIDDEN);
    }
  }
  // Flashed segments settle back to their own colours
  if (ui.flash_left > 0 && --ui.flash_left == 0) {
    ui.flashing.clear();
    ui.last_power = -1;
  }

  // Boot: sweep the gauge up to full, then settle on the real value
  float range = in.max_temp - in.min_temp;
  float target = range > 0 ? (in.set_temp - in.min_temp) / range * 100.0f : 0;
  target = target < 0 ? 0 : (target > 100 ? 100 : target);
  if (ui.booting && ui.boot_t < 32) target = 0;
  if (ui.boot_frames > 0) {
    ui.boot_frames--;
    target = ui.boot_frames > 15 ? 100 : target;
  }
  ui.shown += (target - ui.shown) * 0.2f;
  if (fabsf(target - ui.shown) < 0.05f) ui.shown = target;

  float want_power = (in.on || ui.boot_frames > 0) ? 1.0f : 0.0f;
  ui.power += (want_power - ui.power) * 0.12f;
  if (fabsf(want_power - ui.power) < 0.005f) ui.power = want_power;
  uint8_t pw = (uint8_t) (ui.power * 255);

  // Frame and decoration fade when the air con is off
  if (fabsf(ui.power - ui.last_power) > 0.004f) {
    ui.last_power = ui.power;
    for (auto &d : ui.fades) {
      lv_color_t c = dimmed(d.color, pw);
      switch (d.part) {
        case ARC_MAIN: lv_obj_set_style_arc_color(d.obj, c, LV_PART_MAIN); break;
        case ARC_IND: lv_obj_set_style_arc_color(d.obj, c, LV_PART_INDICATOR); break;
        case BG: lv_obj_set_style_bg_color(d.obj, c, 0); break;
        case TEXT: lv_obj_set_style_text_color(d.obj, c, 0); break;
      }
    }
    for (int i = 0; i < GAUGE_BLOCKS; i++)
      lv_obj_set_style_arc_color(ui.gauge[i], lv_color_mix(ui.gauge_color[i], lv_color_hex(DIM), pw),
                                 LV_PART_INDICATOR);
  }

  // Gauge fill, by angle so the cuts between blocks are skipped naturally. The lowest
  // setting still fills the first (blue) block, so the gauge is never empty.
  float level_ang = GAUGE_SPAN[0] + ui.shown / 100.0f * (ui.gauge_sweep - GAUGE_SPAN[0]);
  for (int i = 0; i < GAUGE_BLOCKS; i++) {
    float fill = (level_ang - ui.gauge_start[i]) / GAUGE_SPAN[i];
    fill = fill < 0 ? 0 : (fill > 1 ? 1 : fill);
    lv_arc_set_value(ui.gauge[i], (int) (fill * 100 + 0.5f));
  }

  // Readout takes the colour at the set temperature
  bool show_off = ui.power < 0.5f;
  // During start-up the number counts up from the bottom of the range
  float shown_set = in.set_temp;
  if (ui.booting && ui.boot_t < 58) {
    float p = ui.boot_t < 40 ? 0 : (ui.boot_t - 40) / 18.0f;
    shown_set = roundf((in.min_temp + (in.set_temp - in.min_temp) * p) * 2) / 2;
  }
  float key = show_off ? -1000.0f : shown_set;
  if (key != ui.last_set) {
    ui.last_set = key;
    if (show_off) {
      lv_label_set_text(ui.num, "OFF");
    } else {
      // Whole degrees without a decimal, half degrees with one
      int tenths = (int) lroundf(shown_set * 10);
      if (tenths % 10 == 0) lv_label_set_text_fmt(ui.num, "%d", tenths / 10);
      else lv_label_set_text_fmt(ui.num, "%d.%d", tenths / 10, abs(tenths % 10));
    }
  }

  // Room temperature text and its notch on the gauge
  float cur = std::isnan(in.cur_temp) ? -999 : in.cur_temp;
  if (cur != ui.last_cur) {
    ui.last_cur = cur;
    if (std::isnan(in.cur_temp)) {
      lv_label_set_text(ui.now_lbl, "--");
    } else {
      int tenths = (int) lroundf(in.cur_temp * 10);
      lv_label_set_text_fmt(ui.now_lbl, "%d.%d\xC2\xB0", tenths / 10, abs(tenths % 10));
    }
  }
  // Inner track eases to the room temperature
  float cur_t = -1;
  if (!std::isnan(in.cur_temp) && range > 0) {
    cur_t = (in.cur_temp - in.min_temp) / range;
    cur_t = cur_t < 0 ? 0 : (cur_t > 1 ? 1 : cur_t);
  }
  if (cur_t < 0) {
    ui.shown_cur = -1;
  } else if (ui.shown_cur < 0) {
    ui.shown_cur = cur_t;
  } else {
    ui.shown_cur += (cur_t - ui.shown_cur) * 0.15f;
    if (fabsf(cur_t - ui.shown_cur) < 0.001f) ui.shown_cur = cur_t;
  }
  lv_arc_set_value(ui.track, ui.shown_cur < 0 ? 0 : (int) lroundf(ui.shown_cur * 1000));
  lv_color_t tc = ui.shown_cur < 0 ? lv_color_hex(UNLIT) : lv_color_mix(ramp(ui.shown_cur), lv_color_hex(DIM), pw);
  if (!lv_color_eq(tc, ui.c_track)) {
    ui.c_track = tc;
    lv_obj_set_style_arc_color(ui.track, tc, LV_PART_INDICATOR);
  }
  int marker = ui.shown_cur < 0 ? -1 : GAUGE_START + (int) lroundf(ui.shown_cur * ui.gauge_sweep);
  if (marker != ui.last_marker) {
    ui.last_marker = marker;
    if (marker < 0) lv_arc_set_angles(ui.marker, 0, 0);
    else lv_arc_set_angles(ui.marker, marker - 2, marker + 2);
  }

  // Number colour follows the set temperature
  float set_t = range > 0 ? (shown_set - in.min_temp) / range : 0;
  set_t = set_t < 0 ? 0 : (set_t > 1 ? 1 : set_t);
  lv_color_t nc = show_off ? lv_color_hex(0x7766AA) : lv_color_mix(ramp(set_t), lv_color_hex(DIM), pw);
  if (!lv_color_eq(nc, ui.c_num)) {
    ui.c_num = nc;
    lv_obj_set_style_text_color(ui.num, nc, 0);
  }

  // Scanner: a light sweeps up and down the right-hand blocks leaving a fading trail.
  // Orange while heating, blue while cooling, lilac when idle, fast and gold while turning.
  bool working = in.on && (in.action == HEATING || in.action == COOLING);
  float speed = in.adjusting ? 0.45f : (working ? 0.2f : 0.06f + 0.06f * ui.power);
  ui.scan_pos += speed;
  float span = 2.0f * (Ui::SCAN - 1);
  if (ui.scan_pos >= span) ui.scan_pos -= span;
  float head = ui.scan_pos < Ui::SCAN - 1 ? ui.scan_pos : span - ui.scan_pos;
  uint32_t scan_color = in.adjusting ? GOLD : in.action == HEATING ? 0xFF7733 : in.action == COOLING ? SKY : LILAC;
  for (int i = 0; i < Ui::SCAN; i++) {
    float d = fabsf(i - head);
    float hit = d < 1 ? 1 - d : 0;
    ui.scan_level[i] = fmaxf(hit, ui.scan_level[i] * 0.86f);
    float lvl = ui.scan_level[i] * (0.35f + 0.65f * ui.power);
    lv_color_t c = lv_color_mix(lv_color_hex(scan_color), lv_color_hex(0x1E2A3A), (uint8_t) (lvl * 255));
    if (!lv_color_eq(c, ui.c_scan[i])) {
      ui.c_scan[i] = c;
      lv_obj_set_style_arc_color(ui.scan[i], c, LV_PART_MAIN);
    }
  }

  // Power button: orange when on, red when off, flashes white when pressed
  lv_color_t bc = lv_color_mix(lv_color_hex(ORANGE), lv_color_hex(0x992A2A), pw);
  if (ui.pulse > 0) bc = lv_color_mix(lv_color_white(), bc, (uint8_t) (ui.pulse * 255));
  if (!lv_color_eq(bc, ui.c_button)) {
    ui.c_button = bc;
    lv_obj_set_style_bg_color(ui.button, bc, 0);
  }

  // Status text
  const char *status;
  uint32_t color;
  if (ui.booting) {
    status = "INIT";
    color = (ui.frame / 4) % 2 ? GOLD : 0x664400;
  } else if (!in.linked) {
    status = "NO LINK";
    color = (ui.frame / 15) % 2 ? RED : 0x661111;
  } else if (in.fan) {
    status = "FAN SPEED";
    color = ORANGE;
  } else if (in.adjusting) {
    status = "ADJUST";
    color = GOLD;
  } else if (!in.on) {
    status = "AC OFF";
    color = 0x7766AA;
  } else if (in.action == HEATING) {
    status = "HEATING";
    color = 0xFF8844;
  } else if (in.action == COOLING) {
    status = "COOLING";
    color = SKY;
  } else {
    status = in.mode;
    color = LILAC;
  }
  if (strcmp(lv_label_get_text(ui.status), status) != 0) lv_label_set_text(ui.status, status);
  if (!lv_color_eq(lv_color_hex(color), ui.c_status)) {
    ui.c_status = lv_color_hex(color);
    lv_obj_set_style_text_color(ui.status, ui.c_status, 0);
  }

  // Underline flashes white on a press
  lv_color_t uc = dimmed(ORANGE, pw);
  if (ui.pulse > 0) {
    ui.pulse -= 0.08f;
    if (ui.pulse < 0) ui.pulse = 0;
    uc = lv_color_mix(lv_color_white(), uc, (uint8_t) (ui.pulse * 255));
  }
  if (!lv_color_eq(uc, ui.c_underline)) {
    ui.c_underline = uc;
    lv_obj_set_style_bg_color(ui.underline, uc, 0);
  }
}

}  // namespace lcars_ac
