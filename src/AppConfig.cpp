#include "AppConfig.h"

namespace {
constexpr const char* NAMESPACE = "eq3gw";
}

void AppConfig::load() {
  Preferences prefs;
  prefs.begin(NAMESPACE, true);
  hostname = prefs.getString("hostname", "eq3-gateway");
  if (hostname.isEmpty()) hostname = "eq3-gateway";
  wifiSsid = prefs.getString("wifiSsid", "");
  wifiPassword = prefs.getString("wifiPass", "");
  staticIpEnabled = prefs.getBool("staticIp", false);
  localIp = prefs.getString("localIp", "");
  gatewayIp = prefs.getString("gatewayIp", "");
  subnetMask = prefs.getString("subnet", "255.255.255.0");
  dns1 = prefs.getString("dns1", "");
  dns2 = prefs.getString("dns2", "");
  mqttHost = prefs.getString("mqttHost", "");
  mqttPort = prefs.getUShort("mqttPort", 1883);
  mqttUser = prefs.getString("mqttUser", "");
  mqttPassword = prefs.getString("mqttPass", "");
  mqttTopic = prefs.getString("mqttTopic", "eq3/gateway");
  mqttUpdateIntervalSeconds = constrain(prefs.getUShort("mqttInterval", 5), 5, 3600);
  homeAssistantDiscovery = prefs.getBool("haDisc", false);
  for (size_t i = 0; i < MAX_THERMOSTATS; ++i) {
    String suffix(i);
    thermostats[i].address = prefs.getString(("tAddr" + suffix).c_str(), "");
    thermostats[i].name = prefs.getString(("tName" + suffix).c_str(), "Thermostat");
    thermostats[i].pin = prefs.getUInt(("tPin" + suffix).c_str(), 0);
  }

  // One-time migration from the former single-thermostat configuration.
  if (!thermostats[0].configured()) {
    thermostats[0].address = prefs.getString("thermoAddr", "");
    thermostats[0].name = prefs.getString("thermoName", "Thermostat");
    thermostats[0].pin = prefs.getUInt("thermoPin", 0);
  }
  webPassword = prefs.getString("webPass", "");
  prefs.end();
}

void AppConfig::save() const {
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  prefs.putString("hostname", hostname);
  prefs.putString("wifiSsid", wifiSsid);
  prefs.putString("wifiPass", wifiPassword);
  prefs.putBool("staticIp", staticIpEnabled);
  prefs.putString("localIp", localIp);
  prefs.putString("gatewayIp", gatewayIp);
  prefs.putString("subnet", subnetMask);
  prefs.putString("dns1", dns1);
  prefs.putString("dns2", dns2);
  prefs.putString("mqttHost", mqttHost);
  prefs.putUShort("mqttPort", mqttPort);
  prefs.putString("mqttUser", mqttUser);
  prefs.putString("mqttPass", mqttPassword);
  prefs.putString("mqttTopic", mqttTopic);
  prefs.putUShort("mqttInterval", mqttUpdateIntervalSeconds);
  prefs.putBool("haDisc", homeAssistantDiscovery);
  for (size_t i = 0; i < MAX_THERMOSTATS; ++i) {
    String suffix(i);
    prefs.putString(("tAddr" + suffix).c_str(), thermostats[i].address);
    prefs.putString(("tName" + suffix).c_str(), thermostats[i].name);
    prefs.putUInt(("tPin" + suffix).c_str(), thermostats[i].pin);
  }
  prefs.putString("webPass", webPassword);
  prefs.end();
}

size_t AppConfig::thermostatCount() const {
  size_t count = 0;
  for (const auto& thermostat : thermostats) {
    if (thermostat.configured()) ++count;
  }
  return count;
}


