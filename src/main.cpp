#include <Arduino.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <cctype>

#include "AppConfig.h"
#include "BuildInfo.h"
#include "Eq3Client.h"
#include "WebPage.h"

namespace {
AppConfig config;
Eq3Client thermostats[AppConfig::MAX_THERMOSTATS];
WebServer server(80);
DNSServer dns;
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
bool accessPointActive = false;
unsigned long lastMqttAttempt = 0;
unsigned long lastBleMaintenance = 0;
unsigned long lastThermostatPoll[AppConfig::MAX_THERMOSTATS] = {};
size_t nextThermostatPoll = 0;
unsigned long restartAt = 0;
String otaUploadError;

const char OTA_SUCCESS_PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>OTA-Update erfolgreich</title><style>body{margin:0;display:grid;min-height:100vh;place-items:center;background:#0b1118;color:#edf4fb;font:16px system-ui,sans-serif}.box{max-width:520px;margin:20px;padding:28px;border:1px solid #263545;border-radius:16px;background:#151e29;text-align:center}h1{font-size:25px}.spin{width:32px;height:32px;margin:24px auto;border:4px solid #263545;border-top-color:#49d7a0;border-radius:50%;animation:r 1s linear infinite}@keyframes r{to{transform:rotate(360deg)}}p{color:#91a5b8}</style></head><body><div class="box"><h1>OTA-Update erfolgreich</h1><div class="spin"></div><p id="status">Das Gateway startet neu. Sobald es wieder erreichbar ist, öffnet sich automatisch die Hauptseite.</p></div><script>let attempts=0;async function returnHome(){attempts++;document.getElementById('status').textContent='Gateway startet neu … Verbindungsversuch '+attempts;try{const response=await fetch('/api/status?ota='+Date.now(),{cache:'no-store'});if(response.ok){location.replace('/');return}}catch(e){}setTimeout(returnHome,1500)}setTimeout(returnHome,8000)</script></body></html>
)HTML";

String jsonEscape(const String& input) {
  String output;
  output.reserve(input.length() + 8);
  for (char c : input) {
    if (c == '\\' || c == '"') { output += '\\'; output += c; }
    else if (c == '\n') output += "\\n";
    else if (static_cast<uint8_t>(c) >= 0x20) output += c;
  }
  return output;
}

String deviceId(const String& address) {
  String id;
  id.reserve(address.length());
  for (char c : address) {
    if (isalnum(static_cast<unsigned char>(c))) id += static_cast<char>(tolower(static_cast<unsigned char>(c)));
  }
  return id;
}

bool validHostname(const String& hostname) {
  if (hostname.isEmpty() || hostname.length() > 32 || hostname[0] == '-' || hostname[hostname.length() - 1] == '-') return false;
  for (char c : hostname) {
    if (!isalnum(static_cast<unsigned char>(c)) && c != '-') return false;
  }
  return true;
}

bool validIpv4(const String& value, bool allowEmpty = false) {
  if (value.isEmpty()) return allowEmpty;
  IPAddress address;
  return address.fromString(value);
}

String deviceTopic(size_t index) {
  return config.mqttTopic + "/" + deviceId(config.thermostats[index].address);
}

String discoveryTopic(const String& component, const String& objectId) {
  return "homeassistant/" + component + "/" + objectId + "/config";
}

bool authorize() {
  if (config.webPassword.isEmpty() || server.authenticate("admin", config.webPassword.c_str())) return true;
  server.requestAuthentication();
  return false;
}

void sendJson(int status, const String& json) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(status, "application/json; charset=utf-8", json);
}

