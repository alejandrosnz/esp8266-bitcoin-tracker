/**
 * @file api.h
 * @brief Binance REST API helpers for real-time price data.
 *
 * Both functions open a fresh HTTPS connection, parse the response as a
 * stream (no intermediate String allocation), and close the connection.
 * TLS buffer sizes are read from config.h (TLS_READ_BUFFER / TLS_WRITE_BUFFER).
 */
#pragma once

/**
 * @brief Fetch the latest traded price for a symbol from Binance.
 *
 * Endpoint: GET https://api.binance.com/api/v3/ticker/price?symbol=<SYMBOL>USDT
 *
 * Example response (≈60 bytes):
 * @code
 * {"symbol":"BTCUSDT","price":"60950.01000000"}
 * @endcode
 *
 * Memory strategy:
 *  - WiFiClientSecure with reduced TLS buffers (see config.h)
 *  - ArduinoJson filter  → only the "price" key is allocated in the document
 *  - Stream-based deserialization → response is never copied into a String
 *
 * @param symbol  Binance base asset (e.g. "BTC", "ETH").
 * @return Price as double, or -1 on HTTP/parse error.
 */
double get_current_price(const char* symbol);

/**
 * @brief Fetch the midnight-UTC opening price for a symbol from Binance.
 *
 * Endpoint: GET https://api.binance.com/api/v3/klines?symbol=<SYMBOL>USDT&interval=1d&limit=1
 *
 * Example response (≈160 bytes):
 * @code
 * [[1499040000000,"60000.00","62000.00","59000.00","61000.00","12345.00",...]]
 * @endcode
 *
 * @c klines[0][1] is the open price of the current daily candle, which is the
 * price at exactly midnight UTC — semantically equivalent to CryptoCompare's
 * @c OPENDAY field. This value is used as the daily reference for % change.
 *
 * The response is small enough (≈160 bytes) that no filter is required;
 * a StaticJsonDocument<512> holds the full document comfortably.
 *
 * @param symbol  Binance base asset (e.g. "BTC", "ETH").
 * @return Opening price as double, or -1 on HTTP/parse error.
 */
double get_closing_price(const char* symbol);
