#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <vector>

struct Eq3Device {
  String address;
  String name;
  int rssi = 0;
};

struct Eq3State {
  bool valid = false;
  float temperature = 0.0f;
  uint8_t valve = 0;
  uint8_t mode = 0;
  bool lowBattery = false;
  unsigned long updatedAt = 0;
  String error;
};

class Eq3Client {
 public:
  void begin();
  std::vector<Eq3Device> scan(uint32_t durationMs = 6000);
  bool requestStatus(const String& address, uint32_t pin);
  bool setTemperature(const String& address, uint32_t pin, float temperature);
  bool setMode(const String& address, uint32_t pin, bool manual);
  bool setBoost(const String& address, uint32_t pin, bool enabled);
  void disconnect();
  const Eq3State& state() const { return state_; }
  bool busy() const { return busy_; }
  bool connected() const;
  bool canReconnect() const { return client_ && !connectedAddress_.isEmpty(); }
  int rssi() const;
  const String& connectionState() const { return connectionState_; }

 private:
  bool ensureConnected(const String& address, uint32_t pin);
  bool transact(const String& address, uint32_t pin, const uint8_t* command, size_t length);
  bool findDeviceAddress(const String& address, NimBLEAddress& result);
  void resetClient();
  void onNotification(const uint8_t* data, size_t length);
  static void notifyCallback(NimBLERemoteCharacteristic*, uint8_t*, size_t, bool);

  Eq3State state_;
  NimBLEClient* client_ = nullptr;
  NimBLERemoteCharacteristic* commandCharacteristic_ = nullptr;
  NimBLERemoteCharacteristic* notifyCharacteristic_ = nullptr;
  String connectedAddress_;
  String connectionState_ = "Nicht verbunden";
  bool initialized_ = false;
  bool busy_ = false;
  volatile bool notificationReceived_ = false;
  static Eq3Client* activeInstance_;
};


