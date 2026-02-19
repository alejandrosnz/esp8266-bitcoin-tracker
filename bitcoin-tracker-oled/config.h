#ifndef CONFIG_H
#define CONFIG_H

// ── WiFi ─────────────────────────────────────────────────────────────────────
const char* ssid     = "your_ssid_here";
const char* password = "wifi_pass_here";

// ── Binance API ───────────────────────────────────────────────────────────────
// Direct connection to Binance – no proxy needed.
//
// Endpoints used:
//   Current price : GET /api/v3/ticker/price?symbol={SYMBOL}USDT
//   Opening price : GET /api/v3/klines?symbol={SYMBOL}USDT&interval=1d&limit=1
//                   → klines[0][1] = daily open = midnight UTC (same as OPENDAY)
const char* BINANCE_HOST = "api.binance.com";

// TLS buffer sizes for BearSSL on ESP8266.
// 512/512 keeps RAM usage low (~28 KB vs the default ~60 KB).
// If you get connection failures, increase TLS_READ_BUFFER to 1024 or 4096.
#define TLS_READ_BUFFER  512
#define TLS_WRITE_BUFFER 512

// ── Symbols ───────────────────────────────────────────────────────────────────
// Add/remove symbols as needed. Each must be a valid Binance base asset
// traded against USDT (e.g. "BTC" → BTCUSDT).
const char* list_of_symbols[] = {"BTC", "ETH"};

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

const int size_of_list_of_symbols = sizeof(list_of_symbols) / sizeof(list_of_symbols[0]);

#endif // CONFIG_H
