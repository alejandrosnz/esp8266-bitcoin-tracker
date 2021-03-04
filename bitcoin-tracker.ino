#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#define ARDUINOJSON_USE_DOUBLE 0
#include <ArduinoJson.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

const char* ssid = "your_ssid_here";
const char* password = "wifi_pass_here";

// Create LCD object, with I2C address 0x3F y 16 columns x 2 rows
LiquidCrystal_I2C lcd(0x3F,16,2);
byte arrow_up[8] = {
B00000,B00100,B01010,B10101,
B00100,B00100,B00100,B00000,};
byte arrow_down[8] = {
B00000,B00100,B00100,B00100,
B10001,B01010,B00100,B00000,};

String current_price_url = "http://api.coindesk.com/v1/bpi/currentprice.json";
String closing_price_url = "http://api.coindesk.com/v1/bpi/historical/close.json";

float previous_price;
float closing_price;

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
    Serial.println("Connecting...");
    lcd.clear();
    lcd.print("Connecting...");
  }
  Serial.println("Connected!");
  lcd.clear();
  lcd.print("Connected!");

  // Get initial price values
  closing_price = get_closing_price();
  previous_price = get_current_price();
  print_to_screen(previous_price, previous_price, closing_price);
}

void loop() 
{
  if (WiFi.status() == WL_CONNECTED) 
  {
    // Get new price
    float new_price = get_current_price();
    if (new_price != previous_price) {
      // Print to screen when price changes
      print_to_screen(new_price, previous_price, closing_price);
    
      // Store new previous price
      previous_price = new_price;
    }   
  }
  delay(5000);
}

float get_current_price() {
  // Returns the current BTC price
  
  String jsonBuffer = sendGET(current_price_url.c_str());
  Serial.println(jsonBuffer);

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, jsonBuffer);
  if (error) {
    Serial.print(F("Failed to parse JSON"));
    Serial.println(error.f_str());
    lcd.clear();
    lcd.print("Failed to parse JSON");
    return -1;
  }

  JsonObject bpi = doc["bpi"];
  JsonObject bpi_USD = bpi["USD"];
  float bpi_USD_rate_float = bpi_USD["rate_float"];

  return bpi_USD_rate_float;
}

float get_closing_price() {
  // Returns the BTC closing price from yesterday
  
  String jsonBuffer = sendGET(closing_price_url.c_str());
  Serial.println(jsonBuffer);

  DynamicJsonDocument doc(1536);
  DeserializationError error = deserializeJson(doc, jsonBuffer);
  if (error) {
    Serial.print(F("Failed to parse JSON"));
    Serial.println(error.f_str());
    lcd.clear();
    lcd.print("Failed to parse JSON");
    return -1;
  }

  JsonObject bpi = doc["bpi"];
  
  // Get last element
  float closing_price = 0;
  for (JsonPair kv : bpi) {
    closing_price = kv.value().as<float>();
  }

  return closing_price;
}


String sendGET(const char* url) {
  // Performs a GET operation to the given URL
  // and returns its response body
  
  HTTPClient http;
  
  // Your IP address with path or Domain name with URL path 
  http.begin(url);
  
  // Send HTTP POST request
  digitalWrite(BUILTIN_LED, LOW);
  int httpResponseCode = http.GET();
  digitalWrite(BUILTIN_LED, HIGH);
  
  String payload = "{}"; 
  
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("HTTP error! ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

void print_to_screen(float current_price, float previous_price, float closing_price) {
  // Prints to the LCD screen the current price,
  // and the difference to the closing price
  
  lcd.clear();
  
  // First row
  // Print up arrow, down arrow or equals sign
  lcd.setCursor(0,0);
  if (current_price > previous_price) {
    lcd.write(byte(0)); // arrow_up
  } else if (current_price < previous_price) {
    lcd.write(byte(1)); // arrow_down
  } else {
    lcd.print("=");     // same price
  }

  // Print current price
  lcd.setCursor(2,0);
  lcd.print(current_price);

  // Print BTC sign
  lcd.setCursor(10,0);
  lcd.print(" BTC/$");

  // Second row
  // Print change over closing price
  float price_diff = current_price - closing_price;
  lcd.setCursor(0,1);
  if (price_diff >= 0){
    lcd.print("+");
  }
  lcd.print(price_diff);

  // Print change in percentage
  lcd.setCursor(9,1);
  lcd.print(" ");
  float perc_diff = (100 * current_price / closing_price) - 100;
  if (perc_diff >= 0){
    lcd.print("+");
  }
  lcd.print(perc_diff);
  lcd.setCursor(15,1);
  lcd.print("%");
}
