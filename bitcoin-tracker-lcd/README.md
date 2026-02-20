# ESP8266 Bitcoin Tracker — LCD Variant

> **Deprecated.** This variant is no longer maintained. Use [`bitcoin-tracker-oled`](../bitcoin-tracker-oled) instead — it connects directly to Binance with no proxy required.

---

This sketch displays live cryptocurrency prices on a **16×2 LCD** display (I2C). Unlike the current OLED version, it does **not** talk to Binance directly. Instead, it relies on a separate HTTP proxy server running on your local network.

## Prerequisites

### 1. Run the proxy server

This sketch requires the [bitcoin-tracker-proxy](https://github.com/alejandrosnz/bitcoin-tracker-proxy) to be running and reachable from your ESP8266 on the same network.

Follow the setup instructions in that repository (Node.js or Docker). Once running, the proxy exposes:

| Endpoint | Returns |
|----------|---------|
| `GET /api/ticker/current_price/{symbol}` | `{ "currentPrice": "..." }` |
| `GET /api/ticker/closing_price/{symbol}` | `{ "closingPrice": ... }` |

### 2. Hardware

- **ESP8266** (NodeMCU, Wemos D1 Mini, or compatible)
- **16×2 LCD** with I2C backpack (I2C address `0x3F`)
  - SDA → D1
  - SCL → D2

### 3. Required Libraries

Install via **Sketch** → **Include Library** → **Manage Libraries**:

- `ArduinoJson` by Benoit Blanchon
- `LiquidCrystal I2C` by Frank de Brabander

## Configuration

Edit `bitcoin-tracker-lcd.ino` before flashing:

```cpp
const char* ssid     = "your_ssid_here";
const char* password = "wifi_pass_here";

String api_host = "http://<proxy-ip>:3001";  // IP of the machine running the proxy
```

## Why is this deprecated?

The proxy architecture was introduced to work around memory limitations of the ESP8266 when parsing large API responses. The OLED variant has since solved this problem directly on the device using stream-based JSON parsing, eliminating the need for an external server entirely.

See [`bitcoin-tracker-oled`](../bitcoin-tracker-oled) for the current, actively maintained version.
