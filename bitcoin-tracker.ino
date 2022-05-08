#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#define ARDUINOJSON_USE_DOUBLE 1
#include <ArduinoJson.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <avr/pgmspace.h>

#ifdef DEBUG
  #define DEBUG_PRINT(x)  Serial.println (x)
#else
  #define DEBUG_PRINT(x)
#endif

const char* ssid = "your_ssid_here";
const char* password = "wifi_pass_here";

// Create LCD object, with I2C address 0x3F y 16 columns x 2 rows
LiquidCrystal_I2C lcd(0x3F,16,2);

// Create custom symbols for LCD
byte arrow_up[8] = {
B00000,B00100,B01010,B10101,
B00100,B00100,B00100,B00000,};
byte arrow_down[8] = {
B00000,B00100,B00100,B00100,
B10001,B01010,B00100,B00000,};

String api_host = "http://192.168.1.113:3001";

String current_price_path = "/api/ticker/current_price/{symbol}";
String closing_price_path = "/api/ticker/closing_price/{symbol}";

// Add symbols for the tracker to follow: {"BTC", "ETH", "DOGE"}
String list_of_symbols[] = {"BTC", "ETH"};
#define SECONDS_TO_DISPLAY_EACH_SYMBOL 7

int size_of_list_of_symbols = sizeof(list_of_symbols)/sizeof(String);

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
  
  // Start LCD and turn backlight
  lcd.init();
  lcd.backlight();

  // Create custom chars
  lcd.createChar(0, arrow_up);
  lcd.createChar(1, arrow_down);

  // Set built-in led to output
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH);

  // Start Wi-Fi connection
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED){
    delay(1000);
    Serial.println(F("Connecting..."));
    lcd.clear();
    lcd.print(F("Connecting..."));
  }
  Serial.println(F("Connected!"));
  lcd.clear();
  lcd.print(F("Connected!"));

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
    delay(200);
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
    lcd.clear();
    lcd.print(F("Failed to parse JSON"));
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
    lcd.clear();
    lcd.print(F("Failed to parse JSON"));
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
  lcd.clear();
  
  // First row
  // Print up arrow, down arrow or equals sign
  lcd.setCursor(0,0);
  if (current_price > previous_price) {
    lcd.write(byte(0)); // arrow_up
  } else if (current_price < previous_price) {
    lcd.write(byte(1)); // arrow_down
  } else {
    lcd.print(F("="));     // same price
  }

  // Print current price
  lcd.setCursor(2,0);
  lcd.print(current_price);

  // Print symbol sign
  lcd.setCursor(10,0);
  lcd.print(F(" "));
  lcd.print(symbol.substring(0, 3));
  lcd.print(F("/$"));

  // Second row
  // Print change over closing price
  double price_diff = current_price - closing_price;
  lcd.setCursor(0,1);
  if (price_diff >= 0){
    lcd.print(F("+"));
  }
  lcd.print(price_diff);

  // Print change in percentage
  lcd.setCursor(9,1);
  lcd.print(F(" "));
  float perc_diff = (100 * current_price / closing_price) - 100;
  if (perc_diff >= 0){
    lcd.print(F("+"));
  }
  lcd.print(perc_diff);
  lcd.setCursor(15,1);
  lcd.print(F("%"));
}
