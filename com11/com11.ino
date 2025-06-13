#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <DHT.h>

#define DHTPIN 12
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

const uint8_t firstPin = 2;

SemaphoreHandle_t xSerialMutex;

TaskHandle_t xHandle_TTP = NULL;
TaskHandle_t xHandle_CMD = NULL;
TaskHandle_t xHandle_DHT = NULL;

void Task_TTP226(void *pvParameters);
void Task_CommandListener(void *pvParameters);
void Task_DHT11_Manager(void *pvParameters);

void setup() {
  Serial.begin(9600);
  dht.begin();

  for (uint8_t i = 0; i < 8; i++) {
    pinMode(firstPin + i, INPUT_PULLUP);
  }

  xSerialMutex = xSemaphoreCreateMutex();
  if (xSerialMutex == NULL) {
    Serial.println("Semaphore creation failed!");
    while (1);
  }

  xTaskCreate(Task_TTP226, "TTP", 128, NULL, 1, &xHandle_TTP);
  xTaskCreate(Task_CommandListener, "CMD", 128, NULL, 2, &xHandle_CMD);
  xTaskCreate(Task_DHT11_Manager, "DHT", 256, NULL, 3, &xHandle_DHT);
}

void loop() {
}

// ---------------- DHT11 Task --------------------
void Task_DHT11_Manager(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    if (xHandle_TTP != NULL) vTaskSuspend(xHandle_TTP);
    if (xHandle_CMD != NULL) vTaskSuspend(xHandle_CMD);

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t)) {
      if (xSemaphoreTake(xSerialMutex, pdMS_TO_TICKS(100))) {
        Serial.print("TEMP:");
        Serial.print((int)t);
        Serial.print("C  | HUM:");
        Serial.print((int)h);
        Serial.println("%");
        xSemaphoreGive(xSerialMutex);
      }
    } else {
      if (xSemaphoreTake(xSerialMutex, pdMS_TO_TICKS(100))) {
        Serial.println("DHT:Error");
        xSemaphoreGive(xSerialMutex);
      }
    }

    if (xHandle_TTP != NULL) vTaskResume(xHandle_TTP);
    if (xHandle_CMD != NULL) vTaskResume(xHandle_CMD);

    vTaskSuspend(NULL);
  }
}

// ---------------- TTP226 Task --------------------
void Task_TTP226(void *pvParameters) {
  (void) pvParameters;
  uint8_t prevState = 0;
  static TickType_t lastNotifyTime = 0;

  for (;;) {
    uint8_t currentState = 0;

    for (uint8_t i = 0; i < 8; i++) {
      bool pressed = (digitalRead(firstPin + i) == LOW);
      bitWrite(currentState, i, pressed);

      if (pressed && !bitRead(prevState, i)) {
        if (xSemaphoreTake(xSerialMutex, pdMS_TO_TICKS(50))) {
          Serial.print("TTP:");
          Serial.println(i + 1);
          xSemaphoreGive(xSerialMutex);
        }
      }
    }
    prevState = currentState;

    if ((xTaskGetTickCount() - lastNotifyTime) > pdMS_TO_TICKS(15000)) {
      if (xHandle_DHT != NULL) {
        vTaskResume(xHandle_DHT);
        xTaskNotifyGive(xHandle_DHT);
        lastNotifyTime = xTaskGetTickCount();
      }
    }

    vTaskDelay(pdMS_TO_TICKS(40));
  }
}

// ---------------- Serial Command Task --------------------
void Task_CommandListener(void *pvParameters) {
  (void) pvParameters;
  char buffer[32];
  uint8_t idx = 0;

  for (;;) {
    while (Serial.available()) {
      char ch = Serial.read();

      if (ch == '\n' || ch == '\r') {
        buffer[idx] = '\0';
        idx = 0;
        memset(buffer, 0, sizeof(buffer));
      } else if (idx < sizeof(buffer) - 1) {
        buffer[idx++] = ch;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ---------------- Stack Overflow Hook --------------------
extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
  Serial.print("Stack overflow detected in task: ");
  Serial.println(pcTaskName);
  while (1);
}