String thermostatJson(size_t index) {
  const ThermostatConfig& cfg = config.thermostats[index];
  const Eq3State& state = thermostats[index].state();
  bool connected = thermostats[index].connected();
  String connectionState = connected ? thermostats[index].connectionState() : "Nicht verbunden";
  String json = "{";
  json += "\"index\":" + String(index);
  json += ",\"name\":\"" + jsonEscape(cfg.name) + "\"";
  json += ",\"address\":\"" + jsonEscape(cfg.address) + "\"";
  json += ",\"connected\":" + String(connected ? "true" : "false");
  json += ",\"busy\":" + String(thermostats[index].busy() ? "true" : "false");
  json += ",\"connectionState\":\"" + jsonEscape(connectionState) + "\"";
  json += ",\"rssi\":" + String(thermostats[index].rssi());
  json += ",\"valid\":" + String(state.valid ? "true" : "false");
  json += ",\"temperature\":" + String(state.temperature, 1);
  json += ",\"valve\":" + String(state.valve);
  json += ",\"mode\":" + String(state.mode);
  json += ",\"haMode\":\"" + String((state.mode & 0x01) ? "heat" : "auto") + "\"";
  json += ",\"lowBattery\":" + String(state.lowBattery ? "true" : "false");
  json += ",\"stateAgeSeconds\":" + String(state.valid ? (millis() - state.updatedAt) / 1000UL : 0);
  json += ",\"error\":\"" + jsonEscape(state.error) + "\"";
  json += "}";
  return json;
}

