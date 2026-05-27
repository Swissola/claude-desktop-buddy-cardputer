#pragma once
// Glass2 Grove secondary OLED (SSD1309, 128x64) helper — Cardputer ADV only.
// Grove HY2.0-4P port: GPIO2=SDA, GPIO1=SCL.
// M5UnitGLASS2 is part of M5GFX, which is already a transitive dependency
// via M5Unified — no extra lib_deps entry required.
//
// Include from exactly one translation unit (main.cpp).
#include <M5UnitGLASS2.h>

static M5UnitGLASS2 _g2(/*sda=*/2, /*scl=*/1);
static bool _g2_ready = false;

inline void glass2Init() {
    _g2_ready = _g2.init();
    if (_g2_ready) {
        _g2.fillScreen(TFT_BLACK);
        _g2.setTextColor(TFT_WHITE, TFT_BLACK);
    }
}

// Show up to four lines. Null or omitted lines are blank.
inline void glass2Show(const char* l1, const char* l2 = nullptr,
                       const char* l3 = nullptr, const char* l4 = nullptr) {
    if (!_g2_ready) return;
    _g2.fillScreen(TFT_BLACK);
    _g2.setTextSize(1);
    _g2.setTextColor(TFT_WHITE, TFT_BLACK);
    if (l1) { _g2.setCursor(0,  0); _g2.print(l1); }
    if (l2) { _g2.setCursor(0, 16); _g2.print(l2); }
    if (l3) { _g2.setCursor(0, 32); _g2.print(l3); }
    if (l4) { _g2.setCursor(0, 48); _g2.print(l4); }
}

inline void glass2Clear() {
    if (!_g2_ready) return;
    _g2.fillScreen(TFT_BLACK);
}
