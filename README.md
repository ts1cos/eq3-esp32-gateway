# eQ-3 ESP32 Gateway

Bluetooth-LE-zu-MQTT-Gateway für bis zu drei eQ-3/Eqiva `CC-RT-BLE` Heizkörperthermostate auf einem **ESP32 WROOM-32D**.

Die Firmware wurde von **therock** erstellt. Sie stellt eine lokale Weboberfläche bereit, hält die Thermostate per Bluetooth verbunden und bindet jedes Gerät über MQTT und Home Assistant Discovery ein.

Aktuelle Version: **1.4.0**

> Der ESP32-C6 wird aktuell nicht unterstützt oder weiterentwickelt. Die vorhandenen C6-Dateien stammen aus einem älteren Entwicklungsstand.

## Funktionen

- Einrichtungs-WLAN beim ersten Start
- Änderbarer Netzwerkname und Zugriff über `GERAETENAME.local`
- DHCP oder feste IPv4-Adresse mit Gateway und zwei DNS-Servern
- Bis zu drei dauerhaft gespeicherte eQ-3/Eqiva-Thermostate
- Automatische Bluetooth-Wiederverbindung
- Änderbare Thermostatnamen und Pairing-PINs
- Lokale Temperatur-, Modus- und Boost-Steuerung
- MQTT mit einstellbarem Aktualisierungsintervall, Standard 5 Sekunden
- Home Assistant Discovery für Klima, Batterie-Warnung, Ventilöffnung, RSSI und Verbindungsstatus
- Web-OTA mit automatischer Rückkehr zur Hauptseite
- Versionierte OTA- und Factory-Firmware
- Optionaler Passwortschutz der Weboberfläche

## Benötigte Hardware

- ESP32-Entwicklungsboard mit **ESP32 WROOM-32D** und 4 MB Flash
- USB-Datenkabel für den ersten Flash
- Ein oder mehrere eQ-3/Eqiva `CC-RT-BLE` Thermostate
- 2,4-GHz-WLAN
- Optional: MQTT-Broker und Home Assistant

## Schnellstart

### 1. Erster USB-Flash

Für ein neues oder vollständig zurückzusetzendes Board die aktuelle Factory-Datei verwenden:

[Factory-Firmware v1.4.0](firmware/esp32-wroom-32d/eq3-gateway-wroom-v1.4.0-factory.bin)

Die ausführliche Anleitung für Windows, Linux und macOS steht unter [Factory-Firmware flashen](docs/FACTORY_FLASH.md).

### 2. Gateway einrichten

1. Nach dem Flash mit dem WLAN `eq3-gateway-XXXXXX` verbinden.
2. Das Standardpasswort lautet `eq3setup`.
3. Falls sich das Portal nicht automatisch öffnet, `http://192.168.4.1` aufrufen.
4. Gerätenamen, WLAN und optional MQTT eintragen.
5. Speichern und den Neustart abwarten.
6. Danach `http://GERAETENAME.local` oder die angezeigte IP-Adresse öffnen.

### 3. Thermostat hinzufügen

1. Die Hersteller-App vollständig schließen. Ein Thermostat erlaubt normalerweise nur eine aktive BLE-Verbindung.
2. Bluetooth beziehungsweise den Pairing-Modus am Thermostat aktivieren.
3. In der Weboberfläche **Geräte scannen** auswählen.
4. Das gefundene Thermostat hinzufügen und gegebenenfalls die vier- oder sechsstellige Pairing-PIN eingeben.

Über **Name/PIN ändern** kann der Name später angepasst werden. Eine leere PIN-Eingabe behält die bisherige PIN; `-` löscht sie. Gespeicherte PINs werden niemals über die Weboberfläche oder Status-API ausgegeben.

## OTA-Update

Für Updates über die Weboberfläche ausschließlich eine Datei mit der Endung `-ota.bin` verwenden:

[OTA-Firmware v1.4.0](firmware/esp32-wroom-32d/eq3-gateway-wroom-v1.4.0-ota.bin)

Unter **System & Firmware** die Datei auswählen und **OTA-Update installieren** drücken. Nach erfolgreichem Upload startet der ESP neu; der Browser wartet auf das Gateway und lädt anschließend automatisch die Hauptseite.

Die Factory-Datei ist kein OTA-Update. Sie wird von der Weboberfläche abgelehnt und ist nur für einen vollständigen USB-Flash an Adresse `0x0` vorgesehen.

## Netzwerk

Der Netzwerkname darf maximal 32 Buchstaben, Zahlen oder Bindestriche enthalten. Er wird als WLAN-Hostname, mDNS-Name und Teil der MQTT-Client-ID verwendet.

