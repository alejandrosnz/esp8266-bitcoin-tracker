#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#define ARDUINOJSON_USE_DOUBLE 1
#include <ArduinoJson.h>
#include <Wire.h>
#include <avr/pgmspace.h>

#include "config.h"
#include "icons.h"

// Uncomment to enable verbose serial output
// #define DEBUG
#ifdef DEBUG
  #define DEBUG_PRINT(x) Serial.println(x)
#else
  #define DEBUG_PRINT(x)
#endif

#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Fonts/FreeMonoBold9pt7b.h>
Adafruit_SH1106G display = Adafruit_SH1106G(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

// ── Price storage ─────────────────────────────────────────────────────────────
// Simple parallel arrays indexed by position in list_of_symbols[].
// Replaces the old nested DynamicJsonDocument, eliminating heap fragmentation.
double current_prices[size_of_list_of_symbols];
double closing_prices[size_of_list_of_symbols];

int  symbol_index;
long start_time;

// ── setup ─────────────────────────────────────────────────────────────────────
void setup()
{
  Serial.begin(9600);

  // OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  delay(250);
  display.begin(OLED_I2C_ADDR, true);
  display.setTextColor(SH110X_WHITE);
  display.clearDisplay();
  display.println(F("Connecting..."));
  display.display();

  // Built-in LED: HIGH = off (active low on ESP8266)
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
  display.println(F("Connected to WiFi!"));
  display.display();

  // Fetch initial prices for every symbol
  for (int i = 0; i < size_of_list_of_symbols; i++) {
    closing_prices[i] = get_closing_price(list_of_symbols[i]);
    current_prices[i] = get_current_price(list_of_symbols[i]);
  }

  print_to_screen(current_prices[0], current_prices[0], closing_prices[0], list_of_symbols[0]);

  symbol_index = 0;
  start_time   = millis();
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop()
{
  if (WiFi.status() != WL_CONNECTED) return;

  long current_millis = millis();

  // Rotate symbol
  if (current_millis > start_time + (SECONDS_TO_DISPLAY_EACH_SYMBOL * 1000L)) {
    start_time = current_millis;
    symbol_index = (symbol_index + 1) % size_of_list_of_symbols;
  }

  double new_price = get_current_price(list_of_symbols[symbol_index]);

  if (new_price > 0 && new_price != current_prices[symbol_index]) {
    print_to_screen(new_price,
                    current_prices[symbol_index],
                    closing_prices[symbol_index],
                    list_of_symbols[symbol_index]);
    current_prices[symbol_index] = new_price;
  }

  delay(poll_delay);
}

// ── get_current_price ─────────────────────────────────────────────────────────
// Calls GET /api/v3/ticker/price?symbol={SYMBOL}USDT
// Response: {"symbol":"BTCUSDT","price":"60950.01000000"}  (~60 bytes)
//
// Uses:
//  • WiFiClientSecure with certificate check disabled (personal project)
//  • Reduced TLS buffers to save ~30 KB of heap vs default
//  • ArduinoJson filter  → only the "price" key is allocated in the document
//  • Stream-based parsing → response is never copied into a String
double get_current_price(const char* symbol) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setBufferSizes(TLS_READ_BUFFER, TLS_WRITE_BUFFER);

  HTTPClient http;
  String url = String(F("https://")) + BINANCE_HOST +
               F("/api/v3/ticker/price?symbol=") + symbol + F("USDT");
  DEBUG_PRINT(url);

  http.begin(client, url);
  digitalWrite(BUILTIN_LED, LOW);
  int code = http.GET();
  digitalWrite(BUILTIN_LED, HIGH);

  double price = -1;

  if (code == HTTP_CODE_OK) {
    // Filter: discard every field except "price"
    StaticJsonDocument<32> filter;
    filter[F("price")] = true;

    StaticJsonDocument<64> doc;
    DeserializationError err = deserializeJson(doc, http.getStream(),
                                               DeserializationOption::Filter(filter));
    if (err) {
      Serial.print(F("JSON error (current): "));
      Serial.println(err.f_str());
    } else {
      price = doc[F("price")].as<double>();
      DEBUG_PRINT(price);
    }
  } else {
    Serial.print(F("HTTP error (current): "));
    Serial.println(code);
  }

  http.end();
  return price;
}

// ── get_closing_price ─────────────────────────────────────────────────────────
// Calls GET /api/v3/klines?symbol={SYMBOL}USDT&interval=1d&limit=1
// Response: [[openTime,"openPrice","highPrice","lowPrice","closePrice",...]]
//
// klines[0][1] = open price of today's daily candle = midnight UTC open price
// This is semantically identical to CryptoCompare's OPENDAY field.
//
// The response is ~160 bytes; StaticJsonDocument<512> is plenty.
// No filter needed at this size – the full document fits comfortably.
double get_closing_price(const char* symbol) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setBufferSizes(TLS_READ_BUFFER, TLS_WRITE_BUFFER);

  HTTPClient http;
  String url = String(F("https://")) + BINANCE_HOST +
               F("/api/v3/klines?symbol=") + symbol + F("USDT&interval=1d&limit=1");
  DEBUG_PRINT(url);

  http.begin(client, url);
  digitalWrite(BUILTIN_LED, LOW);
  int code = http.GET();
  digitalWrite(BUILTIN_LED, HIGH);

  double price = -1;

  if (code == HTTP_CODE_OK) {
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    if (err) {
      Serial.print(F("JSON error (closing): "));
      Serial.println(err.f_str());
    } else {
      // Array layout: [[openTime, open, high, low, close, volume, ...]]
      price = doc[0][1].as<double>();
      DEBUG_PRINT(price);
    }
  } else {
    Serial.print(F("HTTP error (closing): "));
    Serial.println(code);
  }

  http.end();
  return price;
}

