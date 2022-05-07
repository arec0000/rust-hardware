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

const char* ssid = "...";
const char* password = "20031983";

bool ready;
bool onInitialize;
bool inited;
int initMoment;

bool moving;
bool lowered;
int state = 0; // 0 - начало вверху 1 - опускается 2 - внизу 3 - поднимается 4 - вверху
int completedCycles;
int currentTime; //поменять на lastRunningTime или можно просто менять finish при завершении
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
    Serial.println("Ошибка оповещении о подключении: " + fbdo.errorReason());
}


//нужно разобраться как подписаться на изменения, не запрашивать если ничего не поменялось
//ну и в начале запросить
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
    Serial.println("Ошибка получения данных: " + fbdo.errorReason());
  }
}

void sendData() {
  if (!Firebase.setIntAsync(fbdo, "current/currentTime", currentTime))
    Serial.println("Ошибка при отправке данных текущего времени: " + fbdo.errorReason());
  if (!Firebase.setIntAsync(fbdo, "current/completedCycles", completedCycles))
    Serial.println("Ошибка при отправке данных текущего цикла: " + fbdo.errorReason());
  if (!Firebase.setBoolAsync(fbdo, "current/lowered", lowered))
    Serial.println("Ошибка при отправке данных положения: " + fbdo.errorReason());
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

void onWifiConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println(F("Подключено к wifi установлено"));
  if (!inited) {
    onInitialize = true;
    initMoment = millis();
  } else {
    sendData();
  }
}

void onWifiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println(F("Соединение с wifi разорвано, переподключение.."));
  if (!inited) onInitialize = false;
  WiFi.begin(ssid, password);
}

void initWifi() {
  WiFi.setAutoReconnect(true);
  WiFi.setAutoConnect(true);
  WiFi.persistent(true);
  WiFi.onEvent(onWifiConnected, SYSTEM_EVENT_STA_CONNECTED);
  WiFi.onEvent(onWifiDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);
  WiFi.begin(ssid, password);
  Serial.println(F("Подключение к wifi.."));
}

void initServices() {
  Serial.println(F("Инициализации служб"));
  initFirebase();
  timeClient.begin();
  onInitialize = false;
  inited = true;
}

void setup() {
  Serial.begin(115200);
  pinMode(21, OUTPUT);
  initWifi();
  //едем вверх
}

void loop() {
  if (millis() < timer) timer = 0; //сбрасываем таймер вместе с millis

  if (millis() - timer > 1000) {
    Serial.println(millis());

    if (WiFi.status() == WL_CONNECTED) {

      if (!inited && onInitialize && millis() - initMoment > 500)
        initServices();

      //костыль, чтобы время при обрыве связи отправилось не текущее время, а время завершения
      if (processInfo.running) {
        timeClient.update();
        currentTime = timeClient.getEpochTime();
      }

      if (Firebase.ready()) {

        heartbeat();
        getData();

        if (!ready && processInfo.running) 
          abortProcess("Прерван из-за отключения питания");
        else if (!ready) 
          ready = true;

        if (!processInfo.running && state > 0) {
          Serial.println(F("Процесс прерван из приложения"));
          if (state < 3) {
            //функция для подъёма
          }
          if (state > 0) state = 0;
        }

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

        //Сработал один из датчиков холла, здесь можно отправлять lowered, если запущен или движется
        // if (digitalRead(1)) {
        //   moving = false;
        //   lowered = true;
        // }
        // if (digitalRead(2)) {
        //   moving = false;
        //   lowered = false;
        // }
      }
    }

    timer = millis();
  }
}