String stateJson(bool includeSettings) {
  String json = "{";
  json += "\"wifi\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
  json += ",\"mqtt\":" + String(mqtt.connected() ? "true" : "false");
  json += ",\"ap\":" + String(accessPointActive ? "true" : "false");
  json += ",\"ip\":\"" + jsonEscape(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "\"";
  json += ",\"hostname\":\"" + jsonEscape(config.hostname) + "\"";
  json += ",\"firmwareVersion\":\"" + String(FIRMWARE_VERSION) + "\"";
  json += ",\"firmwareAuthor\":\"" + String(FIRMWARE_AUTHOR) + "\"";
  json += ",\"firmwareBoard\":\"" + String(FIRMWARE_BOARD) + "\"";
  json += ",\"configuredDevices\":" + String(config.thermostatCount());
  size_t connectedCount = 0;
  for (size_t i = 0; i < AppConfig::MAX_THERMOSTATS; ++i) {
    if (config.thermostats[i].configured() && thermostats[i].connected()) ++connectedCount;
  }
  json += ",\"connectedDevices\":" + String(connectedCount);
  if (includeSettings) {
    json += ",\"wifiSsid\":\"" + jsonEscape(config.wifiSsid) + "\"";
    json += ",\"staticIpEnabled\":" + String(config.staticIpEnabled ? "true" : "false");
    json += ",\"localIp\":\"" + jsonEscape(config.localIp) + "\"";
    json += ",\"gatewayIp\":\"" + jsonEscape(config.gatewayIp) + "\"";
    json += ",\"subnetMask\":\"" + jsonEscape(config.subnetMask) + "\"";
    json += ",\"dns1\":\"" + jsonEscape(config.dns1) + "\"";
    json += ",\"dns2\":\"" + jsonEscape(config.dns2) + "\"";
    json += ",\"mqttHost\":\"" + jsonEscape(config.mqttHost) + "\"";
    json += ",\"mqttPort\":" + String(config.mqttPort);
    json += ",\"mqttUser\":\"" + jsonEscape(config.mqttUser) + "\"";
    json += ",\"mqttTopic\":\"" + jsonEscape(config.mqttTopic) + "\"";
    json += ",\"mqttUpdateIntervalSeconds\":" + String(config.mqttUpdateIntervalSeconds);
    json += ",\"homeAssistantDiscovery\":" + String(config.homeAssistantDiscovery ? "true" : "false");
    json += ",\"webAuthenticationEnabled\":" + String(config.webPassword.isEmpty() ? "false" : "true");
  }
  json += ",\"devices\":[";
  bool first = true;
  for (size_t i = 0; i < AppConfig::MAX_THERMOSTATS; ++i) {
    if (!config.thermostats[i].configured()) continue;
    if (!first) json += ',';
    first = false;
    json += thermostatJson(i);
  }
  json += "]}";
  return json;
}

void publishDiscovery(size_t index, bool enabled) {
  if (!mqtt.connected() || !config.thermostats[index].configured()) return;
  const ThermostatConfig& cfg = config.thermostats[index];
  String id = deviceId(cfg.address);
  String objectId = "eq3_" + id;
  String climateTopic = discoveryTopic("climate", objectId);
  String batteryTopic = discoveryTopic("binary_sensor", objectId + "_battery");
  String valveTopic = discoveryTopic("sensor", objectId + "_valve");
  String rssiTopic = discoveryTopic("sensor", objectId + "_rssi");
  String connectionTopic = discoveryTopic("binary_sensor", objectId + "_connectivity");
  if (!enabled) {
    mqtt.publish(climateTopic.c_str(), "", true);
    mqtt.publish(batteryTopic.c_str(), "", true);
    mqtt.publish(valveTopic.c_str(), "", true);
    mqtt.publish(rssiTopic.c_str(), "", true);
    mqtt.publish(connectionTopic.c_str(), "", true);
    return;
  }

  String base = deviceTopic(index);
  String stateTopic = base + "/state";
  String device = "{\"identifiers\":[\"" + objectId + "\"],\"name\":\"" + jsonEscape(cfg.name) + "\",\"manufacturer\":\"eQ-3\",\"model\":\"CC-RT-BLE\",\"sw_version\":\"" + String(FIRMWARE_VERSION) + "\"}";
  String json = "{";
  json += "\"name\":\"" + jsonEscape(cfg.name) + "\"";
  json += ",\"unique_id\":\"" + objectId + "\"";
  json += ",\"availability_topic\":\"" + base + "/availability\"";
  json += ",\"payload_available\":\"online\",\"payload_not_available\":\"offline\"";
  json += ",\"temperature_command_topic\":\"" + base + "/temperature/set\"";
  json += ",\"temperature_state_topic\":\"" + stateTopic + "\"";
  json += ",\"temperature_state_template\":\"{{ value_json.temperature }}\"";
  json += ",\"mode_command_topic\":\"" + base + "/mode/set\"";
  json += ",\"mode_state_topic\":\"" + stateTopic + "\"";
  json += ",\"mode_state_template\":\"{{ value_json.haMode }}\"";
  json += ",\"modes\":[\"auto\",\"heat\"],\"min_temp\":4.5,\"max_temp\":30.0,\"temp_step\":0.5,\"precision\":0.5,\"temperature_unit\":\"C\"";
  json += ",\"device\":" + device;
  json += "}";
  mqtt.publish(climateTopic.c_str(), json.c_str(), true);

  json = "{";
  json += "\"name\":\"Batterie schwach\"";
  json += ",\"unique_id\":\"" + objectId + "_battery_low\"";
  json += ",\"state_topic\":\"" + stateTopic + "\"";
  json += ",\"value_template\":\"{{ 'ON' if value_json.lowBattery else 'OFF' }}\"";
  json += ",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"device_class\":\"battery\",\"entity_category\":\"diagnostic\"";
  json += ",\"availability_topic\":\"" + base + "/availability\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\"";
  json += ",\"device\":" + device + "}";
  mqtt.publish(batteryTopic.c_str(), json.c_str(), true);

  json = "{";
  json += "\"name\":\"Ventilöffnung\"";
  json += ",\"unique_id\":\"" + objectId + "_valve\"";
  json += ",\"state_topic\":\"" + stateTopic + "\",\"value_template\":\"{{ value_json.valve }}\"";
  json += ",\"unit_of_measurement\":\"%\",\"state_class\":\"measurement\",\"icon\":\"mdi:valve\"";
  json += ",\"availability_topic\":\"" + base + "/availability\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\"";
  json += ",\"device\":" + device + "}";
  mqtt.publish(valveTopic.c_str(), json.c_str(), true);

  json = "{";
  json += "\"name\":\"Bluetooth Signalstärke\"";
  json += ",\"unique_id\":\"" + objectId + "_rssi\"";
  json += ",\"state_topic\":\"" + stateTopic + "\",\"value_template\":\"{{ value_json.rssi }}\"";
  json += ",\"unit_of_measurement\":\"dBm\",\"device_class\":\"signal_strength\",\"state_class\":\"measurement\",\"entity_category\":\"diagnostic\"";
  json += ",\"availability_topic\":\"" + base + "/availability\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\"";
  json += ",\"device\":" + device + "}";
  mqtt.publish(rssiTopic.c_str(), json.c_str(), true);

  json = "{";
  json += "\"name\":\"Bluetooth Verbindung\"";
  json += ",\"unique_id\":\"" + objectId + "_connectivity\"";
  json += ",\"state_topic\":\"" + stateTopic + "\"";
  json += ",\"value_template\":\"{{ 'ON' if value_json.connected else 'OFF' }}\"";
  json += ",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"device_class\":\"connectivity\",\"entity_category\":\"diagnostic\"";
  json += ",\"availability_topic\":\"" + config.mqttTopic + "/availability\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\"";
  json += ",\"device\":" + device + "}";
  mqtt.publish(connectionTopic.c_str(), json.c_str(), true);
}

void publishState(size_t index) {
  if (!mqtt.connected() || !config.thermostats[index].configured()) return;
  String base = deviceTopic(index);
  String payload = thermostatJson(index);
  mqtt.publish((base + "/state").c_str(), payload.c_str(), true);
  mqtt.publish((base + "/availability").c_str(), thermostats[index].connected() ? "online" : "offline", true);
}

void publishAll() {
  for (size_t i = 0; i < AppConfig::MAX_THERMOSTATS; ++i) {
    if (!config.thermostats[i].configured()) continue;
    publishDiscovery(i, config.homeAssistantDiscovery);
    publishState(i);
  }
}

bool readThermostatIndex(size_t& index) {
  if (!server.hasArg("index")) {
    sendJson(400, "{\"message\":\"Geräteindex fehlt\"}");
    return false;
  }
  int parsed = server.arg("index").toInt();
  if (parsed < 0 || parsed >= static_cast<int>(AppConfig::MAX_THERMOSTATS) || !config.thermostats[parsed].configured()) {
    sendJson(404, "{\"message\":\"Thermostat nicht gefunden\"}");
    return false;
  }
  index = static_cast<size_t>(parsed);
  return true;
}

void sendOperationResult(size_t index, bool ok) {
  publishState(index);
  if (ok) {
    sendJson(200, "{\"ok\":true,\"index\":" + String(index) + "}");
  } else {
    sendJson(502, "{\"ok\":false,\"message\":\"" + jsonEscape(thermostats[index].state().error) + "\"}");
  }
}

void handleMqttMessage(char* topic, byte* payload, unsigned int length) {
  String target(topic);
  String value;
  for (unsigned int i = 0; i < length; ++i) value += static_cast<char>(payload[i]);
  value.trim();

  for (size_t i = 0; i < AppConfig::MAX_THERMOSTATS; ++i) {
    const ThermostatConfig& cfg = config.thermostats[i];
    if (!cfg.configured()) continue;
    String base = deviceTopic(i);
    bool handled = true;
    if (target == base + "/temperature/set") {
      thermostats[i].setTemperature(cfg.address, cfg.pin, value.toFloat());
    } else if (target == base + "/mode/set") {
      thermostats[i].setMode(cfg.address, cfg.pin, value.equalsIgnoreCase("manual") || value.equalsIgnoreCase("heat"));
    } else if (target == base + "/boost/set" || target == base + "/preset/set") {
      thermostats[i].setBoost(cfg.address, cfg.pin, value == "1" || value.equalsIgnoreCase("on") || value.equalsIgnoreCase("boost"));
    } else if (target == base + "/status/get") {
      thermostats[i].requestStatus(cfg.address, cfg.pin);
    } else {
      handled = false;
    }
    if (handled) {
      lastThermostatPoll[i] = millis();
      publishState(i);
      return;
    }
  }
}

void startAccessPoint() {
  WiFi.mode(WIFI_AP_STA);
  uint64_t chipId = ESP.getEfuseMac();
  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%06llX", chipId & 0xFFFFFFULL);
  String apName = config.hostname;
  if (apName.length() > 24) apName.remove(24);
  String ssid = apName + "-" + String(suffix);
  WiFi.softAP(ssid.c_str(), "eq3setup");
  dns.start(53, "*", WiFi.softAPIP());
  accessPointActive = true;
  Serial.printf("Einrichtungs-WLAN: %s / Passwort: eq3setup / http://192.168.4.1\n", ssid.c_str());
}

void connectWifi() {
  if (!config.hasWifi()) {
    startAccessPoint();
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(config.hostname.c_str());
  IPAddress zero(0, 0, 0, 0);
  if (config.staticIpEnabled) {
    IPAddress localIp;
    IPAddress gatewayIp;
    IPAddress subnetMask;
    IPAddress primaryDns;
    IPAddress secondaryDns;
    localIp.fromString(config.localIp);
    gatewayIp.fromString(config.gatewayIp);
    subnetMask.fromString(config.subnetMask);
    primaryDns.fromString(config.dns1.isEmpty() ? config.gatewayIp : config.dns1);
    if (!config.dns2.isEmpty()) secondaryDns.fromString(config.dns2);
    WiFi.config(localIp, gatewayIp, subnetMask, primaryDns, secondaryDns);
    Serial.printf("Feste IP: %s · Gateway: %s · DNS: %s\n", config.localIp.c_str(), config.gatewayIp.c_str(), primaryDns.toString().c_str());
  } else {
    WiFi.config(zero, zero, zero);
  }
  WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());
  Serial.printf("Verbinde mit WLAN %s", config.wifiSsid.c_str());
  unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 15000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Weboberfläche: http://%s oder http://%s.local\n", WiFi.localIP().toString().c_str(), config.hostname.c_str());
    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    if (MDNS.begin(config.hostname.c_str())) MDNS.addService("http", "tcp", 80);
  } else {
    startAccessPoint();
  }
}