Standardmäßig verwendet das Gateway DHCP. Alternativ lassen sich feste IPv4-Adresse, Subnetzmaske, Gateway sowie primärer und optionaler zweiter DNS-Server eintragen. Ohne primären DNS-Eintrag wird die Gateway-Adresse als DNS-Server verwendet. Bei einer nicht erreichbaren WLAN-Konfiguration startet das Einrichtungs-WLAN erneut.

## MQTT

Jedes Thermostat erhält anhand seiner Bluetooth-Adresse ein eigenes Untertopic. Beispiel für `00:1A:22:AA:BB:CC` und das Basis-Topic `eq3/gateway`:

| Topic | Richtung | Inhalt |
|---|---|---|
| `eq3/gateway/001a22aabbcc/state` | Gateway → Broker | JSON-Status, retained |
| `eq3/gateway/001a22aabbcc/availability` | Gateway → Broker | `online` oder `offline`, retained |
| `eq3/gateway/001a22aabbcc/temperature/set` | Broker → Gateway | Temperatur von `4.5` bis `30.0` in 0,5-°C-Schritten |
| `eq3/gateway/001a22aabbcc/mode/set` | Broker → Gateway | `auto`, `manual` oder `heat` |
| `eq3/gateway/001a22aabbcc/boost/set` | Broker → Gateway | `on`/`off` oder `1`/`0` |
| `eq3/gateway/001a22aabbcc/status/get` | Broker → Gateway | Beliebiger Inhalt löst eine Statusabfrage aus |

Das Aktualisierungsintervall ist von 5 bis 3600 Sekunden einstellbar. Verbundene Thermostate werden nacheinander abgefragt; für getrennte Geräte gelten längere Wiederverbindungsintervalle.

## Home Assistant Discovery

Bei aktivierter Discovery erstellt das Gateway pro Thermostat:

- einen Klimaregler für Solltemperatur und Betriebsart
- eine Batterie-Warnung (`okay` oder `schwach`; kein Prozentwert verfügbar)
- einen Sensor für die Ventilöffnung in Prozent
- einen Sensor für die Bluetooth-Signalstärke in dBm
- einen binären Sensor für den Bluetooth-Verbindungsstatus

Alle Entitäten werden demselben Thermostat-Gerät zugeordnet. Beim Abschalten der Discovery oder Entfernen eines Thermostats werden die betreffenden retained Discovery-Einträge gelöscht.

## Selbst bauen

Voraussetzung ist [PlatformIO](https://platformio.org/). Im Projektverzeichnis:

```text
pio run -e esp32-wroom-32d
pio run -e esp32-wroom-32d -t upload
pio device monitor
```

Jeder WROOM-Build erzeugt automatisch zwei unveränderliche, versionierte Dateien unter `firmware/esp32-wroom-32d/`:

- `eq3-gateway-wroom-vVERSION-ota.bin`
- `eq3-gateway-wroom-vVERSION-factory.bin`

Existiert eine Versionsdatei bereits mit anderem Inhalt, bricht der Archivschritt ab. Dadurch werden ältere Releases nicht versehentlich überschrieben.

## Projektstruktur

```text
include/                 Konfiguration, Weboberfläche und Build-Informationen
src/                     Firmware und Bluetooth-Protokoll
scripts/                 Erzeugung der versionierten Firmwaredateien
docs/                    Anleitungen
firmware/                Vorgebaute OTA- und Factory-Dateien
platformio.ini           PlatformIO-Projektkonfiguration
```

## Hinweise

- MQTT ist derzeit unverschlüsselt und sollte nur in einem vertrauenswürdigen LAN verwendet werden.
- WLAN und Bluetooth teilen sich den Funkchip. Sehr kurze Abfrageintervalle und BLE-Scans können die Weboberfläche vorübergehend verlangsamen.
- Ohne Zugriff auf jede Thermostat-Hardware- und Firmwarevariante kann die Pairing-Abfolge nicht für alle Revisionen garantiert werden.

## Verwendetes eQ-3-Protokoll

- Service: `3e135142-654f-9090-134a-a6ff5bb77046`
- Command Characteristic: `3fa4585a-ce4a-3bad-db4b-b8df8179ea09`
- Notification Characteristic: `d0e8434d-cd29-0996-af41-6c90f4e0eb2a`

Grundlage für die Protokollimplementierung: [Heckie75/Eqiva-Smart-Radiator-Thermostat](https://github.com/Heckie75/Eqiva-Smart-Radiator-Thermostat).

## Weitere Dokumentation

- [Factory-Firmware flashen](docs/FACTORY_FLASH.md)
- [Firmwarearchiv](firmware/README.md)
- [Versionshistorie](CHANGELOG.md)
- [Offizielle esptool-Installation](https://docs.espressif.com/projects/esptool/en/latest/esp32/installation.html)
- [Offizielle esptool-Flash-Anleitung](https://docs.espressif.com/projects/esptool/en/latest/esp32/esptool/flashing-firmware.html)
