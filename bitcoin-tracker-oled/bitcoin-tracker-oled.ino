#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#define ARDUINOJSON_USE_DOUBLE 1
#include <ArduinoJson.h>
#include <Wire.h> 
#include <avr/pgmspace.h>

#include "config.h"
#include "icons.h"

// #define DEBUG
#ifdef DEBUG
  #define DEBUG_PRINT(x)  Serial.println (x)
#else
  #define DEBUG_PRINT(x)
#endif

#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Fonts/FreeMonoBold9pt7b.h>
Adafruit_SH1106G display = Adafruit_SH1106G(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

// Create custom symbols for LCD
byte arrow_up[8] = {
B00000,B00100,B01010,B10101,
B00100,B00100,B00100,B00000,};
byte arrow_down[8] = {
B00000,B00100,B00100,B00100,
B10001,B01010,B00100,B00000,};

#define JSON_SIZE_PER_SYMBOL 75
#define CLOSING_PRICE "clse"
#define CURRENT_PRICE "curr"

DynamicJsonDocument list_of_prices(size_of_list_of_symbols * JSON_SIZE_PER_SYMBOL);
DynamicJsonDocument json_doc(1000);

int symbol_index;
long start_time;

void setup() 
{
  // Start Serial Port with baud rate 9600
  Serial.begin(9600);
  
  // OLED setup
  Wire.begin(OLED_SDA, OLED_SCL);
  delay(250);
  display.begin(OLED_I2C_ADDR, true);
  display.setTextColor(SH110X_WHITE);
  display.clearDisplay();
  display.println("Connecting...");
  display.display();
  
  // // Create custom chars
  // lcd.createChar(0, arrow_up);
  // lcd.createChar(1, arrow_down);

  // Set built-in led to output
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH);

  // Start Wi-Fi connection
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED){
    delay(1000);
    Serial.print(".");
  }
  Serial.print("Connected to WiFi! IP: ");
  Serial.println(WiFi.localIP());
  display.clearDisplay();
  display.println("Connected to WiFi!");
  display.display();

  // Get initial price values
  for (byte i = 0; i < size_of_list_of_symbols; i++) {
    String symbol = list_of_symbols[i];
    list_of_prices[symbol][CLOSING_PRICE] = get_closing_price(symbol);
    list_of_prices[symbol][CURRENT_PRICE] = get_current_price(symbol);
  }
  // Print first price values
  print_to_screen(list_of_prices[list_of_symbols[0]][CURRENT_PRICE], \
                  list_of_prices[list_of_symbols[0]][CURRENT_PRICE], \
                  list_of_prices[list_of_symbols[0]][CLOSING_PRICE], \
                  list_of_symbols[0]);
  
  symbol_index = 0;
  start_time = millis();
}

void loop() 
{
  String symbol;

  if (WiFi.status() == WL_CONNECTED) {  
    long current_millis = millis();

    if(current_millis > start_time + (SECONDS_TO_DISPLAY_EACH_SYMBOL * 1000)){
      start_time = current_millis;
      symbol_index += 1;
      if (symbol_index >= size_of_list_of_symbols) {
        symbol_index = 0;
      }
    }
    symbol = list_of_symbols[symbol_index];
    
    double new_price = get_current_price(symbol);
    if (new_price != list_of_prices[symbol][CURRENT_PRICE]) {
      print_to_screen(new_price, \
                      list_of_prices[symbol][CURRENT_PRICE], \
                      list_of_prices[symbol][CLOSING_PRICE], \
                      symbol);

      list_of_prices[symbol][CURRENT_PRICE] = new_price;
    }
    delay(poll_delay);
  }
}

double get_current_price(String symbol) {
  String _current_price_path = current_price_path;
  _current_price_path.replace(F("{symbol}"), symbol);
  String jsonBuffer = sendGET(api_host, _current_price_path);
  DEBUG_PRINT(jsonBuffer);

  DeserializationError error = deserializeJson(json_doc, jsonBuffer);
  if (error) {
    Serial.print(F("Failed to parse JSON"));
    Serial.println(error.f_str());
    display.clearDisplay();
    display.print(F("Failed to parse JSON"));
    display.display();
    return -1;
  }

  String current_price = json_doc[F("currentPrice")];

  return current_price.toDouble();
}

double get_closing_price(String symbol) {
  String _closing_price_path = closing_price_path;
  _closing_price_path.replace(F("{symbol}"), symbol);
  String jsonBuffer = sendGET(api_host, _closing_price_path);
  DEBUG_PRINT(jsonBuffer);

  DeserializationError error = deserializeJson(json_doc, jsonBuffer);
  if (error) {
    Serial.print(F("Failed to parse JSON"));
    Serial.println(error.f_str());
    display.clearDisplay();
    display.print(F("Failed to parse JSON"));
    display.display();
    return -1;
  }

  double closing_price = json_doc[F("closingPrice")];

  return closing_price;
}


