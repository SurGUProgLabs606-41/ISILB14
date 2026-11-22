
#include <M5Unified.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// UUID сервиса
#define SERVICE_UUID "f1d985e5-cf8f-4663-8fa8-61443bf7cbce"

// UUID характеристики акселерометра
#define ACCEL_UUID "a31e7b42-9c56-4d80-b213-6f8a5e1c7349"

// UUID характеристики для записи
#define WRITE_UUID "c72f4a91-3e68-42b5-8d10-9a6c2f7e4153"

// BLE-сервер
BLEServer* pServer = NULL;

// Характеристики
BLECharacteristic* pAccelCharacteristic = NULL;
BLECharacteristic* pWriteCharacteristic = NULL;

// Состояние подключения
bool deviceConnected = false;
bool oldDeviceConnected = false;

// Полученное сообщение
String receivedValue = "No data";
volatile bool newValueReceived = false;

// Время последней отправки данных
unsigned long lastSendTime = 0;

// Обработчик подключения BLE
class ServerCallbacks : public BLEServerCallbacks {

  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
  }

  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
  }
};

// Обработчик записи характеристики
class WriteCallbacks : public BLECharacteristicCallbacks {

  void onWrite(BLECharacteristic* pCharacteristic) override {

    receivedValue = pCharacteristic->getValue();

    newValueReceived = true;
  }
};

// Настройка BLE
void setupBLE() {

  // Инициализация Bluetooth
  BLEDevice::init("M5StickS3");

  // Создание сервера
  pServer = BLEDevice::createServer();

  pServer->setCallbacks(new ServerCallbacks());

  // Создание сервиса
  BLEService* pService = pServer->createService(SERVICE_UUID);

  // Характеристика акселерометра
  pAccelCharacteristic = pService->createCharacteristic(
    ACCEL_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  // Дескриптор уведомлений
  pAccelCharacteristic->addDescriptor(new BLE2902());

  pAccelCharacteristic->setValue("0.00,0.00,0.00");

  // Характеристика для чтения и записи
  pWriteCharacteristic = pService->createCharacteristic(
    WRITE_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE
  );

  pWriteCharacteristic->setValue("Send data");

  pWriteCharacteristic->setCallbacks(new WriteCallbacks());

  // Запуск сервиса
  pService->start();

  // Запуск рекламы BLE
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();

  pAdvertising->addServiceUUID(SERVICE_UUID);

  BLEDevice::startAdvertising();
}

// Отображение данных на экране
void showScreen(float ax, float ay, float az) {

  M5.Display.fillScreen(TFT_BLACK);

  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextSize(1);

  M5.Display.setCursor(5, 5);
  M5.Display.println("BLE DEVICE");

  M5.Display.setCursor(5, 25);

  if (deviceConnected) {
    M5.Display.println("Connected");
  } else {
    M5.Display.println("Waiting...");
  }

  M5.Display.setCursor(5, 50);
  M5.Display.println("Accelerometer:");

  M5.Display.setCursor(5, 65);
  M5.Display.printf("X: %.2f", ax);

  M5.Display.setCursor(5, 80);
  M5.Display.printf("Y: %.2f", ay);

  M5.Display.setCursor(5, 95);
  M5.Display.printf("Z: %.2f", az);

  M5.Display.setCursor(5, 115);
  M5.Display.println("Received:");

  M5.Display.setCursor(5, 128);
  M5.Display.println(receivedValue);
}

void setup() {

  // Инициализация M5StickS3
  M5.begin();

  M5.Display.setRotation(1);

  M5.Display.fillScreen(TFT_BLACK);

  // Инициализация BLE
  setupBLE();

  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextSize(2);

  M5.Display.setCursor(15, 45);
  M5.Display.println("BLE Ready");
}

void loop() {

  M5.update();

  // Проверяем состояние акселерометра
  M5.Imu.update();

  auto imuData = M5.Imu.getImuData();

  // Получаем значения по трем осям
  float ax = imuData.accel.x;
  float ay = imuData.accel.y;
  float az = imuData.accel.z;

  unsigned long currentTime = millis();

  // Отправка данных один раз в секунду
  if (currentTime - lastSendTime >= 1000) {

    lastSendTime = currentTime;

    // Формируем строку со значениями акселерометра
    String accelValue =
      String(ax, 3) + "," +
      String(ay, 3) + "," +
      String(az, 3);

    // Обновляем значение характеристики
    pAccelCharacteristic->setValue(accelValue);

    // Отправляем уведомление клиенту
    if (deviceConnected) {
      pAccelCharacteristic->notify();
    }

    // Обновляем экран
    showScreen(ax, ay, az);
  }

  // Проверяем получение новых данных
  if (newValueReceived) {

    newValueReceived = false;

    // Отображаем полученное сообщение
    showScreen(ax, ay, az);

    Serial.print("Received: ");
    Serial.println(receivedValue);
  }

  // Повторный запуск рекламы после отключения
  if (!deviceConnected && oldDeviceConnected) {

    delay(500);

    pServer->startAdvertising();

    oldDeviceConnected = false;
  }

  // Запоминаем подключение
  if (deviceConnected && !oldDeviceConnected) {

    oldDeviceConnected = true;
  }

  delay(10);
}
