#ifndef CREDENTIALS_INFO
/*
 * PERHATIAN!
 * File ini telah disanitasi untuk publikasi ke repositori publik.
 * Gantilah semua placeholder kredensial di bawah ini dengan data asli Anda secara lokal.
 * Jangan pernah commit kredensial asli ke repositori publik.
 *
 * Untuk penggunaan produksi, gunakan Preferences ESP32 atau file konfigurasi eksternal yang tidak diupload ke repo.
 */
#define CREDENTIALS_INFO
#endif

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <GravityTDS.h>
#include <EEPROM.h>
#include <time.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <LiquidCrystal_I2C.h>
#define DEFAULT_WIFI_SSID "YOUR_WIFI_SSID"
#define DEFAULT_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define API_KEY "YOUR_FIREBASE_API_KEY"
#define DATABASE_URL "YOUR_FIREBASE_DATABASE_URL"
#define FIREBASE_PROJECT_ID "YOUR_FIREBASE_PROJECT_ID"
#define DEFAULT_USER_EMAIL "YOUR_FIREBASE_EMAIL"
#define DEFAULT_USER_PASSWORD "YOUR_FIREBASE_PASSWORD"

// Device ID
const String DEVICE_ID = "ESP32_001";

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
FirebaseJson firestoreData;

const int DS18B20_PIN = 4;
const int TDS_SENSOR_PIN = 36;
const int PH_SENSOR_PIN = 39;
const int DO_SENSOR_PIN = 33;

OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);
GravityTDS gravityTds;
float temperature = 25;
float tdsValue = 0;

LiquidCrystal_I2C lcd(0x27, 20, 4);

WebServer server(80);
Preferences preferences;

String wifiSSID;
String wifiPassword;
String userEmail;
String userPassword;

float readPH() {
  float Po = 0;
  float pH_step;
  int nilai_analog_pH;
  double TeganganpH;
  float pH4 = 4.4;
  float pH7 = 3.5;

  nilai_analog_pH = analogRead(PH_SENSOR_PIN);
  TeganganpH = 5 / 4095.0 * nilai_analog_pH;
  pH_step = (pH4 - pH7) / 3;
  Po = 7.00 + ((pH7 - TeganganpH) / pH_step);
  return Po;
}

float readDO() {
  const float VREF = 3300.0;
  const float ADC_RES = 4096.0;
  const float CAL1_V = 1100.0;
  const float CAL1_T = 17.0;
  const float CAL2_V = 1300.0; 
  const float CAL2_T = 15.0; 
  const float DO_Table[] = {14460, 14220, 13820, 13440, 13090, 12740, 12420, 12110, 11810, 11530, 11260, 11010, 10770, 10530, 10300, 10080, 9860, 9660, 9460, 9270, 9080, 8900, 8730, 8570, 8410, 8250, 8110, 7960, 7820, 7690, 7560, 7430, 7300, 7180, 7070, 6950, 6840, 6730, 6630, 6530, 6410};

  uint16_t ADC_Raw = analogRead(DO_SENSOR_PIN);
  float ADC_Voltage = (VREF * ADC_Raw) / ADC_RES; 
  float V_saturation;

#if TWO_POINT_CALIBRATION == 0
  V_saturation = CAL1_V + (35.0 * 17.0) - (CAL1_T * 35.0);
  return (ADC_Voltage * DO_Table[17] / V_saturation);
#else
  // V_saturation = ((CAL1_V - CAL2_V) / (CAL1_T - CAL2_T)) * (17.0 - CAL2_T) + CAL2_V;
  V_saturation = (int16_t)(17.0 - CAL2_T) * (CAL1_V - CAL2_V) / (CAL1_T - CAL2_T) + CAL2_V;
  return (ADC_Voltage * DO_Table[17] / V_saturation);
#endif
}


void handleRoot() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  File file = SPIFFS.open("/index.html", "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  String html = file.readString();
  file.close();
  server.send(200, "text/html", html);
}

