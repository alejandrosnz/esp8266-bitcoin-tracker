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
 *
 *  loop():   Every poll_delay ms, re-fetches the current price for the
 *            active symbol and redraws the screen if the price changed.
 *            Symbols rotate every SECONDS_TO_DISPLAY_EACH_SYMBOL seconds.
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

#include "config.h"
#include "debug.h"
#include "api.h"
#include "display_utils.h"

// ── Globals ───────────────────────────────────────────────────────────────────

Adafruit_SH1106G display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

/// Last known price for each symbol (index mirrors list_of_symbols[]).
double current_prices[size_of_list_of_symbols];

/// Midnight-UTC open price for each symbol, fetched once at boot.
double closing_prices[size_of_list_of_symbols];

int  symbol_index = 0;
long start_time   = 0;

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

  // Fetch initial prices for all configured symbols
  for (int i = 0; i < size_of_list_of_symbols; i++) {
    closing_prices[i] = get_closing_price(list_of_symbols[i]);
    current_prices[i] = get_current_price(list_of_symbols[i]);
  }

  // Show the first symbol immediately
  print_to_screen(display,
                  current_prices[0], current_prices[0],
                  closing_prices[0], list_of_symbols[0]);

  start_time = millis();
}

// ─────────────────────────────────────────────────────────────────────────────

void loop() {
  if (WiFi.status() != WL_CONNECTED) return;

  long now = millis();

  // Rotate to the next symbol once the display interval has elapsed
  if (now > start_time + (SECONDS_TO_DISPLAY_EACH_SYMBOL * 1000L)) {
    start_time   = now;
    symbol_index = (symbol_index + 1) % size_of_list_of_symbols;
  }

  double new_price = get_current_price(list_of_symbols[symbol_index]);

  // Only redraw when the price has actually changed (avoids flicker)
  if (new_price > 0 && new_price != current_prices[symbol_index]) {
    print_to_screen(display,
                    new_price,
                    current_prices[symbol_index],
                    closing_prices[symbol_index],
                    list_of_symbols[symbol_index]);
    current_prices[symbol_index] = new_price;
  }

  delay(poll_delay);
}
