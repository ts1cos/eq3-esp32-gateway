#include "Eq3Client.h"

#include <ctime>

namespace {
constexpr const char* EQ3_SERVICE_UUID = "3e135142-654f-9090-134a-a6ff5bb77046";
constexpr const char* EQ3_COMMAND_UUID = "3fa4585a-ce4a-3bad-db4b-b8df8179ea09";
constexpr const char* EQ3_NOTIFY_UUID = "d0e8434d-cd29-0996-af41-6c90f4e0eb2a";
uint32_t pairingPin = 0;
bool nimbleInitialized = false;

class Eq3SecurityCallbacks : public NimBLEClientCallbacks {
  void onPassKeyEntry(NimBLEConnInfo& info) override {
    NimBLEDevice::injectPassKey(info, pairingPin);
  }

  void onConfirmPasskey(NimBLEConnInfo& info, uint32_t) override {
    NimBLEDevice::injectConfirmPasskey(info, true);
  }
};

Eq3SecurityCallbacks securityCallbacks;
}

Eq3Client* Eq3Client::activeInstance_ = nullptr;

void Eq3Client::begin() {
  if (initialized_) return;
  if (!nimbleInitialized) {
    NimBLEDevice::init("EQ3-Gateway");
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY);
    // The thermostats use legacy passkey pairing; Secure Connections must stay optional.
    NimBLEDevice::setSecurityAuth(true, true, false);
    nimbleInitialized = true;
  }
  initialized_ = true;
}

bool Eq3Client::connected() const {
  return client_ && client_->isConnected();
}

int Eq3Client::rssi() const {
  return connected() ? client_->getRssi() : 0;
}

std::vector<Eq3Device> Eq3Client::scan(uint32_t durationMs) {
  begin();
  std::vector<Eq3Device> found;
  if (busy_) return found;
  busy_ = true;

  NimBLEScan* scanner = NimBLEDevice::getScan();
  scanner->setActiveScan(true);
  // WLAN and BLE share one radio. A 100% scan window can interrupt the HTTP request.
  scanner->setInterval(120);
  scanner->setWindow(45);
  scanner->clearResults();
  NimBLEScanResults results = scanner->getResults(durationMs, false);

  for (int i = 0; i < results.getCount(); ++i) {
    const NimBLEAdvertisedDevice* device = results.getDevice(i);
    String address(device->getAddress().toString().c_str());
    String name = device->haveName() ? String(device->getName().c_str()) : String("Unbekannt");
    address.toUpperCase();
    bool looksLikeEq3 = name.indexOf("CC-RT-BLE") >= 0 || address.startsWith("00:1A:22:");
    if (looksLikeEq3) {
      Eq3Device result;
      result.address = address;
      result.name = name;
      result.rssi = device->getRSSI();
      found.push_back(result);
    }
  }

  scanner->clearResults();
  busy_ = false;
  return found;
}

bool Eq3Client::findDeviceAddress(const String& wantedAddress, NimBLEAddress& result) {
  NimBLEScan* scanner = NimBLEDevice::getScan();
  scanner->setActiveScan(false);
  scanner->setInterval(120);
  scanner->setWindow(45);
  scanner->clearResults();
  NimBLEScanResults results = scanner->getResults(3500, false);
  for (int i = 0; i < results.getCount(); ++i) {
    const NimBLEAdvertisedDevice* device = results.getDevice(i);
    String address(device->getAddress().toString().c_str());
    if (address.equalsIgnoreCase(wantedAddress)) {
      result = device->getAddress();
      scanner->clearResults();
      return true;
    }
  }
  scanner->clearResults();
  return false;
}

void Eq3Client::resetClient() {
  commandCharacteristic_ = nullptr;
  notifyCharacteristic_ = nullptr;
  connectedAddress_ = "";
  if (!client_) return;
  if (client_->isConnected()) {
    client_->disconnect();
    unsigned long started = millis();
    while (client_->isConnected() && millis() - started < 1000) delay(10);
  }
  NimBLEDevice::deleteClient(client_);
  client_ = nullptr;
}

void Eq3Client::disconnect() {
  resetClient();
  connectionState_ = "Getrennt";
}

