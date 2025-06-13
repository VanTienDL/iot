#include <Arduino_FreeRTOS.h>
#include <queue.h>
#include <semphr.h>

constexpr uint8_t PIN_BOM = 7;
constexpr uint8_t PIN_RELAY_DEN = 6;
constexpr uint8_t PIN_SENSOR_DO_AM = A0;
constexpr uint8_t PIN_SENSOR_ANH_SANG = A1;
constexpr uint8_t PIN_LED = 13;
constexpr uint8_t PIN_FIRE_SENSOR = 10;
constexpr uint8_t PIN_BUZZER = 12;

QueueHandle_t queueDoAm;
QueueHandle_t queueAnhSang;
QueueHandle_t queueLua;
SemaphoreHandle_t mutexCheDo;

enum class CheDo : uint8_t { TU_DONG, TAY_BAT, TAY_TAT };
volatile CheDo cheDoBom = CheDo::TU_DONG;
volatile CheDo cheDoDen = CheDo::TU_DONG;
volatile CheDo cheDoBuzzer = CheDo::TU_DONG;
volatile bool justSwitchedAutoBom = false;
volatile bool justSwitchedAutoDen = false;
volatile bool justSwitchedAutoBuzzer = false;

TickType_t lastChangeBom = 0;
TickType_t lastChangeDen = 0;
TickType_t lastChangeBuzzer = 0;
bool bomOn = false;

