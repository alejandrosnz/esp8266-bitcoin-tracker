/**
 * @file display_utils.cpp
 * @brief OLED rendering implementation for the Bitcoin tracker.
 */

#include "display_utils.h"
#include <avr/pgmspace.h>

#include "config.h"
#include "icons.h"

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Select and draw the direction bitmap based on a % change value.
 *
 * Thresholds (absolute %, with sign):
 *   ≥ +3.5 → double-up   /  ≤ −3.5 → double-down
 *   ≥ +1.5 → single-up   /  ≤ −1.5 → single-down
 *   ≥  0   → thin-up     /  < 0    → thin-down
 */
static void draw_direction_icon(Adafruit_SH1106G& display, double pct_diff) {
  const unsigned char* bmp;

  if      (pct_diff >= 3.5)  bmp = bitmap_up_double;
  else if (pct_diff >= 1.5)  bmp = bitmap_up_single;
  else if (pct_diff >= 0)    bmp = bitmap_up_thin;
  else if (pct_diff > -1.5)  bmp = bitmap_down_thin;
  else if (pct_diff > -3.5)  bmp = bitmap_down_single;
  else                       bmp = bitmap_down_double;

  display.drawBitmap(59, 37, bmp, ICON_WIDTH, ICON_HEIGHT, SH110X_WHITE);
}

/**
 * @brief Render the price number, choosing size and decimal precision by range.
 *
 * At text-size 3, each character is 18 px wide and the screen is 128 px wide,
 * so the number of integer digits that fit constrains what we can show:
 *
 * | Range         | Integer digits | Decimal shown? |
 * |---------------|----------------|----------------|
 * | ≥ 1 000 000   | 7              | no             |
 * | ≥ 100 000     | 6              | no             |
 * | ≥ 1 000       | 2–5            | 2 digits (size 2) |
 * | ≥ 10          | 2–3            | 2 digits (size 2) |
 * | < 10          | 1              | 4 digits (size 3) |
 */
static void draw_price(Adafruit_SH1106G& display, double price) {
  display.setTextSize(3);
  String s         = String(price, 4);
  int    dec_index = s.indexOf('.');

  if (price >= 1000000) {
    display.setCursor(0, 4);
    display.print(s.substring(0, dec_index));
  } else if (price >= 100000) {
    display.setCursor(5, 4);
    display.print(s.substring(0, dec_index));
  } else if (price >= 1000) {
    display.setCursor(5, 4);
    display.print(s.substring(0, dec_index));
    display.setTextSize(2);
    display.setCursor(dec_index * 17 + 5, 11);
    display.print(s.substring(dec_index, dec_index + (7 - dec_index)));
  } else if (price >= 10) {
    display.setCursor(20, 4);
    display.print(s.substring(0, dec_index));
    display.setTextSize(2);
    display.setCursor(dec_index * 17 + 20, 11);
    display.print(s.substring(dec_index, dec_index + (6 - dec_index)));
  } else {
    display.setCursor(5, 4);
    display.print(s);
  }

  // Dollar sign — omitted only when the price overflows at ≥ $1 M
  if (price < 1000000) {
    display.setCursor(115, 11);
    display.setTextSize(2);
    display.print('$');
  }
}

/**
 * @brief Render the percentage change, and optionally the absolute change.
 *
 * Controlled by DIFF_PRINT_PERCENTAGE_AND_VALUE in config.h:
 *   false → percentage only, text-size 2 (larger, easier to read at a glance)
 *   true  → percentage + absolute $ change, text-size 1 (compact)
 */
static void draw_change(Adafruit_SH1106G& display,
                        double current_price, double closing_price) {
  float pct = fabsf((float)(current_price - closing_price)
                    / (float)closing_price * 100.0f);

  if (DIFF_PRINT_PERCENTAGE_AND_VALUE) {
    display.setCursor(80, 35);
    display.setTextSize(1);
    display.print(pct, 2);
    display.print('%');

    display.setCursor(80, 45);
    display.setTextSize(1);
    display.print((float)fabs(current_price - closing_price), 2);
  } else {
    display.setCursor(80, 37);
    display.setTextSize(2);
    display.print(pct, (pct < 10.0f) ? 1 : 0);
    if (pct >= 10.0f) {
      // Nudge the '%' sign closer when there is no decimal digit
      display.setTextSize(1);
      display.print(' ');
      display.setTextSize(2);
    }
    display.print('%');
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void print_to_screen(Adafruit_SH1106G& display,
                     double            current_price,
                     double            /*previous_price*/,   // reserved for future use
                     double            closing_price,
                     const char*       symbol) {
  display.clearDisplay();

  draw_price(display, current_price);

  // Symbol label inside a rounded rectangle
  display.setCursor(15, 37);
  display.setTextSize(2);
  display.print(symbol);
  display.drawRoundRect(10, 32, 44, 24, 8, SH110X_WHITE);

  double pct_diff = (current_price - closing_price) / closing_price * 100.0;
  draw_direction_icon(display, pct_diff);
  draw_change(display, current_price, closing_price);

  display.display();
}
