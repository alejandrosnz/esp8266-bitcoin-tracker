/**
 * @file config.h
 * @brief User-facing configuration for the Bitcoin tracker.
 *
 * This is the only file you need to edit before flashing:
 *  1. Set @c ssid and @c password to your WiFi credentials.
 *  2. Adjust @c list_of_symbols to the Binance base assets you want to track.
 *  3. Optionally tweak timing, OLED pins, or TLS buffer sizes.
 */
#ifndef CONFIG_H
#define CONFIG_H

// ── WiFi ─────────────────────────────────────────────────────────────────────
// static gives these internal linkage so each .cpp translation unit gets its
// own copy — prevents "multiple definition" linker errors when config.h is
// included by more than one .cpp file (api.cpp, display_utils.cpp, etc.).
static const char* const ssid     = "your_ssid_here";
static const char* const password = "wifi_pass_here";

// ── Binance API ───────────────────────────────────────────────────────────────
// Direct connection to Binance – no proxy needed.
//
// Endpoints used:
//   Current price : GET /api/v3/ticker/price?symbol={SYMBOL}USDT
//   Opening price : GET /api/v3/klines?symbol={SYMBOL}USDT&interval=1d&limit=1
//                   → klines[0][1] = daily open = midnight UTC (same as OPENDAY)
static const char* const BINANCE_HOST = "api.binance.com";

// TLS buffer sizes for BearSSL on ESP8266.
// Larger buffers (1024/1024) are more reliable than smaller ones (512/512),
// especially on slower WiFi or with higher network latency.
// If you run out of RAM or want aggressive optimisation, reduce to 512/512.
// If you get connection failures ("-5" errors), try increasing to 2048/2048.
#define TLS_READ_BUFFER  1024
#define TLS_WRITE_BUFFER 1024

// ── Symbols ───────────────────────────────────────────────────────────────────
// Add/remove symbols as needed. Each must be a valid Binance base asset
// traded against USDT (e.g. "BTC" → BTCUSDT).
static const char* const list_of_symbols[] = {"BTC", "ETH"};

// Seconds each symbol is shown on screen before rotating to the next one
#define SECONDS_TO_DISPLAY_EACH_SYMBOL 10

// false → show only the percentage change (larger font)
// true  → show percentage + absolute value change
#define DIFF_PRINT_PERCENTAGE_AND_VALUE false

// ── OLED ──────────────────────────────────────────────────────────────────────
#define OLED_SDA      D1
#define OLED_SCL      D2
#define OLED_I2C_ADDR 0x3c
#define OLED_WIDTH    128
#define OLED_HEIGHT   64
#define OLED_RESET    -1

// ── Polling ───────────────────────────────────────────────────────────────────
// Poll delay in milliseconds. 5000 ms gives the TLS handshake time to complete
// without stalling the display. Reduce only if your network is fast and stable.
const int poll_delay = 5000;

// ── Opening price refresh ─────────────────────────────────────────────────────
// When true, the midnight-UTC opening price for every symbol is re-fetched once
// per day at the moment the UTC calendar day rolls over (detected via NTP).
// This requires an NTP time sync during setup() — adds about 1–3 s to boot time
// and uses pool.ntp.org by default. Set to false to disable the daily refresh
// (prices will drift from the true daily-open reference after the first 24 h).
#define REFRESH_OPENING_PRICE_AT_MIDNIGHT true

const int size_of_list_of_symbols = sizeof(list_of_symbols) / sizeof(list_of_symbols[0]);

#endif // CONFIG_H