// ── print_to_screen ───────────────────────────────────────────────────────────
void print_to_screen(double current_price, double previous_price, double closing_price, const char* symbol) {
  display.clearDisplay();

  // Price value
  display.setTextSize(3);
  String current_price_str = String(current_price, 4);
  int dec_index = current_price_str.indexOf('.');
  if (current_price >= 1000000) {         // 1M+
    display.setCursor(0, 4);
    display.print(current_price_str.substring(0, dec_index));
  } else if (current_price >= 100000) {   // 100K–1M
    display.setCursor(5, 4);
    display.print(current_price_str.substring(0, dec_index));
  } else if (current_price >= 1000) {     // 1K–100K
    display.setCursor(5, 4);
    display.print(current_price_str.substring(0, dec_index));
    display.setTextSize(2);
    display.setCursor(dec_index * 17 + 5, 11);
    display.print(current_price_str.substring(dec_index, dec_index + (7 - dec_index)));
  } else if (current_price >= 10) {       // 10–1K
    display.setCursor(20, 4);
    display.print(current_price_str.substring(0, dec_index));
    display.setTextSize(2);
    display.setCursor(dec_index * 17 + 20, 11);
    display.print(current_price_str.substring(dec_index, dec_index + (6 - dec_index)));
  } else {                                // < $10
    display.setCursor(5, 4);
    display.print(current_price_str);
  }

  // $ sign
  if (current_price < 1000000) {
    display.setCursor(115, 11);
    display.setTextSize(2);
    display.print('$');
  }

  // Symbol label inside rounded rectangle
  display.setCursor(15, 37);
  display.setTextSize(2);
  display.print(symbol);  // already 3-char max per convention
  display.drawRoundRect(10, 32, 44, 24, 8, SH110X_WHITE);

  // Direction icon (vs midnight UTC open)
  double diff = (current_price - closing_price) / closing_price * 100;
  if      (diff >= 3.5)              display.drawBitmap(59, 37, bitmap_up_double,   ICON_WIDTH, ICON_HEIGHT, SH110X_WHITE);
  else if (diff >= 1.5)              display.drawBitmap(59, 37, bitmap_up_single,   ICON_WIDTH, ICON_HEIGHT, SH110X_WHITE);
  else if (diff >= 0)                display.drawBitmap(59, 37, bitmap_up_thin,     ICON_WIDTH, ICON_HEIGHT, SH110X_WHITE);
  else if (diff > -1.5)              display.drawBitmap(59, 37, bitmap_down_thin,   ICON_WIDTH, ICON_HEIGHT, SH110X_WHITE);
  else if (diff > -3.5)              display.drawBitmap(59, 37, bitmap_down_single, ICON_WIDTH, ICON_HEIGHT, SH110X_WHITE);
  else                               display.drawBitmap(59, 37, bitmap_down_double, ICON_WIDTH, ICON_HEIGHT, SH110X_WHITE);

  // Percentage (and optionally absolute) change
  float pct = abs(((current_price - closing_price) / closing_price) * 100);
  if (DIFF_PRINT_PERCENTAGE_AND_VALUE) {
    display.setCursor(80, 35);
    display.setTextSize(1);
    display.print(pct, 2);
    display.print('%');
    display.setCursor(80, 45);
    display.setTextSize(1);
    display.print(abs(current_price - closing_price), 2);
  } else {
    display.setCursor(80, 37);
    display.setTextSize(2);
    display.print(pct, (pct < 10) ? 1 : 0);
    if (pct >= 10) {
      display.setTextSize(1);
      display.print(' ');
      display.setTextSize(2);
    }
    display.print('%');
  }

  display.display();
}
