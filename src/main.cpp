#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

const char* ssid = "1812";
const char* password = "1q2w3e4r5t";

void connectToWifi() {
  WiFi.begin(ssid, password);
  Serial.print("Connection to wi-fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("connected");
}

void initFireBase() {
  config.host = "https://rust-74e51-default-rtdb.europe-west1.firebasedatabase.app";
  config.api_key = "AIzaSyARw8quvcK1eZjJurgsHYGEeIHFa3cfIoY";
  auth.user.email = "esp32@test.com";
  auth.user.password = "test13";

  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void setup() {
  Serial.begin(115200);
  connectToWifi();
  initFireBase();
  pinMode(21, OUTPUT);
}

int timer;

void loop() {
  if (millis() - timer > 500 && Firebase.ready()) {
    if (Firebase.getBool(fbdo, "/led")) {
      if  (fbdo.dataType() == "boolean") {
        int led = fbdo.boolData();
        if (led) {
          digitalWrite(21, HIGH);
        } else {
          digitalWrite(21, LOW);
        }
        Serial.println(led);
      }
    } else {
      Serial.println(fbdo.errorReason());
    }

    timer = millis();
  }
}