void setupRoutes() {
  server.on("/", HTTP_GET, [] {
    if (!authorize()) return;
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", WEB_PAGE);
  });
  server.on("/api/status", HTTP_GET, [] { if (authorize()) sendJson(200, stateJson(true)); });

  server.on("/api/scan", HTTP_POST, [] {
    if (!authorize()) return;
    auto devices = thermostats[0].scan();
    String json = "{\"devices\":[";
    for (size_t i = 0; i < devices.size(); ++i) {
      if (i) json += ',';
      json += "{\"address\":\"" + jsonEscape(devices[i].address) + "\",\"name\":\"" + jsonEscape(devices[i].name) + "\",\"rssi\":" + String(devices[i].rssi) + "}";
    }
    json += "]}";
    sendJson(200, json);
  });

  server.on("/api/settings", HTTP_POST, [] {
    if (!authorize()) return;
    String newHostname = server.arg("hostname");
    newHostname.trim();
    newHostname.toLowerCase();
    if (!validHostname(newHostname)) {
      sendJson(400, "{\"message\":\"Der Netzwerkname muss 1 bis 32 Zeichen lang sein und darf nur Buchstaben, Zahlen und Bindestriche enthalten.\"}");
      return;
    }
    bool newStaticIpEnabled = server.hasArg("staticIpEnabled");
    String newLocalIp = server.hasArg("localIp") ? server.arg("localIp") : config.localIp;
    String newGatewayIp = server.hasArg("gatewayIp") ? server.arg("gatewayIp") : config.gatewayIp;
    String newSubnetMask = server.hasArg("subnetMask") ? server.arg("subnetMask") : config.subnetMask;
    String newDns1 = server.hasArg("dns1") ? server.arg("dns1") : config.dns1;
    String newDns2 = server.hasArg("dns2") ? server.arg("dns2") : config.dns2;
    newLocalIp.trim();
    newGatewayIp.trim();
    newSubnetMask.trim();
    newDns1.trim();
    newDns2.trim();
    if (newStaticIpEnabled &&
        (!validIpv4(newLocalIp) || newLocalIp == "0.0.0.0" ||
         !validIpv4(newGatewayIp) || newGatewayIp == "0.0.0.0" ||
         !validIpv4(newSubnetMask) || newSubnetMask == "0.0.0.0" ||
         !validIpv4(newDns1, true) || !validIpv4(newDns2, true))) {
      sendJson(400, "{\"message\":\"Bitte gültige IPv4-Adressen für feste IP, Subnetz, Gateway und DNS eintragen.\"}");
      return;
    }
    bool oldDiscovery = config.homeAssistantDiscovery;
    bool newDiscovery = server.hasArg("haDiscovery");
    bool newWebAuthenticationEnabled = server.hasArg("webAuthenticationEnabled");
    String newWebPassword = server.arg("webPassword");
    if (newWebAuthenticationEnabled && config.webPassword.isEmpty() && newWebPassword.isEmpty()) {
      sendJson(400, "{\"message\":\"Bitte ein Passwort für die Web-Anmeldung festlegen.\"}");
      return;
    }
    if (oldDiscovery && !newDiscovery && mqtt.connected()) {
      for (size_t i = 0; i < AppConfig::MAX_THERMOSTATS; ++i) publishDiscovery(i, false);
    }
    config.hostname = newHostname;
    config.wifiSsid = server.arg("wifiSsid");
    if (!server.arg("wifiPassword").isEmpty()) config.wifiPassword = server.arg("wifiPassword");
    config.staticIpEnabled = newStaticIpEnabled;
    config.localIp = newLocalIp;
    config.gatewayIp = newGatewayIp;
    config.subnetMask = newSubnetMask;
    config.dns1 = newDns1;
    config.dns2 = newDns2;
    config.mqttHost = server.arg("mqttHost");
    config.mqttPort = constrain(server.arg("mqttPort").toInt(), 1, 65535);
    config.mqttUser = server.arg("mqttUser");
    if (!server.arg("mqttPassword").isEmpty()) config.mqttPassword = server.arg("mqttPassword");
    config.mqttTopic = server.arg("mqttTopic");
    if (config.mqttTopic.isEmpty()) config.mqttTopic = "eq3/gateway";
    config.mqttUpdateIntervalSeconds = constrain(server.arg("mqttUpdateIntervalSeconds").toInt(), 5, 3600);
    config.homeAssistantDiscovery = newDiscovery;
    if (!newWebAuthenticationEnabled) config.webPassword = "";
    else if (!newWebPassword.isEmpty()) config.webPassword = newWebPassword;
    config.save();
    if (!oldDiscovery && newDiscovery && mqtt.connected()) publishAll();
    sendJson(200, "{\"ok\":true}");
    restartAt = millis() + 1200;
  });

  server.on("/api/thermostat/select", HTTP_POST, [] {
    if (!authorize()) return;
    String address = server.arg("address");
    address.toUpperCase();
    String pinText = server.arg("pin");
    pinText.trim();
    if (!pinText.isEmpty() && pinText.length() != 4 && pinText.length() != 6) {
      sendJson(400, "{\"message\":\"Die PIN muss 4 oder 6 Stellen haben\"}");
      return;
    }
    for (size_t i = 0; i < pinText.length(); ++i) {
      if (!isDigit(pinText[i])) {
        sendJson(400, "{\"message\":\"Die PIN darf nur Ziffern enthalten\"}");
        return;
      }
    }

    int selected = -1;
    for (size_t i = 0; i < AppConfig::MAX_THERMOSTATS; ++i) {
      if (config.thermostats[i].address.equalsIgnoreCase(address)) selected = static_cast<int>(i);
    }
    if (selected < 0) {
      for (size_t i = 0; i < AppConfig::MAX_THERMOSTATS; ++i) {
        if (!config.thermostats[i].configured()) { selected = static_cast<int>(i); break; }
      }
    }
    if (selected < 0) {
      sendJson(409, "{\"message\":\"Maximal drei Thermostate sind möglich\"}");
      return;
    }

    size_t index = static_cast<size_t>(selected);
    if (!config.thermostats[index].address.equalsIgnoreCase(address)) thermostats[index].disconnect();
    config.thermostats[index].address = address;
    config.thermostats[index].name = server.arg("name");
    if (config.thermostats[index].name.isEmpty()) config.thermostats[index].name = "Thermostat " + String(index + 1);
    config.thermostats[index].pin = static_cast<uint32_t>(pinText.toInt());
    config.save();
    bool ok = thermostats[index].requestStatus(address, config.thermostats[index].pin);
    lastThermostatPoll[index] = millis();
    if (config.homeAssistantDiscovery) publishDiscovery(index, true);
    sendOperationResult(index, ok);
  });

  server.on("/api/thermostat/remove", HTTP_POST, [] {
    if (!authorize()) return;
    size_t index;
    if (!readThermostatIndex(index)) return;
    if (config.homeAssistantDiscovery) publishDiscovery(index, false);
    thermostats[index].disconnect();
    config.thermostats[index] = ThermostatConfig();
    lastThermostatPoll[index] = 0;
    config.save();
    sendJson(200, "{\"ok\":true}");
  });

  server.on("/api/thermostat/update", HTTP_POST, [] {
    if (!authorize()) return;
    size_t index;
    if (!readThermostatIndex(index)) return;

    String name = server.arg("name");
    name.trim();
    if (name.isEmpty() || name.length() > 48) {
      sendJson(400, "{\"message\":\"Der Gerätename muss 1 bis 48 Zeichen lang sein.\"}");
      return;
    }

    String pinText = server.arg("pin");
    pinText.trim();
    if (server.hasArg("pin")) {
      if (pinText.length() != 4 && pinText.length() != 6) {
        sendJson(400, "{\"message\":\"Die neue PIN muss 4 oder 6 Stellen haben.\"}");
        return;
      }
      for (size_t i = 0; i < pinText.length(); ++i) {
        if (!isDigit(pinText[i])) {
          sendJson(400, "{\"message\":\"Die neue PIN darf nur Ziffern enthalten.\"}");
          return;
        }
      }
    }

    config.thermostats[index].name = name;
    if (server.hasArg("clearPin")) config.thermostats[index].pin = 0;
    else if (server.hasArg("pin")) config.thermostats[index].pin = static_cast<uint32_t>(pinText.toInt());
    config.save();
    if (config.homeAssistantDiscovery) publishDiscovery(index, true);
    publishState(index);
    sendJson(200, "{\"ok\":true,\"message\":\"Name und PIN-Einstellung gespeichert.\"}");
  });

  server.on("/api/thermostat/connect", HTTP_POST, [] {
    if (!authorize()) return;
    size_t index;
    if (!readThermostatIndex(index)) return;
    const ThermostatConfig& cfg = config.thermostats[index];
    bool ok = thermostats[index].requestStatus(cfg.address, cfg.pin);
    lastThermostatPoll[index] = millis();
    sendOperationResult(index, ok);
  });
  server.on("/api/thermostat/disconnect", HTTP_POST, [] {
    if (!authorize()) return;
    size_t index;
    if (!readThermostatIndex(index)) return;
    thermostats[index].disconnect();
    publishState(index);
    sendJson(200, "{\"ok\":true}");
  });
  server.on("/api/thermostat/status", HTTP_POST, [] {
    if (!authorize()) return;
    size_t index;
    if (!readThermostatIndex(index)) return;
    const ThermostatConfig& cfg = config.thermostats[index];
    bool ok = thermostats[index].requestStatus(cfg.address, cfg.pin);
    lastThermostatPoll[index] = millis();
    sendOperationResult(index, ok);
  });
  server.on("/api/thermostat/temperature", HTTP_POST, [] {
    if (!authorize()) return;
    size_t index;
    if (!readThermostatIndex(index)) return;
    const ThermostatConfig& cfg = config.thermostats[index];
    bool ok = thermostats[index].setTemperature(cfg.address, cfg.pin, server.arg("temperature").toFloat());
    lastThermostatPoll[index] = millis();
    sendOperationResult(index, ok);
  });
  server.on("/api/thermostat/mode", HTTP_POST, [] {
    if (!authorize()) return;
    size_t index;
    if (!readThermostatIndex(index)) return;
    const ThermostatConfig& cfg = config.thermostats[index];
    bool manual = server.arg("mode") == "manual" || server.arg("mode") == "heat";
    bool ok = thermostats[index].setMode(cfg.address, cfg.pin, manual);
    lastThermostatPoll[index] = millis();
    sendOperationResult(index, ok);
  });
  server.on("/api/thermostat/boost", HTTP_POST, [] {
    if (!authorize()) return;
    size_t index;
    if (!readThermostatIndex(index)) return;
    const ThermostatConfig& cfg = config.thermostats[index];
    bool ok = thermostats[index].setBoost(cfg.address, cfg.pin, server.arg("enabled") == "1");
    lastThermostatPoll[index] = millis();
    sendOperationResult(index, ok);
  });

  server.on("/api/reboot", HTTP_POST, [] {
    if (!authorize()) return;
    sendJson(200, "{\"ok\":true,\"message\":\"Neustart wird ausgeführt\"}");
    restartAt = millis() + 750;
  });
  server.on("/api/ota", HTTP_POST, [] {
    if (!authorize()) return;
    bool ok = otaUploadError.isEmpty() && !Update.hasError();
    if (ok) {
      server.sendHeader("Cache-Control", "no-store");
      server.send_P(200, "text/html; charset=utf-8", OTA_SUCCESS_PAGE);
      // Leave enough time for the browser to receive the complete response
      // before Wi-Fi disappears during the restart.
      restartAt = millis() + 5000;
    } else {
      String message = otaUploadError.isEmpty() ? "Update fehlgeschlagen." : otaUploadError;
      server.send(500, "text/plain; charset=utf-8", message);
    }
  }, [] {
    if (!config.webPassword.isEmpty() && !server.authenticate("admin", config.webPassword.c_str())) return;
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      otaUploadError = "";
      String filename = upload.filename;
      filename.toLowerCase();
      if (filename.indexOf("factory") >= 0) {
        otaUploadError = "Die Factory-Datei ist nur für den ersten USB-Flash. Bitte die OTA-Datei auswählen.";
      } else if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        otaUploadError = "OTA-Update konnte nicht gestartet werden.";
      }
    } else if (upload.status == UPLOAD_FILE_WRITE && otaUploadError.isEmpty()) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) otaUploadError = "Fehler beim Schreiben der OTA-Datei.";
    } else if (upload.status == UPLOAD_FILE_END && otaUploadError.isEmpty()) {
      if (!Update.end(true)) otaUploadError = "OTA-Datei ist ungültig oder unvollständig.";
    }
  });
  server.onNotFound([] {
    if (accessPointActive) {
      server.sendHeader("Location", "http://192.168.4.1/", true);
      server.send(302, "text/plain", "");
    } else sendJson(404, "{\"message\":\"Nicht gefunden\"}");
  });
  server.begin();
}

