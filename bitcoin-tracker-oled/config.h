#ifndef CONFIG_H
#define CONFIG_H

const char* ssid = "your_ssid_here";
const char* password = "wifi_pass_here";

String api_host = "http://192.168.1.113:3001";
String current_price_path = "/api/ticker/current_price/{symbol}";
String closing_price_path = "/api/ticker/closing_price/{symbol}";

String list_of_symbols[] = {"BTC", "ETH"};
#define SECONDS_TO_DISPLAY_EACH_SYMBOL 10
#define DIFF_PRINT_PERCENTAGE_AND_VALUE false

#define OLED_SDA D1
#define OLED_SCL D2
#define OLED_I2C_ADDR 0x3c
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_RESET -1

const int poll_delay = 1500;
const int size_of_list_of_symbols = sizeof(list_of_symbols) / sizeof(list_of_symbols[0]);

#endif // CONFIG_H 