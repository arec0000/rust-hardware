#include <Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <FirebaseESP32.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

const char* ssid = "1812";
const char* password = "1q2w3e4r5t";

bool ready = false;

bool running = false;
int top;
int down;
int cycles;

float temp = 0;
bool lowered = false;
int currentCycle;
bool aborted = false;

//0 - выключен 1 - опускается
//2 - внизу 3 - поднимается
//4 - вверху
int state = 0;
bool moving = false;
int currentTime;
int targetTime;

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

void heartbeat() {
  //Отмечаем, что esp подключена
  if (Firebase.setBoolAsync(fbdo, "current/connected", true)) {
    Serial.println("connected установлено в true");
  } else {
    Serial.print("Ошибка при отправке connected: ");
    Serial.println(fbdo.errorReason());
  }
}

void sendData() {
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

  if (Firebase.setIntAsync(fbdo, "current/currentTime", currentTime)) {
    Serial.println("Текущее время отправлено");
  } else {
    Serial.print("Ошибка при отправке данных текущего времени: ");
    Serial.println(fbdo.errorReason());
  }
}

void setup() {
  Serial.begin(115200);
  connectToWifi();
  initFireBase();
  pinMode(21, OUTPUT);
  timeClient.begin();
}

int timer;

void loop() {
  if (millis() < timer) {
    timer = 0;
  }
  if (millis() - timer > 1000 && Firebase.ready()) {
    heartbeat();
    timeClient.update(); //обновляем время
    currentTime = timeClient.getEpochTime();

    getData();



    //Если сразу же после включения процесс запущен, значит он прервался
    if (running && !ready) {
      running = false;
      if (Firebase.setBoolAsync(fbdo, "current/running", false)) {
        Serial.println("Предыдущий процесс прерван");
      } else {
        Serial.print("Ошибка при прерывании предыдущего процесса: ");
        Serial.println(fbdo.errorReason());
      }
      if (Firebase.setBoolAsync(fbdo, "current/aborted", true)) {
        Serial.println("Предыдущий процесс прерван");
      } else {
        Serial.print("Ошибка при прерывании предыдущего процесса: ");
        Serial.println(fbdo.errorReason());
      }
      if (Firebase.setStringAsync(fbdo, "current/abortReason", "Прерван из-за отключения питания")) {
        Serial.println("Причина прерывания отправлена");
      } else {
        Serial.print("Ошибка при отправке причины прерывания: ");
        Serial.println(fbdo.errorReason());
      }
    } else {
      ready = true;
    }



    //прерывание из приложения
    if (!running) {
      if (state < 3) {
        //функция для подъёма
      }
      if (state > 0) {
        state = 0;
      }
    }



    if (running && state == 0) {
      state = 1;
      //функция для опускания
    }
    if (state == 1 && !moving) {
      state = 2;
      targetTime = currentTime + down;
    }
    if (state == 2 && currentTime >= targetTime) {
      state = 3;
      //функция для подъёма
    }
    if (state == 3 && !moving) {
      state = 4;
      targetTime = currentTime + top;
    }
    if (state == 4 && currentTime >= targetTime) {
      state = 0;
      currentCycle++;
      if (currentCycle >= cycles) {
        running = false;
        //отмечаем что процесс завершён
        //возможно меняем время завершения
      }
    }



    //////////
    if (running) {
      digitalWrite(21, HIGH);
    } else {
      digitalWrite(21, LOW);
    }
    Serial.println(state);
    //////////


    //Сработал один из датчиков холла
    if (digitalRead(1)) {
      moving = false;
      lowered = true;
    }
    if (digitalRead(2)) {
      moving = false;
      lowered = false;
    }



    if (running) { //возможно температуру стоит отправлять и до запуска, если процесс !comleted
      sendData();
    }

    

    timer = millis();
  }
}