bool Eq3Client::ensureConnected(const String& address, uint32_t pin) {
  if (connected() && connectedAddress_.equalsIgnoreCase(address) && commandCharacteristic_ && notifyCharacteristic_) {
    connectionState_ = "Verbunden";
    return true;
  }

  pairingPin = pin;
  activeInstance_ = this;

  bool connectionOk = false;
  if (client_ && connectedAddress_.equalsIgnoreCase(address)) {
    // Reuse the stored address type and GATT cache after a remote disconnect.
    connectionState_ = "Verbinde erneut";
    connectionOk = client_->connect(false, false, false);
  } else {
    if (client_) resetClient();
    connectionState_ = "Suche Thermostat";
    NimBLEAddress deviceAddress;
    if (!findDeviceAddress(address, deviceAddress)) {
      state_.error = "Thermostat nicht gefunden. Bluetooth am Thermostat aktivieren und Hersteller-App schließen.";
      connectionState_ = "Nicht gefunden";
      return false;
    }

    client_ = NimBLEDevice::createClient(deviceAddress);
    if (!client_) {
      state_.error = "Kein BLE-Client verfügbar (maximal drei Verbindungen)";
      connectionState_ = "BLE-Limit erreicht";
      return false;
    }
    client_->setClientCallbacks(&securityCallbacks, false);
    client_->setConnectionParams(12, 24, 0, 200);
    client_->setConnectTimeout(7000);
    connectionState_ = "Verbinde";
    // The thermostat only needs the default 23-byte MTU. Skipping MTU exchange
    // improves compatibility with older CC-RT-BLE firmware.
    connectionOk = client_->connect(deviceAddress, true, false, false);
    if (connectionOk) connectedAddress_ = address;
  }

  if (!connectionOk) {
    state_.error = "BLE-Verbindung fehlgeschlagen (Fehler " + String(client_->getLastError()) + "). Pairing-Modus und PIN prüfen.";
    connectionState_ = "Verbindung fehlgeschlagen";
    return false;
  }

  NimBLERemoteService* service = client_->getService(EQ3_SERVICE_UUID);
  commandCharacteristic_ = service ? service->getCharacteristic(EQ3_COMMAND_UUID) : nullptr;
  notifyCharacteristic_ = service ? service->getCharacteristic(EQ3_NOTIFY_UUID) : nullptr;
  if (!service || !commandCharacteristic_ || !notifyCharacteristic_) {
    state_.error = "eQ-3 GATT-Dienst nicht gefunden. Ist das Gerät wirklich ein CC-RT-BLE?";
    connectionState_ = "Falscher GATT-Dienst";
    resetClient();
    return false;
  }
  if (!notifyCharacteristic_->subscribe(true, notifyCallback)) {
    state_.error = "Benachrichtigungen konnten nicht aktiviert werden. Bei 6-stelliger PIN erneut Pairing starten.";
    connectionState_ = "Kopplung fehlgeschlagen";
    resetClient();
    return false;
  }

  connectedAddress_ = address;
  connectionState_ = "Verbunden";
  state_.error = "";
  return true;
}

bool Eq3Client::transact(const String& address, uint32_t pin, const uint8_t* command, size_t length) {
  begin();
  if (busy_) {
    state_.error = "Bluetooth ist beschäftigt";
    return false;
  }
  busy_ = true;
  notificationReceived_ = false;
  state_.error = "";
  pairingPin = pin;
  activeInstance_ = this;

  bool ok = ensureConnected(address, pin);
  if (ok && !commandCharacteristic_->writeValue(command, length, true)) {
    state_.error = "Befehl konnte nicht gesendet werden (Fehler " + String(client_->getLastError()) + ")";
    connectionState_ = "Schreibfehler";
    ok = false;
  } else if (ok) {
    unsigned long started = millis();
    while (!notificationReceived_ && millis() - started < 3500) delay(20);
    if (!notificationReceived_) {
      state_.error = "Verbunden, aber keine Antwort vom Thermostat";
      connectionState_ = "Verbunden, keine Antwort";
      ok = false;
    } else {
      connectionState_ = "Verbunden";
    }
  }

  activeInstance_ = nullptr;
  busy_ = false;
  return ok;
}

bool Eq3Client::requestStatus(const String& address, uint32_t pin) {
  uint8_t command[7] = {0x03};
  size_t length = 1;
  struct tm now {};
  if (getLocalTime(&now, 100)) {
    command[1] = static_cast<uint8_t>((now.tm_year + 1900) - 2000);
    command[2] = static_cast<uint8_t>(now.tm_mon + 1);
    command[3] = static_cast<uint8_t>(now.tm_mday);
    command[4] = static_cast<uint8_t>(now.tm_hour);
    command[5] = static_cast<uint8_t>(now.tm_min);
    command[6] = static_cast<uint8_t>(now.tm_sec);
    length = sizeof(command);
  }
  return transact(address, pin, command, length);
}

bool Eq3Client::setTemperature(const String& address, uint32_t pin, float temperature) {
  temperature = constrain(roundf(temperature * 2.0f) / 2.0f, 4.5f, 30.0f);
  uint8_t command[] = {0x41, static_cast<uint8_t>(temperature * 2.0f)};
  return transact(address, pin, command, sizeof(command));
}

bool Eq3Client::setMode(const String& address, uint32_t pin, bool manual) {
  uint8_t command[] = {0x40, static_cast<uint8_t>(manual ? 0x40 : 0x00)};
  return transact(address, pin, command, sizeof(command));
}

bool Eq3Client::setBoost(const String& address, uint32_t pin, bool enabled) {
  uint8_t command[] = {0x45, static_cast<uint8_t>(enabled ? 0xff : 0x00)};
  return transact(address, pin, command, sizeof(command));
}

void Eq3Client::notifyCallback(NimBLERemoteCharacteristic*, uint8_t* data, size_t length, bool) {
  if (activeInstance_) activeInstance_->onNotification(data, length);
}

void Eq3Client::onNotification(const uint8_t* data, size_t length) {
  if (length >= 6 && data[0] == 0x02 && data[1] == 0x01) {
    state_.mode = data[2];
    state_.valve = data[3];
    state_.temperature = data[5] / 2.0f;
    state_.lowBattery = (data[2] & 0x80) != 0;
    state_.valid = true;
    state_.updatedAt = millis();
    state_.error = "";
  }
  notificationReceived_ = true;
}