void maintainMqtt() {
  if (WiFi.status() != WL_CONNECTED || config.mqttHost.isEmpty()) return;
  if (mqtt.connected()) {
    mqtt.loop();
    return;
  }
  if (millis() - lastMqttAttempt < 5000) return;
  lastMqttAttempt = millis();
  mqtt.setServer(config.mqttHost.c_str(), config.mqttPort);
  mqtt.setCallback(handleMqttMessage);
  String clientId = config.hostname + "-" + String(static_cast<uint32_t>(ESP.getEfuseMac()), HEX);
  String willTopic = config.mqttTopic + "/availability";
  bool connected = config.mqttUser.isEmpty()
      ? mqtt.connect(clientId.c_str(), willTopic.c_str(), 0, true, "offline")
      : mqtt.connect(clientId.c_str(), config.mqttUser.c_str(), config.mqttPassword.c_str(), willTopic.c_str(), 0, true, "offline");
  if (!connected) return;
  mqtt.publish(willTopic.c_str(), "online", true);
  for (size_t i = 0; i < AppConfig::MAX_THERMOSTATS; ++i) {
    if (!config.thermostats[i].configured()) continue;
    String base = deviceTopic(i);
    mqtt.subscribe((base + "/temperature/set").c_str());
    mqtt.subscribe((base + "/mode/set").c_str());
    mqtt.subscribe((base + "/boost/set").c_str());
    mqtt.subscribe((base + "/preset/set").c_str());
    mqtt.subscribe((base + "/status/get").c_str());
  }
  publishAll();
}

