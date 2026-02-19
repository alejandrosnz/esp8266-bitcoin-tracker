/**
 * @file api.cpp
 * @brief Binance REST API implementation.
 *
 * Uses WiFiClientSecure (BearSSL) with deliberately reduced TLS buffers to
 * keep heap usage around 28 KB instead of the default ~60 KB.  Certificate
 * verification is disabled (setInsecure) — acceptable for a personal project
 * where authenticity of the price data is not safety-critical.
 *
 * Both functions parse the HTTP response directly from the stream using
 * ArduinoJson, so the full payload is never materialised as a String.
 */

#include "api.h"

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

// Must be defined before including ArduinoJson to ensure 64-bit float arithmetic.
#define ARDUINOJSON_USE_DOUBLE 1
#include <ArduinoJson.h>

#include "config.h"
#include "debug.h"

// ─────────────────────────────────────────────────────────────────────────────
// Internal helper
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Apply standard TLS settings to a WiFiClientSecure instance.
 *
 * Called once per request before HTTPClient::begin().  Keeping this in one
 * place makes it easy to tighten security (e.g. add a certificate fingerprint)
 * without touching every call-site.
 */
static void configure_tls(WiFiClientSecure& client) {
  client.setInsecure();                              // skip certificate verification
  client.setBufferSizes(TLS_READ_BUFFER, TLS_WRITE_BUFFER);
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

double get_current_price(const char* symbol) {
  WiFiClientSecure client;
  configure_tls(client);

  HTTPClient http;
  String url = String(F("https://")) + BINANCE_HOST
             + F("/api/v3/ticker/price?symbol=") + symbol + F("USDT");
  DEBUG_PRINT(url);

  http.begin(client, url);
  digitalWrite(BUILTIN_LED, LOW);   // blink LED while request is in flight
  int code = http.GET();
  digitalWrite(BUILTIN_LED, HIGH);

  double price = -1;

  if (code == HTTP_CODE_OK) {
    // Discard everything except the single "price" field to minimise the
    // amount of memory ArduinoJson allocates for the parsed document.
    StaticJsonDocument<32> filter;
    filter[F("price")] = true;

    StaticJsonDocument<64> doc;
    DeserializationError err = deserializeJson(
        doc, http.getStream(), DeserializationOption::Filter(filter));

    if (err) {
      Serial.print(F("[api] JSON error (current): "));
      Serial.println(err.f_str());
    } else {
      price = doc[F("price")].as<double>();
      DEBUG_PRINT(price);
    }
  } else {
    Serial.print(F("[api] HTTP error (current): "));
    Serial.println(code);
  }

  http.end();
  return price;
}

double get_closing_price(const char* symbol) {
  WiFiClientSecure client;
  configure_tls(client);

  HTTPClient http;
  String url = String(F("https://")) + BINANCE_HOST
             + F("/api/v3/klines?symbol=") + symbol + F("USDT&interval=1d&limit=1");
  DEBUG_PRINT(url);

  http.begin(client, url);
  digitalWrite(BUILTIN_LED, LOW);
  int code = http.GET();
  digitalWrite(BUILTIN_LED, HIGH);

  double price = -1;

  if (code == HTTP_CODE_OK) {
    // The klines response is ~160 bytes for limit=1; a 512-byte document is
    // sufficient and a filter would add more complexity than it saves here.
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, http.getStream());

    if (err) {
      Serial.print(F("[api] JSON error (closing): "));
      Serial.println(err.f_str());
    } else {
      // Klines layout: [[openTime, open, high, low, close, volume, closeTime, ...]]
      price = doc[0][1].as<double>();
      DEBUG_PRINT(price);
    }
  } else {
    Serial.print(F("[api] HTTP error (closing): "));
    Serial.println(code);
  }

  http.end();
  return price;
}