String sendGET(String _api_host, String _path) {
  String url = _api_host + _path;
  
  WiFiClient client;
  HTTPClient http;

  DEBUG_PRINT(url);
  http.begin(client, url);
    
  // Send HTTP POST request
  digitalWrite(BUILTIN_LED, LOW);
  int httpResponseCode = http.GET();
  digitalWrite(BUILTIN_LED, HIGH);
  
  String payload = "{}"; 
  
  if (httpResponseCode > 0) {
    Serial.print(F("HTTP Response code: "));
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print(F("HTTP error! "));
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

void print_to_screen(double current_price, double previous_price, double closing_price, String symbol) {
  display.clearDisplay();

  // Print PRICE
  display.setTextSize(3);
  String current_price_str = String(current_price, 4);
  int dec_index = current_price_str.indexOf('.');
  if (current_price >= 1000000) {  // + 1M
    display.setCursor(0, 4);
    display.print(current_price_str.substring(0, dec_index));
  } else if (current_price >= 100000) { // 100K to 1M
    display.setCursor(5, 4);
    display.print(current_price_str.substring(0, dec_index));
  } else if (current_price >= 1000) {   // 1K to 100K
    display.setCursor(5, 4);
    display.print(current_price_str.substring(0, dec_index));
    display.setTextSize(2);
    display.setCursor(dec_index * 17 + 5, 11);
    display.print(current_price_str.substring(dec_index, dec_index + (7 - dec_index)));
  } else if (current_price >= 10) {     // 10 to 1K
    display.setCursor(20, 4);
    display.print(current_price_str.substring(0, dec_index));
    display.setTextSize(2);
    display.setCursor(dec_index * 17 + 20, 11);
    display.print(current_price_str.substring(dec_index, dec_index + (6 - dec_index)));
  } else {  // less than 10$
    display.setCursor(5, 4);
    display.print(current_price_str);
  }

  // Print $ sign
  if (current_price < 1000000) {
    display.setCursor(115, 11);
    display.setTextSize(2);
    display.print("$");
  }

  // Print Crypto symbol
  display.setCursor(15, 37);
  display.setTextSize(2);
  display.print(symbol.substring(0, 3));

  int x = 15;
  int y = 37;
  display.drawRoundRect(x - 5 , y - 5, 44, 24, 8, SH110X_WHITE);

  // Print UP or DOWN
  double diff = (current_price - closing_price) / closing_price * 100;
  if (diff >= 3.5) {
    display.drawBitmap(59, 37, bitmap_up_double, ICON_WIDTH, ICON_HEIGHT, SH110X_WHITE);
  } else if (diff >= 1.5 && diff < 3.5) {
    display.drawBitmap(59, 37, bitmap_up_single, ICON_WIDTH, ICON_HEIGHT, SH110X_WHITE);
  } else if (diff >= 0 && diff < 1.5) {
    display.drawBitmap(59, 37, bitmap_up_thin, ICON_WIDTH, ICON_HEIGHT, SH110X_WHITE);
  } else if (diff < 0 && diff > -1.5) {
    display.drawBitmap(59, 37, bitmap_down_thin, ICON_WIDTH, ICON_HEIGHT, SH110X_WHITE);
  } else if (diff <= -1.5 && diff > -3.5) {
    display.drawBitmap(59, 37, bitmap_down_single, ICON_WIDTH, ICON_HEIGHT, SH110X_WHITE);
  } else if (diff <= -3.5) {
    display.drawBitmap(59, 37, bitmap_down_double, ICON_WIDTH, ICON_HEIGHT, SH110X_WHITE);
  }

  if (DIFF_PRINT_PERCENTAGE_AND_VALUE == true) {
    // Print diff %
    display.setCursor(80, 35);
    display.setTextSize(1);
    display.print(abs(((current_price - closing_price) / closing_price) * 100), 2);
    display.print("%");

    // // Print diff num
    display.setCursor(80, 45);
    display.setTextSize(1);
    display.print(abs(current_price - closing_price), 2);

  } else {
    // Print larger diff %
    display.setCursor(80, 37);
    display.setTextSize(2);
    display.print(abs(((current_price - closing_price) / closing_price) * 100), 1);
    display.print("%");
  }

  // Print reference date
  // display.setCursor(110, 40);
  // display.setTextSize(1);
  // display.print("1D");

  // Refresh
  display.display();
}
