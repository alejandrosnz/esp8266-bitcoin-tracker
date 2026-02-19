/**
 * @file display_utils.h
 * @brief OLED rendering helper for the Bitcoin tracker.
 *
 * The single public function @ref print_to_screen takes the display instance
 * by reference so that this module has no global state and no dependency on
 * the sketch's global @c display object.
 */
#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

/**
 * @brief Render the full price screen on the SH1106G OLED.
 *
 * Screen layout (128 × 64 px):
 * @verbatim
 * ┌──────────────────────────────────┐
 * │  60,950              (price)    $│  ← text size 3
 * │  .01                             │  ← text size 2 (decimals, if ≥ $1 K)
 * │ ┌─────┐  ↑↑   3.5%              │
 * │ │ BTC │  icon  pct               │  ← text size 2
 * │ └─────┘                          │
 * └──────────────────────────────────┘
 * @endverbatim
 *
 * The direction bitmap is chosen from icons.h based on the % change vs the
 * midnight-UTC open price (@p closing_price):
 *
 * | Range            | Icon              |
 * |------------------|-------------------|
 * | diff ≥ +3.5 %    | bitmap_up_double  |
 * | +1.5 % ≤ diff    | bitmap_up_single  |
 * |  0 %  ≤ diff     | bitmap_up_thin    |
 * |        diff > −1.5 % | bitmap_down_thin  |
 * |        diff > −3.5 % | bitmap_down_single|
 * |        diff ≤ −3.5 % | bitmap_down_double|
 *
 * When @c DIFF_PRINT_PERCENTAGE_AND_VALUE is @c true (config.h), both the
 * percentage and the absolute dollar change are shown in a smaller font.
 * When @c false, only the percentage is rendered in a larger font.
 *
 * @param display        Reference to the SH1106G display instance.
 * @param current_price  Most recently fetched price.
 * @param previous_price Price from the previous poll cycle.
 *                       Reserved for future use (e.g. flash animation on
 *                       direction change); not rendered at the moment.
 * @param closing_price  Midnight-UTC open price used as the daily reference.
 * @param symbol         Asset label shown in the rounded-rectangle box.
 *                       Should be ≤ 3 characters to fit the layout (e.g. "BTC").
 */
void print_to_screen(Adafruit_SH1106G& display,
                     double            current_price,
                     double            previous_price,
                     double            closing_price,
                     const char*       symbol);