void handleSetup() {
  String newSSID = server.arg("ssid");
  String newPassword = server.arg("password");
  String newEmail = server.arg("email");
  String newFirebasePassword = server.arg("firebasePassword");

  if (newSSID.length() > 0 && newPassword.length() > 0 && newEmail.length() > 0 && newFirebasePassword.length() > 0) {
    preferences.begin("wifi", false);
    preferences.putString("ssid", newSSID);
    preferences.putString("password", newPassword);
    preferences.putString("email", newEmail);
    preferences.putString("firebasePassword", newFirebasePassword);
    preferences.end();
    server.send(200, "text/html", "WiFi and Firebase setup complete. Restarting...");
    delay(2000);
    ESP.restart();
  } else {
    server.send(200, "text/html", "Invalid input.");
  }
}

void setup(){
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");
  
  preferences.begin("wifi", true);
  wifiSSID = preferences.getString("ssid", DEFAULT_WIFI_SSID);
  wifiPassword = preferences.getString("password", DEFAULT_WIFI_PASSWORD);
  userEmail = preferences.getString("email", DEFAULT_USER_EMAIL);
  userPassword = preferences.getString("firebasePassword", DEFAULT_USER_PASSWORD);
  preferences.end();

  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  Serial.print("Connecting to Wi-Fi");
  lcd.setCursor(0, 1);
  lcd.print("Connecting Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    lcd.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  lcd.setCursor(0, 2);
  lcd.print("WiFi Connected");

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = userEmail.c_str();
  auth.user.password = userPassword.c_str();

  config.token_status_callback = tokenStatusCallback; 
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  sensors.begin();
  pinMode(DO_SENSOR_PIN, INPUT);
  pinMode(PH_SENSOR_PIN, INPUT);
  pinMode(TDS_SENSOR_PIN, INPUT);

  EEPROM.begin(512);
  gravityTds.setPin(TDS_SENSOR_PIN);
  gravityTds.setAref(3.3);
  gravityTds.setAdcRange(4096);
  gravityTds.begin();

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  while (!time(nullptr)) {
    delay(1000);
    Serial.println("Waiting for time sync...");
  }
  Serial.println("Time synchronized.");

  server.on("/", handleRoot);
  server.on("/setup", handleSetup);
  server.begin();
  Serial.println("HTTP server started");

  lcd.setCursor(0, 3);
  lcd.print("Setup complete.");
}

void loop() {
  server.handleClient();

  static unsigned long lastLCDUpdate = 0;
  unsigned long currentTime = millis();
  if (currentTime - lastLCDUpdate >= 2000) {
    lastLCDUpdate = currentTime;
    displayLCD();
  }

  if (Firebase.ready()) {
    static unsigned long lastRealtime = 0;
    static unsigned long lastFirestore = 0;
    if (currentTime - lastRealtime >= 9000) {
      lastRealtime = currentTime;
      dataRealtime();
    }
    if (currentTime - lastFirestore >= 1800000) {
      lastFirestore = currentTime;
      dataFirestore();
    }
  } else {
    Serial.println("No internet connection, only updating LCD.");
  }
}

void displayLCD() {
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);
  gravityTds.setTemperature(temperatureC);
  gravityTds.update();
  float tdsValue = gravityTds.getTdsValue() * 4 / 100;
  float pHValue = readPH();
  float doValue = readDO();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(temperatureC, 2);
  lcd.print(" C");

  lcd.setCursor(0, 1);
  lcd.print("TDS: ");
  lcd.print(tdsValue, 2);
  lcd.print(" ppt");

  lcd.setCursor(0, 2);
  lcd.print("pH: ");
  lcd.print(pHValue, 2);

  lcd.setCursor(0, 3);
  lcd.print("DO: ");
  lcd.print(doValue, 2);
  lcd.print(" mg/L");
}

