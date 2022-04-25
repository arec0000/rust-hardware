#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

const char* ssid = "TP-Link_ECB8";
const char* password = "06309794";

bool running = false;
int top;
int down;
int cycles;

float temp = 0;
bool lowered = false;
int currentCycle;
bool aborted = false;

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

void getData() {
  if (Firebase.getJSON(fbdo, "/current")) {
      if (fbdo.dataType() == "json") {
        FirebaseJson &json = fbdo.to<FirebaseJson>();
        FirebaseJsonData res;
        json.get(res, "running");
        if (res.success) {
          running = res.to<bool>();
        }
        json.get(res, "top");
        if (res.success) {
          top = res.to<int>();
        }
        json.get(res, "down");
        if (res.success) {
          down = res.to<int>();
        }
        json.get(res, "cycles");
        if (res.success) {
          cycles = res.to<int>();
        }
      }
    } else {
      Serial.print("Ошибка получения данных: ");
      Serial.println(fbdo.errorReason());
    }
}

void sendData() {
  //Отмечаем, что esp подключена
  if (Firebase.setBoolAsync(fbdo, "current/connected", true)) {
    Serial.println("connected установлено в true");
  } else {
    Serial.print("Ошибка при отправке connected: ");
    Serial.println(fbdo.errorReason());
  }

  if (Firebase.setFloatAsync(fbdo, "current/temp", temp)) {
    Serial.println("Температура отправлена");
  } else {
    Serial.print("Ошибка при отправке данных температуры: ");
    Serial.println(fbdo.errorReason());
  }

  if (Firebase.setBoolAsync(fbdo, "current/lowered", lowered)) {
    Serial.println("Положение отправлено");
  } else {
    Serial.print("Ошибка при отправке данных положения: ");
    Serial.println(fbdo.errorReason());
  }
  
  if (Firebase.setIntAsync(fbdo, "current/currentCycle", currentCycle)) {
    Serial.println("Текущий цикл отправлен");
  } else {
    Serial.print("Ошибка при отправке данных текущего цикла: ");
    Serial.println(fbdo.errorReason());
  }

  if (aborted) {
    Serial.println("Процесс прерван");
    if (Firebase.setBoolAsync(fbdo, "current/aborted", aborted)) {
      Serial.println("Отправлено оповещение о прерывании процесса");
    } else {
      Serial.print("Ошибка при отправке оповещения о прерывании процесса: ");
      Serial.println(fbdo.errorReason());
    }
  }
}

void setup() {
  Serial.begin(115200);
  connectToWifi();
  initFireBase();
  pinMode(21, OUTPUT);
}

int timer;

void loop() {
  if (millis() < timer) {
    timer = 0;
  }
  if (millis() - timer > 500 && Firebase.ready()) {

    getData();
    Serial.print(running);
    Serial.print(" ");
    Serial.print(top);
    Serial.print(" ");
    Serial.print(down);
    Serial.print(" ");
    Serial.println(cycles);

    if (running) {
      digitalWrite(21, HIGH);
    } else {
      digitalWrite(21, LOW);
    }

    temp += 0.3;
    lowered = !lowered;
    currentCycle += 1;

    sendData();

    timer = millis();
  }
}
