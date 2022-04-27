#include <Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <FirebaseESP32.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

FirebaseAuth auth;
FirebaseConfig config;
FirebaseData fbdo;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

const char* ssid = "1812";
const char* password = "1q2w3e4r5t";

bool ready;
bool moving;
bool lowered;
int state = 0; // 0 - начало вверху 1 - опускается 2 - внизу 3 - поднимается 4 - вверху
int completedCycles;
int currentTime;
int targetTime;
float temp = 0;

struct ProcessInfo {
  bool running;
  bool completed;
  // bool aborted;
  int top;
  int down;
  int cycles;
  // int start;
  // int finish;
  // char* name;
  // char* abortReason;
  // char* description;
} processInfo;

int timer;

void connectToWifi() { //нужно переделать!
  WiFi.begin(ssid, password);
  Serial.print("Connection to wi-fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("connected");
}

void initFirebase() {
  config.host = "https://rust-74e51-default-rtdb.europe-west1.firebasedatabase.app";
  config.api_key = "AIzaSyARw8quvcK1eZjJurgsHYGEeIHFa3cfIoY";
  auth.user.email = "esp32@test.com";
  auth.user.password = "test13";
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void heartbeat() { //Отмечаем, что esp подключена
  if (!Firebase.setBoolAsync(fbdo, "current/connected", true)) 
    Serial.print("Ошибка оповещении о подключении: " + fbdo.errorReason());
}

void getData() {
  if (Firebase.getJSON(fbdo, "/current")) {
    if (fbdo.dataType() == "json") {
      FirebaseJson &json = fbdo.to<FirebaseJson>();
      FirebaseJsonData res;
      json.get(res, "running");
      if (res.success)
        processInfo.running = res.to<bool>();
      json.get(res, "completed");
      if (res.success)
        processInfo.completed = res.to<bool>();
      json.get(res, "top");
      if (res.success)
        processInfo.top = res.to<int>();
      json.get(res, "down");
      if (res.success)
        processInfo.down = res.to<int>();
      json.get(res, "cycles");
      if (res.success)
        processInfo.cycles = res.to<int>();
    }
  } else {
    Serial.print("Ошибка получения данных: " + fbdo.errorReason());
  }
}

void sendData() {
  if (!Firebase.setIntAsync(fbdo, "current/currentTime", currentTime))
    Serial.print("Ошибка при отправке данных текущего времени: " + fbdo.errorReason());
  if (!Firebase.setIntAsync(fbdo, "current/completedCycles", completedCycles))
    Serial.print("Ошибка при отправке данных текущего цикла: " + fbdo.errorReason());
  if (!Firebase.setBoolAsync(fbdo, "current/lowered", lowered))
    Serial.print("Ошибка при отправке данных положения: " + fbdo.errorReason());
}

void completeProcess() {
  processInfo.running = false;
  Firebase.setBoolAsync(fbdo, "current/running", false);
  Firebase.setBoolAsync(fbdo, "current/completed", true);
  Serial.println(F("Процесс завершён"));
}

void abortProcess(const char* reason) {
  processInfo.running = false;
  Firebase.setBoolAsync(fbdo, "current/running", false);
  Firebase.setBoolAsync(fbdo, "current/aborted", true);
  Firebase.setStringAsync(fbdo, "current/abortReason", reason);
  Serial.println(reason);
}

void processLoop() {
  if (processInfo.running && state == 0) {
    state = 1;
    //функция для опускания
    Serial.println(F("Процесс запущен"));
  }
  if (state == 1 && !moving) {
    state = 2;
    targetTime = currentTime + processInfo.down;
  }
  if (state == 2 && currentTime >= targetTime) {
    state = 3;
    //функция для подъёма
  }
  if (state == 3 && !moving) {
    state = 4;
    targetTime = currentTime + processInfo.top;
  }
  if (state == 4 && currentTime >= targetTime) {
    completedCycles++;
    Serial.println(String(completedCycles) + " цикл завершён");
    if (completedCycles >= processInfo.cycles) {
      state = 0;
      completeProcess();
      sendData();
    } else {
      state = 1;
      //функция для опускания
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(21, OUTPUT);
  connectToWifi();
  initFirebase();
  timeClient.begin();
}

void loop() {
  if (millis() < timer) timer = 0;

  if (Firebase.ready() && millis() - timer > 1000) {
    timeClient.update();
    currentTime = timeClient.getEpochTime();

    heartbeat();

    getData();

    //прерывание из приложения
    if (!processInfo.running && state > 0) {
      Serial.println(F("Процесс прерван из приложения"));
      if (state < 3) {
        //функция для подъёма
      }
      if (state > 0) {
        state = 0;
      }
    }

    //Если сразу же после включения процесс запущен, значит он прервался
    //Возможно это лучше сделать в сетапе, но не точно!
    if (!ready && processInfo.running)
      abortProcess("Прерван из-за отключения питания");
    else if (!ready)
      ready = true;

    if (!processInfo.completed) {
      if (Firebase.setFloatAsync(fbdo, "current/temp", temp))
        Serial.println("Температура отправлена");
      else
        Serial.print("Ошибка при отправке данных температуры: " + fbdo.errorReason());
    }

    processLoop();

    if (processInfo.running) {
      sendData();
    }

    //Сработал один из датчиков холла
    if (digitalRead(1)) {
      moving = false;
      lowered = true;
    }
    if (digitalRead(2)) {
      moving = false;
      lowered = false;
    }

    timer = millis();
  }
}