void dataRealtime(){
  sensors.requestTemperatures(); 
  float temperatureC = sensors.getTempCByIndex(0);
  gravityTds.setTemperature(temperatureC);  
  gravityTds.update();  
  float tdsValue = gravityTds.getTdsValue()*4/100;  
  float pHValue = readPH();
  float doValue = readDO();
  time_t now = time(nullptr);


  
  if (Firebase.ready()) {
    Serial.println("Firebase ready.");
    bool allDataSent = true;
    if (!Firebase.RTDB.setFloat(&fbdo, ("devices/" + DEVICE_ID + "/temperature").c_str(), temperatureC)) {
      allDataSent = false;
      Serial.print("Temperature data error: ");
      Serial.println(fbdo.errorReason());
    }
    if (!Firebase.RTDB.setFloat(&fbdo, ("devices/" + DEVICE_ID + "/TDS").c_str(), tdsValue)) {
      allDataSent = false;
      Serial.print("TDS data error: ");
      Serial.println(fbdo.errorReason());
    }
    if (!Firebase.RTDB.setFloat(&fbdo, ("devices/" + DEVICE_ID + "/pH").c_str(), pHValue)) {
      allDataSent = false;
      Serial.print("pH data error: ");
      Serial.println(fbdo.errorReason());
    }
    if (!Firebase.RTDB.setFloat(&fbdo, ("devices/" + DEVICE_ID + "/DO").c_str(), doValue)) {
      allDataSent = false;
      Serial.print("DO data error: ");
      Serial.println(fbdo.errorReason());
    }
    if (!Firebase.RTDB.setInt(&fbdo, ("devices/" + DEVICE_ID + "/heartbeat").c_str(), now)) {
      allDataSent = false;
      Serial.print("Send data error: ");
      Serial.println(fbdo.errorReason());
    }
    if (allDataSent) {
      Serial.println("Data Firebase sent successfully");
    }
  } else {
    Serial.println("Firebase not ready, connection issue?");
  }
}

void dataFirestore(){
  sensors.requestTemperatures(); 
  float temperatureC = sensors.getTempCByIndex(0);
  gravityTds.setTemperature(temperatureC);  
  gravityTds.update();  
  float tdsValue = gravityTds.getTdsValue()*4/100;  
  float pHValue = readPH();
  float voltage = analogRead(PH_SENSOR_PIN) * (5.0 / 4095.0);
  float doValue = readDO();
  String statusAir = determineStatusAir(pHValue, temperatureC, doValue, tdsValue);
  time_t now;
  time(&now);
  struct tm *timeinfo;
  char timeString[25];
  timeinfo = gmtime(&now);
  strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%S", timeinfo);
  strcat(timeString, ".000Z");

  firestoreData.set("fields/temperatureC/doubleValue", String(temperatureC).c_str());
  firestoreData.set("fields/tdsValue/doubleValue", String(tdsValue).c_str());
  firestoreData.set("fields/pHValue/doubleValue", String(pHValue).c_str());
  firestoreData.set("fields/doValue/doubleValue", String(doValue).c_str());
  firestoreData.set("fields/statusAir/stringValue", statusAir.c_str());
  firestoreData.set("fields/timestamp/timestampValue", timeString);

  String userId = String(auth.token.uid.c_str());
  String userPath = "users/" + userId;
  if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", userPath.c_str())) {
    FirebaseJson payload;
    payload.setJsonData(fbdo.payload().c_str());
    FirebaseJsonData jsonData;
    payload.get(jsonData, "fields/device/stringValue", true);
    Serial.print("Device ID: ");
    String deviceId = jsonData.stringValue;
    Serial.println(deviceId);
    String documentPath = "devices/" + deviceId + "/data/" + String(millis());
    if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), firestoreData.raw())) {
      Serial.println("Data berhasil disimpan ke Firestore");
    } else {
      Serial.print("Kesalahan menyimpan data ke Firestore: ");
      Serial.println(fbdo.errorReason());
    }
  }
}

String determineStatusAir(float pH, float temp, float doValue, float tds) {
  if ((pH >= 7.5 && pH <= 9.0) && (temp >= 20 && temp <= 30) && (doValue >= 4 && doValue <= 8) && (tds >= 5 && tds <= 30)) {
    return "Normal";
  } else if ((pH >= 7.0 && pH < 7.5) || (pH > 9.0 && pH <= 9.5) ||
              (temp >= 18 && temp < 20) || (temp > 30 && temp <= 32) ||
              (doValue >= 3 && doValue < 4) || (doValue > 8 && doValue <= 9) ||
              (tds >= 3 && tds < 5) || (tds > 30 && tds <= 35)) {
    return "Waspada";
  } else {
    return "Bahaya";
  }
}
