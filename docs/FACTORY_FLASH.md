# Factory-Firmware auf einen ESP32 WROOM-32D flashen

Diese Anleitung beschreibt den ersten vollständigen USB-Flash mit `esptool`. Die Factory-Datei enthält Bootloader, Partitionstabelle, OTA-Bootdaten und Anwendungsfirmware in einem gemeinsamen Image.

> **Achtung:** Nur für den klassischen ESP32 WROOM-32D verwenden. Nicht auf einen ESP32-C6 flashen. Ein vollständiges Löschen entfernt WLAN-, MQTT-, Thermostat- und alle anderen gespeicherten Einstellungen.

## Was wird benötigt?

- ESP32-WROOM-32D-Entwicklungsboard
- USB-Datenkabel; reine Ladekabel funktionieren nicht
- Computer mit Windows, Linux oder macOS
- Python und `esptool`
- Aktuelle Datei `eq3-gateway-wroom-vVERSION-factory.bin`

Die aktuelle esptool-Version benötigt Python 3.10 oder neuer. Mit älteren Python-Versionen kann weiterhin esptool 4.x verwendet werden. Siehe [offizielle Installationsanleitung](https://docs.espressif.com/projects/esptool/en/latest/esp32/installation.html).

## 1. esptool installieren

### Windows

PowerShell oder Eingabeaufforderung öffnen:

```powershell
py -m pip install --upgrade esptool
py -m esptool --help
```

Falls `py` nicht gefunden wird, Python von [python.org](https://www.python.org/downloads/) installieren und dabei **Add Python to PATH** aktivieren.

### Linux oder macOS

```bash
python3 -m pip install --upgrade esptool
python3 -m esptool --help
```

## 2. Seriellen Anschluss ermitteln

Board anstecken und den Anschluss bestimmen:

- Windows: Geräte-Manager → **Anschlüsse (COM & LPT)**, beispielsweise `COM5`
- Linux: normalerweise `/dev/ttyUSB0` oder `/dev/ttyACM0`
- macOS: normalerweise `/dev/cu.usbserial-*` oder `/dev/cu.SLAB_USBtoUART`

Abhängig vom Board kann ein Treiber für CP210x oder CH340 notwendig sein. Unter Linux muss der Benutzer gegebenenfalls Mitglied der Gruppe `dialout` sein:

```bash
sudo usermod -a -G dialout "$USER"
```

Danach ab- und wieder anmelden. Weitere Hinweise stehen in der [offiziellen esptool-Fehlerbehebung](https://docs.espressif.com/projects/esptool/en/latest/esp32/troubleshooting.html).

## 3. Optional: Flash vollständig löschen

Für eine saubere Ersteinrichtung oder einen vollständigen Werksreset empfohlen.

### Windows

```powershell
py -m esptool --chip esp32 --port COM5 erase-flash
```

### Linux

```bash
python3 -m esptool --chip esp32 --port /dev/ttyUSB0 erase-flash
```

### macOS

```bash
python3 -m esptool --chip esp32 --port /dev/cu.usbserial-0001 erase-flash
```

Den Beispielanschluss jeweils durch den eigenen Anschluss ersetzen.

## 4. Factory-Datei flashen

In das Verzeichnis mit der heruntergeladenen Factory-Datei wechseln.

### Windows

```powershell
py -m esptool --chip esp32 --port COM5 --baud 460800 write-flash 0x0 eq3-gateway-wroom-v1.5.0-factory.bin
```

### Linux

```bash
python3 -m esptool --chip esp32 --port /dev/ttyUSB0 --baud 460800 write-flash 0x0 eq3-gateway-wroom-v1.5.0-factory.bin
```

### macOS

```bash
python3 -m esptool --chip esp32 --port /dev/cu.usbserial-0001 --baud 460800 write-flash 0x0 eq3-gateway-wroom-v1.5.0-factory.bin
```

Wichtig sind `--chip esp32` und die Flash-Adresse `0x0`. Die Datei ist bereits vollständig zusammengeführt; keine weiteren Binärdateien oder Adressen werden benötigt. Die Syntax entspricht der [offiziellen Espressif-Flash-Anleitung](https://docs.espressif.com/projects/esptool/en/latest/esp32/esptool/flashing-firmware.html).

Falls der Transfer mit 460800 Baud instabil ist, `--baud 115200` verwenden.

## 5. Falls keine Verbindung möglich ist

Viele Entwicklungsboards wechseln automatisch in den Bootloader. Wenn `Failed to connect` erscheint:

1. **BOOT** gedrückt halten.
2. Kurz **EN** beziehungsweise **RESET** drücken und wieder loslassen.
3. Den Flash-Befehl starten beziehungsweise weiterlaufen lassen.
4. **BOOT** loslassen, sobald esptool mit dem Schreiben beginnt.

Der ESP32 startet den seriellen Bootloader, wenn GPIO0 während des Resets auf LOW liegt. Details erklärt Espressif unter [Boot Mode Selection](https://docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/boot-mode-selection.html).

Zusätzlich prüfen:

- Ist es wirklich ein USB-Datenkabel?
- Ist der richtige COM-/USB-Anschluss ausgewählt?
- Ist der serielle Monitor oder ein anderes Programm geschlossen?
- Ist der passende CP210x-/CH340-Treiber installiert?
- Funktioniert ein anderes USB-Kabel oder ein direkter USB-Anschluss ohne Hub?

## 6. Erster Start

Nach erfolgreichem Flash startet der ESP normalerweise automatisch. Andernfalls einmal **EN/RESET** drücken.

1. Mit `eq3-gateway-XXXXXX` verbinden.
2. WLAN-Passwort `eq3setup` eingeben.
3. `http://192.168.4.1` öffnen.
4. Gateway konfigurieren und neu starten.

## Factory oder OTA?

| Datei | Verwendung | Flash-Adresse | Einstellungen |
|---|---|---:|---|
| `*-factory.bin` | Erster USB-Flash oder vollständiger Werksreset | `0x0` | werden beim vollständigen Löschen entfernt |
| `*-ota.bin` | Update über die laufende Weboberfläche | automatisch | bleiben erhalten |

Eine Factory-Datei niemals über das OTA-Formular hochladen.

## Prüfsumme kontrollieren

Für Version 1.5.0:

```text
SHA-256 Factory: 115FE37E0A8D6EFFC5654C50629BF454A19175507B405E5DA5A3C76438B5D24C
SHA-256 OTA:     1B0F8CF3A872747F703B39EAA9AE3E0D5DF90C8133362E2D84A41F525CB64F7D
```

### Windows PowerShell

```powershell
Get-FileHash .\eq3-gateway-wroom-v1.5.0-factory.bin -Algorithm SHA256
```

### Linux

```bash
sha256sum eq3-gateway-wroom-v1.5.0-factory.bin
```

### macOS

```bash
shasum -a 256 eq3-gateway-wroom-v1.5.0-factory.bin
```


