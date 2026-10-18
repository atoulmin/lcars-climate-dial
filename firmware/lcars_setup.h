// LCARS first-run screens, shown over the dial on the top layer:
//  - WIFI: no Wi-Fi yet, the setup hotspot is up; a QR code joins it
//  - HOME_ASSISTANT: on Wi-Fi, waiting for Home Assistant to add the dial (no API key yet)
//  - CLIMATE: added, but no climate entity chosen yet
//  - NOT_FOUND: the chosen climate entity doesn't exist in Home Assistant
#pragma once
#include "lvgl.h"
#include "lcars_climate.h"
#include <string>

namespace lcars_setup {

enum Mode : uint8_t { NONE, WIFI, HOME_ASSISTANT, CLIMATE, NOT_FOUND };

struct Ui {
  lv_obj_t *root = nullptr, *title = nullptr, *page[5] = {}, *ip = nullptr, *entity = nullptr;
  Mode mode = NONE;
  std::string ip_text, entity_text;
};
static Ui U;

static lv_obj_t *text(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *s, int y) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_line_space(l, -2, 0);
  lv_label_set_text(l, s);
  lv_obj_align(l, LV_ALIGN_CENTER, 0, y);
  return l;
}

static lv_obj_t *page(lv_obj_t *parent) {
  lv_obj_t *p = lv_obj_create(parent);
  lv_obj_remove_style_all(p);
  lv_obj_set_size(p, 240, 240);
  lv_obj_add_flag(p, LV_OBJ_FLAG_HIDDEN);
  return p;
}

// big: 20 px LCARS (the dial's status font), small: 16 px LCARS
static void build(const lv_font_t *big, const lv_font_t *small, const char *ssid) {
  using namespace lcars_ac;
  U.root = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(U.root);
  lv_obj_set_size(U.root, 240, 240);
  lv_obj_set_style_bg_color(U.root, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(U.root, LV_OPA_COVER, 0);
  lv_obj_add_flag(U.root, LV_OBJ_FLAG_HIDDEN);

  // The dial's double frame, broken at the top for the title
  struct Seg { int from, to; uint32_t color; };
  const Seg outer[] = {{198, 248, 0xFFCC66}, {292, 340, 0x9999FF}, {150, 194, 0xCC6666}, {344, 30, ORANGE},
                       {104, 146, PEACH},    {34, 76, 0xCC9966},   {80, 100, 0x9999FF}};
  for (auto &s : outer) band(U.root, 238, 15, s.from, s.to, s.color, false);
  const Seg inner[] = {{148, 212, ORANGE}, {216, 244, LILAC}, {296, 324, PEACH}, {36, 70, VIOLET}, {110, 144, VIOLET}};
  for (auto &s : inner) band(U.root, 204, 6, s.from, s.to, s.color, false);
  U.title = text(U.root, big, SKY, "", -105);

  // Wi-Fi: QR code that joins the hotspot
  lv_obj_t *p = U.page[WIFI] = page(U.root);
  lv_obj_t *qr = lv_qrcode_create(p);
  lv_qrcode_set_size(qr, 84);
  lv_qrcode_set_dark_color(qr, lv_color_black());
  lv_qrcode_set_light_color(qr, lv_color_hex(PEACH));
  std::string data = std::string("WIFI:T:nopass;S:") + ssid + ";;";
  lv_qrcode_update(qr, data.c_str(), data.size());
  lv_obj_set_style_border_color(qr, lv_color_hex(PEACH), 0);
  lv_obj_set_style_border_width(qr, 4, 0);
  lv_obj_align(qr, LV_ALIGN_CENTER, 0, -12);
  text(p, small, LILAC, "SCAN OR JOIN", 46);
  text(p, big, ORANGE, ssid, 64);

  // Home Assistant: where to find it
  p = U.page[HOME_ASSISTANT] = page(U.root);
  text(p, small, LILAC, "SUBSPACE LINK READY", -56);
  text(p, big, ORANGE, "ADD ME IN\nHOME ASSISTANT", -22);
  text(p, small, PEACH, "SETTINGS > DEVICES & SERVICES\nLOOK UNDER DISCOVERED", 22);
  U.ip = text(p, small, SKY, "", 56);

  // No climate entity chosen yet
  p = U.page[CLIMATE] = page(U.root);
  text(p, small, LILAC, "LINKED TO HOME ASSISTANT", -62);
  text(p, big, ORANGE, "SELECT A\nCLIMATE DEVICE", -32);
  text(p, small, PEACH, "1. ALLOW DEVICE ACTIONS\n2. TYPE CLIMATE ENTITY\nTHEN PRESS ENTER", 16);
  text(p, small, SKY, "ON THE DEVICE PAGE", 56);

  // Chosen entity not found
  p = U.page[NOT_FOUND] = page(U.root);
  text(p, small, LILAC, "NO RESPONSE FROM", -50);
  U.entity = text(p, big, 0xCC6666, "", -26);
  text(p, small, PEACH, "CHECK CLIMATE ENTITY ON\nTHE DEVICE PAGE", 12);
  text(p, small, SKY, "E.G. CLIMATE.LIVING_ROOM", 48);
}

static void upper(std::string &s) {
  for (auto &c : s)
    c = toupper((unsigned char) c);
}

// Called about once a second
static void show(Mode mode, const char *ip, const char *entity) {
  if (!U.root)
    return;
  if (U.ip_text != ip) {
    U.ip_text = ip;
    lv_label_set_text(U.ip, ip);
  }
  if (U.entity_text != entity) {
    U.entity_text = entity;
    std::string e = entity;
    upper(e);
    lv_label_set_text(U.entity, e.c_str());
  }
  if (mode == U.mode)
    return;
  bool was_setup = U.mode != NONE;
  U.mode = mode;
  static const char *TITLES[] = {"", "WI-FI SETUP", "STANDBY", "STANDBY", "ALERT"};
  lv_label_set_text(U.title, TITLES[mode]);
  lv_obj_set_style_text_color(U.title, lv_color_hex(mode == NOT_FOUND ? 0xCC6666 : lcars_ac::SKY), 0);
  for (int i = 1; i < 5; i++) {
    if (i == mode)
      lv_obj_remove_flag(U.page[i], LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_add_flag(U.page[i], LV_OBJ_FLAG_HIDDEN);
  }
  if (mode == NONE) {
    lv_obj_add_flag(U.root, LV_OBJ_FLAG_HIDDEN);
    // Setup done: build the dial up
    if (was_setup)
      lcars_ac::boot();
  } else {
    lv_obj_remove_flag(U.root, LV_OBJ_FLAG_HIDDEN);
  }
}

static bool active() { return U.mode != NONE; }

}  // namespace lcars_setup
