#include <Arduino.h> // Обов'язково для PlatformIO
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --- НАЛАШТУВАННЯ ---
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// 👇 ВСТАВ СЮДИ СВІЙ URL (без слеша в кінці) 👇
const String SERVER_URL = "https://donetskwaterhope.onrender.com"; 

const int DEVICE_ID = 1;
const String ACCOUNT_NUMBER = "WH-0MWXOUI0";
const String PASSWORD = "123456789";

// --- ЗМІННІ ---
String jwtToken = "";
long totalCounter = 0;
const int POT_PIN = 34; 

// Оголошення функцій (у C++ це бажано робити перед використанням)
bool login();
void syncLastValue();
void sendTelemetry(long value);
void sendAlert(String msg);

void setup() {
  Serial.begin(115200); // Це вже є
  Serial.println("\n\n--- SYSTEM START ---\n\n");
  
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  if (login()) {
    syncLastValue();
  }
}

void loop() {
  if (jwtToken == "") {
    Serial.println("Re-connecting login...");
    login();
    delay(5000);
    return;
  }

  int potValue = analogRead(POT_PIN);
  int flowRate = map(potValue, 0, 4095, 0, 10);
  
  if (flowRate > 0) {
    totalCounter += flowRate;
    Serial.printf("Flow: %d L. Total: %ld\n", flowRate, totalCounter);
    
    if (flowRate > 8) {
       sendAlert("CRITICAL: Pipe burst detected!");
    }
    
    sendTelemetry(totalCounter);
  } else {
    Serial.println("Water is OFF");
  }

  delay(5000); 
}

// --- РЕАЛІЗАЦІЯ ФУНКЦІЙ ---

bool login() {
  HTTPClient http;
  http.begin(SERVER_URL + "/api/auth/login");
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<200> doc;
  doc["accountNumber"] = ACCOUNT_NUMBER;
  doc["password"] = PASSWORD;
  String requestBody;
  serializeJson(doc, requestBody);

  int httpResponseCode = http.POST(requestBody);

  if (httpResponseCode == 200) {
    String response = http.getString();
    StaticJsonDocument<1024> responseDoc;
    deserializeJson(responseDoc, response);
    const char* token = responseDoc["token"];
    jwtToken = String(token);
    Serial.println("Login Success!");
    http.end();
    return true;
  } else {
    Serial.printf("Login Failed! Error: %d\n", httpResponseCode);
    http.end();
    return false;
  }
}

void syncLastValue() {
  if (jwtToken == "") return;
  
  HTTPClient http;
  http.begin(SERVER_URL + "/api/consumption/device/" + String(DEVICE_ID));
  http.addHeader("Authorization", "Bearer " + jwtToken);

  if (http.GET() == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, payload);
    
    if (doc.size() > 0) {
      long lastVal = doc[0]["value"];
      if (lastVal > totalCounter) {
        totalCounter = lastVal;
        Serial.printf("Synced! Continuing from: %ld\n", totalCounter);
      }
    }
  }
  http.end();
}

void sendTelemetry(long value) {
  HTTPClient http;
  http.begin(SERVER_URL + "/api/consumption");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + jwtToken);

  StaticJsonDocument<200> doc;
  doc["deviceId"] = DEVICE_ID;
  doc["currentValue"] = value;
  String body;
  serializeJson(doc, body);

  if (http.POST(body) == 201) Serial.println("Data Sent.");
  else Serial.println("Send Error.");
  http.end();
}

void sendAlert(String msg) {
  HTTPClient http;
  http.begin(SERVER_URL + "/api/alerts");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + jwtToken);
  StaticJsonDocument<200> doc;
  doc["deviceId"] = DEVICE_ID;
  doc["messageText"] = msg;
  doc["type"] = "Critical";
  String body;
  serializeJson(doc, body);
  http.POST(body);
  Serial.println("ALERT SENT!");
  http.end();
}