#pragma once

#include <Arduino.h>
#include <Preferences.h>

struct ThermostatConfig {
  String address;
  String name = "Thermostat";
  uint32_t pin = 0;

  bool configured() const { return !address.isEmpty(); }
};

struct AppConfig {
  static constexpr size_t MAX_THERMOSTATS = 3;

  String hostname = "eq3-gateway";
  String wifiSsid;
  String wifiPassword;
  bool staticIpEnabled = false;
  String localIp;
  String gatewayIp;
  String subnetMask = "255.255.255.0";
  String dns1;
  String dns2;
  String mqttHost;
  uint16_t mqttPort = 1883;
  String mqttUser;
  String mqttPassword;
  String mqttTopic = "eq3/gateway";
  uint16_t mqttUpdateIntervalSeconds = 5;
  bool homeAssistantDiscovery = false;
  ThermostatConfig thermostats[MAX_THERMOSTATS];
  String webPassword;

  void load();
  void save() const;
  bool hasWifi() const { return !wifiSsid.isEmpty(); }
  size_t thermostatCount() const;
};