void maintainThermostats() {
  if (millis() - lastBleMaintenance < 250) return;
  lastBleMaintenance = millis();
  for (size_t checked = 0; checked < AppConfig::MAX_THERMOSTATS; ++checked) {
    size_t index = nextThermostatPoll;
    nextThermostatPoll = (nextThermostatPoll + 1) % AppConfig::MAX_THERMOSTATS;
    const ThermostatConfig& cfg = config.thermostats[index];
    if (!cfg.configured() || thermostats[index].busy()) continue;
    unsigned long interval = thermostats[index].connected()
        ? static_cast<unsigned long>(config.mqttUpdateIntervalSeconds) * 1000UL
        : (thermostats[index].canReconnect() ? 10000UL : 45000UL);
    if (lastThermostatPoll[index] && millis() - lastThermostatPoll[index] < interval) continue;
    lastThermostatPoll[index] = millis();
    thermostats[index].requestStatus(cfg.address, cfg.pin);
    publishState(index);
    return;
  }
}
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\neQ-3 Gateway %s startet · erstellt von %s\n", FIRMWARE_VERSION, FIRMWARE_AUTHOR);
  config.load();
  thermostats[0].begin();
  mqtt.setBufferSize(2048);
  mqtt.setKeepAlive(30);
  connectWifi();
  setupRoutes();
}

void loop() {
  server.handleClient();
  if (accessPointActive) dns.processNextRequest();
  maintainMqtt();
  maintainThermostats();
  if (restartAt && millis() >= restartAt) ESP.restart();
  delay(2);
}


