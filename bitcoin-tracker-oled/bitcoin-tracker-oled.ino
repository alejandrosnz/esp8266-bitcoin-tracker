/**
 * @file bitcoin-tracker-oled.ino
 * @brief ESP8266 crypto price tracker with SH1106G OLED display.
 *
 * Displays real-time prices and a daily-change indicator for one or more
 * crypto assets fetched directly from the Binance REST API over HTTPS.
 * No proxy or external server is required.
 *
 * ── Hardware ────────────────────────────────────────────────────────────────
 *  - ESP8266 board (NodeMCU, Wemos D1 Mini, or compatible)
 *  - SH1106G 128×64 OLED via I2C (default address 0x3C)
 *    SDA → D1  |  SCL → D2  (configurable in config.h)
 *
 * ── Dependencies (Arduino Library Manager) ──────────────────────────────────
 *  - ArduinoJson  ≥ 6.x   (Benoit Blanchon)
 *  - Adafruit GFX Library
 *  - Adafruit SH110X
 *
 * ── Quick start ─────────────────────────────────────────────────────────────
 *  1. Open config.h and set your WiFi credentials and desired symbols.
 *  2. Flash to your ESP8266 via the Arduino IDE.
 *  3. Done — no Docker, no proxy, no API key required.
 *
 * ── How it works ────────────────────────────────────────────────────────────
 *  setup():  Connects to WiFi, then for every symbol fetches
 *              • the midnight-UTC open price  (Binance klines, daily candle)
 *              • the latest traded price      (Binance ticker/price)
 *            If REFRESH_OPENING_PRICE_AT_MIDNIGHT is enabled (config.h),
 *            configTime() registers the NTP servers for background sync.
 *
 *  loop():   Non-blocking. Two independent timers drive behaviour:
 *              • Every poll_delay ms              → fetch the active symbol's price.
 *              • Every SECONDS_TO_DISPLAY_EACH_SYMBOL s → rotate symbol.
 *            WiFi drops are detected each iteration; reconnection is attempted
 *            every WIFI_RECONNECT_TIMEOUT_MS ms and the screen shows an error
 *            message while the link is down.
 *            At UTC midnight, closing_prices[] is refreshed silently in
 *            the background (only if REFRESH_OPENING_PRICE_AT_MIDNIGHT).
 *
 * ── Memory highlights ───────────────────────────────────────────────────────
 *  - Prices are stored in plain double[] arrays (no JSON document overhead).
 *  - API responses are parsed as streams (never loaded into a String).
 *  - ArduinoJson filters ensure only needed fields are allocated.
 *  - BearSSL TLS buffers are capped via config.h (~28 KB vs default ~60 KB).
 */

#include <ESP8266WiFi.h>
#include <Wire.h>
#include <avr/pgmspace.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <time.h>

#include "config.h"
#include "debug.h"
#include "api.h"
#include "display_utils.h"

// ── Globals ───────────────────────────────────────────────────────────────────

Adafruit_SH1106G display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

/// Last known price for each symbol (index mirrors list_of_symbols[]).
double current_prices[size_of_list_of_symbols];

/// Midnight-UTC open price for each symbol, used as daily % reference.
double closing_prices[size_of_list_of_symbols];

int           symbol_index           = 0;
unsigned long start_time             = 0;  ///< Symbol-rotation timer (unsigned to avoid millis() overflow)
unsigned long last_poll_time         = 0;  ///< Last time a price was fetched

#if REFRESH_OPENING_PRICE_AT_MIDNIGHT
/// UTC day-of-year at last opening-price fetch; -1 until NTP is synced.
int last_day = -1;
#endif

// ─────────────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(9600);

  // OLED init
  Wire.begin(OLED_SDA, OLED_SCL);
  delay(250);
  display.begin(OLED_I2C_ADDR, true);
  display.setTextColor(SH110X_WHITE);
  display.clearDisplay();
  display.println(F("Connecting..."));
  display.display();

  // Built-in LED is active-low on most ESP8266 boards (HIGH = off)
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH);

  // Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print('.');
  }
  Serial.print(F("Connected! IP: "));
  Serial.println(WiFi.localIP());
  display.clearDisplay();
  display.println(F("WiFi connected!"));
  display.display();

#if REFRESH_OPENING_PRICE_AT_MIDNIGHT
  // Register NTP servers — non-blocking. The ESP8266 SNTP client syncs in the
  // background automatically; no waiting here. loop() will initialise last_day
  // on the first successful sync and trigger midnight refreshes thereafter.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  DEBUG_PRINT(F("[ntp] Servers registered — sync in background"));
#endif

  // Fetch initial prices for all configured symbols
  for (int i = 0; i < size_of_list_of_symbols; i++) {
    closing_prices[i] = get_closing_price(list_of_symbols[i]);
    current_prices[i] = get_current_price(list_of_symbols[i]);
  }

  // Show the first symbol immediately
  print_to_screen(display,
                  current_prices[0], current_prices[0],
                  closing_prices[0], list_of_symbols[0]);

  last_poll_time = millis();
  start_time     = millis();
}

// ─────────────────────────────────────────────────────────────────────────────

void loop() {
  unsigned long now_ms = millis();

  // ── WiFi watchdog ─────────────────────────────────────────────────────────
  if (WiFi.status() != WL_CONNECTED) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("Connecting..."));
    display.display();
    delay(500);
    return;
  }

  // ── Non-blocking poll gate ────────────────────────────────────────────────
  // Nothing to do until poll_delay ms have elapsed since the last fetch.
  if (now_ms - last_poll_time < (unsigned long)poll_delay) return;
  last_poll_time = now_ms;

  // ── Symbol rotation ───────────────────────────────────────────────────────
  if (now_ms - start_time >= (unsigned long)(SECONDS_TO_DISPLAY_EACH_SYMBOL * 1000L)) {
    start_time   = now_ms;
    symbol_index = (symbol_index + 1) % size_of_list_of_symbols;
  }

  // ── Midnight UTC opening-price refresh ───────────────────────────────────
  // Runs silently in the background: the current price is still shown while
  // the closing_prices[] array is being updated.
#if REFRESH_OPENING_PRICE_AT_MIDNIGHT
  {
    time_t t = time(nullptr);
    if (t > 1000000000L) {                        // NTP has synced
      struct tm* tm_info = gmtime(&t);
      if (last_day == -1) {
        // First sync after boot — just record today's day so we don't
        // trigger a redundant refresh (closing_prices were already fetched
        // in setup()).
        last_day = tm_info->tm_yday;
        DEBUG_PRINT(F("[ntp] Synced — last_day initialised"));
      } else if (tm_info->tm_yday != last_day) {  // UTC day has rolled over
        last_day = tm_info->tm_yday;
        DEBUG_PRINT(F("[midnight] Refreshing opening prices..."));
        for (int i = 0; i < size_of_list_of_symbols; i++) {
          double new_closing = get_closing_price(list_of_symbols[i]);
          if (new_closing > 0) closing_prices[i] = new_closing;
        }
      }
    }
  }
#endif

  // ── Price fetch and display ───────────────────────────────────────────────
  double new_price = get_current_price(list_of_symbols[symbol_index]);

  if (new_price < 0) {
    // API call failed — show error message
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("API error"));
    display.display();
  } else if (new_price != current_prices[symbol_index]) {
    // Only redraw when the price has actually changed (avoids flicker).
    print_to_screen(display,
                    new_price,
                    current_prices[symbol_index],
                    closing_prices[symbol_index],
                    list_of_symbols[symbol_index]);
    current_prices[symbol_index] = new_price;
  }
}