void taskDocCamBien(void* pvParameters) {
  uint16_t doAm, anhSang;
  uint16_t lua;

  while (true) {
    doAm = analogRead(PIN_SENSOR_DO_AM);
    anhSang = analogRead(PIN_SENSOR_ANH_SANG);
    lua = digitalRead(PIN_FIRE_SENSOR);

    xQueueOverwrite(queueDoAm, &doAm);
    xQueueOverwrite(queueAnhSang, &anhSang);
    xQueueOverwrite(queueLua, &lua);

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void taskXuLyDieuKhien(void* pvParameters) {
  uint16_t doAm = 0, anhSang = 0, lua = 0;
  static bool prevBomState = false;
  static bool prevDenState = false;
  static bool prevBuzzerState = false;

  while (true) {
    xQueuePeek(queueDoAm, &doAm, 0);
    xQueuePeek(queueAnhSang, &anhSang, 0);
    xQueuePeek(queueLua, &lua, 0);

    CheDo localDen, localBom, localBuzzer;
    bool localJustDen, localJustBom, localJustBuzzer;
    if (xSemaphoreTake(mutexCheDo, pdMS_TO_TICKS(10)) == pdTRUE) {
      localDen = cheDoDen;
      localBom = cheDoBom;
      localBuzzer = cheDoBuzzer;
      localJustDen = justSwitchedAutoDen;
      localJustBom = justSwitchedAutoBom;
      localJustBuzzer = justSwitchedAutoBuzzer;
      xSemaphoreGive(mutexCheDo);
    } else {
      localDen = CheDo::TU_DONG;
      localBom = CheDo::TU_DONG;
      localBuzzer = CheDo::TU_DONG;
      localJustDen = localJustBom = localJustBuzzer = false;
    }

    // Đèn
    if (localDen == CheDo::TU_DONG) {
      bool denMoi = (anhSang > 900) ? true : (anhSang < 750) ? false : prevDenState;
      if (denMoi != prevDenState || localJustDen) {
        prevDenState = denMoi;
        digitalWrite(PIN_RELAY_DEN, denMoi);
      }
    } else if (localDen == CheDo::TAY_BAT) {
      prevDenState = true;
      digitalWrite(PIN_RELAY_DEN, HIGH);
    } else {
      prevDenState = false;
      digitalWrite(PIN_RELAY_DEN, LOW);
    }

    // Bơm
    if (localBom == CheDo::TU_DONG) {
      bool bomMoi = (doAm > 500);
      if (bomMoi != prevBomState || localJustBom) {
        prevBomState = bomMoi;
        bomOn = bomMoi;
        digitalWrite(PIN_BOM, bomOn);
        digitalWrite(PIN_LED, bomOn);
        if (localJustBom) {
          xSemaphoreTake(mutexCheDo, pdMS_TO_TICKS(10));
          justSwitchedAutoBom = false;
          xSemaphoreGive(mutexCheDo);
        }
      }
    } else {
      bool manual = (localBom == CheDo::TAY_BAT);
      prevBomState = manual;
      bomOn = manual;
      digitalWrite(PIN_BOM, bomOn);
      digitalWrite(PIN_LED, bomOn);
    }

    // Buzzer
    if (localBuzzer == CheDo::TU_DONG) {
      bool buzzerOn = (lua == HIGH);
      if (buzzerOn != prevBuzzerState || localJustBuzzer) {
        prevBuzzerState = buzzerOn;
        digitalWrite(PIN_BUZZER, buzzerOn ? HIGH : LOW);
      }
    } else if (localBuzzer == CheDo::TAY_BAT) {
      prevBuzzerState = true;
      digitalWrite(PIN_BUZZER, HIGH);
    } else {
      prevBuzzerState = false;
      digitalWrite(PIN_BUZZER, LOW);
    }

    if (xSemaphoreTake(mutexCheDo, pdMS_TO_TICKS(10)) == pdTRUE) {
      justSwitchedAutoDen = false;
      justSwitchedAutoBuzzer = false;
      xSemaphoreGive(mutexCheDo);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void taskPrintStatus(void* pvParameters) {
  uint16_t doAm, anhSang, lua;

  while (true) {
    if (xQueuePeek(queueDoAm, &doAm, 0) &&
        xQueuePeek(queueAnhSang, &anhSang, 0) &&
        xQueuePeek(queueLua, &lua, 0)) {

      bool localBomOn;
      CheDo localDen;
      if (xSemaphoreTake(mutexCheDo, pdMS_TO_TICKS(10)) == pdTRUE) {
        localBomOn = bomOn;
        localDen = cheDoDen;
        xSemaphoreGive(mutexCheDo);
      } else {
        localBomOn = false;
        localDen = CheDo::TU_DONG;
      }

      Serial.print(F("DoAm: ")); Serial.println(doAm);
      Serial.print(F("AnhSang: ")); Serial.println(anhSang);
      Serial.print(F("Lua: ")); Serial.println(lua ? F("ON") : F("OFF"));
      Serial.print(F("Bom: ")); Serial.println(localBomOn ? F("ON") : F("OFF"));
      Serial.print(F("Den: ")); Serial.println(digitalRead(PIN_RELAY_DEN) ? F("BAT") : F("TAT"));
      Serial.print(F("Buzzer: ")); Serial.println(digitalRead(PIN_BUZZER) ? F("ON") : F("OFF"));
      Serial.println(F("------"));
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void taskNhanLenhTuPC(void* pvParameters) {
  char cmdBuffer[16];
  uint8_t idx = 0;

  while (true) {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') {
        if (idx > 0) {
          cmdBuffer[idx] = '\0';
          for (uint8_t i = 0; i < idx; i++) cmdBuffer[i] = toupper(cmdBuffer[i]);

          if (xSemaphoreTake(mutexCheDo, portMAX_DELAY) == pdTRUE) {
            if (strcmp(cmdBuffer, "DEN BAT") == 0) {
              cheDoDen = CheDo::TAY_BAT;
              lastChangeDen = xTaskGetTickCount();
              Serial.println(F("Den: MAN ON"));
            } else if (strcmp(cmdBuffer, "DEN TAT") == 0) {
              cheDoDen = CheDo::TAY_TAT;
              lastChangeDen = xTaskGetTickCount();
              Serial.println(F("Den: MAN OFF"));
            } else if (strcmp(cmdBuffer, "DEN AUTO") == 0) {
              cheDoDen = CheDo::TU_DONG;
              justSwitchedAutoDen = true;
              Serial.println(F("Den: AUTO"));
            } else if (strcmp(cmdBuffer, "BOM BAT") == 0) {
              cheDoBom = CheDo::TAY_BAT;
              lastChangeBom = xTaskGetTickCount();
              Serial.println(F("Bom: MAN ON"));
            } else if (strcmp(cmdBuffer, "BOM TAT") == 0) {
              cheDoBom = CheDo::TAY_TAT;
              lastChangeBom = xTaskGetTickCount();
              Serial.println(F("Bom: MAN OFF"));
            } else if (strcmp(cmdBuffer, "BOM AUTO") == 0) {
              cheDoBom = CheDo::TU_DONG;
              justSwitchedAutoBom = true;
              Serial.println(F("Bom: AUTO"));
            } else if (strcmp(cmdBuffer, "BUZZER BAT") == 0) {
              cheDoBuzzer = CheDo::TAY_BAT;
              lastChangeBuzzer = xTaskGetTickCount();
              Serial.println(F("Buzzer: MAN ON"));
            } else if (strcmp(cmdBuffer, "BUZZER TAT") == 0) {
              cheDoBuzzer = CheDo::TAY_TAT;
              lastChangeBuzzer = xTaskGetTickCount();
              Serial.println(F("Buzzer: MAN OFF"));
            } else if (strcmp(cmdBuffer, "BUZZER AUTO") == 0) {
              cheDoBuzzer = CheDo::TU_DONG;
              justSwitchedAutoBuzzer = true;
              Serial.println(F("Buzzer: AUTO"));
            } else if (strcmp(cmdBuffer, "AUTO ALL") == 0) {
              cheDoDen = CheDo::TU_DONG;
              cheDoBom = CheDo::TU_DONG;
              cheDoBuzzer = CheDo::TU_DONG;
              justSwitchedAutoDen = true;
              justSwitchedAutoBom = true;
              justSwitchedAutoBuzzer = true;
              lastChangeDen = xTaskGetTickCount();
              lastChangeBom = xTaskGetTickCount();
              lastChangeBuzzer = xTaskGetTickCount();
              Serial.println(F("All devices set to AUTO mode"));
            } else {
              Serial.println(F("Invalid cmd"));
            }
            xSemaphoreGive(mutexCheDo);
          }
          idx = 0;
        }
      } else if (idx < sizeof(cmdBuffer) - 1) {
        cmdBuffer[idx++] = c;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void taskAutoModeSwitch(void* pvParameters) {
  while (true) {
    TickType_t now = xTaskGetTickCount();
    
    if (xSemaphoreTake(mutexCheDo, pdMS_TO_TICKS(10)) == pdTRUE) {
      // Bom
      if (cheDoBom == CheDo::TAY_BAT && now - lastChangeBom >= pdMS_TO_TICKS(10000)) {
        cheDoBom = CheDo::TAY_TAT;
        lastChangeBom = now;
        Serial.println(F("Bom auto OFF sau 10s MAN ON"));
      } else if (cheDoBom == CheDo::TAY_TAT && now - lastChangeBom >= pdMS_TO_TICKS(10000)) {
        cheDoBom = CheDo::TU_DONG;
        justSwitchedAutoBom = true;
        Serial.println(F("Bom auto AUTO sau 10s MAN OFF"));
      }
      // Den
      if (cheDoDen == CheDo::TAY_BAT && now - lastChangeDen >= pdMS_TO_TICKS(10000)) {
        cheDoDen = CheDo::TAY_TAT;
        lastChangeDen = now;
        Serial.println(F("Den auto OFF sau 10s MAN ON"));
      } else if (cheDoDen == CheDo::TAY_TAT && now - lastChangeDen >= pdMS_TO_TICKS(10000)) {
        cheDoDen = CheDo::TU_DONG;
        justSwitchedAutoDen = true;
        Serial.println(F("Den auto AUTO sau 10s MAN OFF"));
      }
      // Buzzer
      if (cheDoBuzzer == CheDo::TAY_BAT && now - lastChangeBuzzer >= pdMS_TO_TICKS(10000)) {
        cheDoBuzzer = CheDo::TAY_TAT;
        lastChangeBuzzer = now;
        Serial.println(F("Buzzer auto OFF sau 10s MAN ON"));
      } else if (cheDoBuzzer == CheDo::TAY_TAT && now - lastChangeBuzzer >= pdMS_TO_TICKS(10000)) {
        cheDoBuzzer = CheDo::TU_DONG;
        justSwitchedAutoBuzzer = true;
        Serial.println(F("Buzzer auto AUTO sau 10s MAN OFF"));
      }
      xSemaphoreGive(mutexCheDo);
    }
    
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BOM, OUTPUT);
  pinMode(PIN_RELAY_DEN, OUTPUT);
  pinMode(PIN_FIRE_SENSOR, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  mutexCheDo = xSemaphoreCreateMutex();
  if (mutexCheDo == NULL) {
    Serial.println(F("Failed to create mutex"));
    while (true);
  }

  queueDoAm = xQueueCreate(1, sizeof(uint16_t));
  queueAnhSang = xQueueCreate(1, sizeof(uint16_t));
  queueLua = xQueueCreate(1, sizeof(uint16_t));

  xTaskCreate(taskXuLyDieuKhien, "XuLy", 128, nullptr, 2, nullptr);
  xTaskCreate(taskNhanLenhTuPC, "PC", 96, nullptr, 1, nullptr);
  xTaskCreate(taskDocCamBien, "DocCB", 96, nullptr, 2, nullptr);
  xTaskCreate(taskPrintStatus, "Print", 96, nullptr, 1, nullptr);
  xTaskCreate(taskAutoModeSwitch, "Auto", 96, nullptr, 1, nullptr);
}

void loop() {
}
