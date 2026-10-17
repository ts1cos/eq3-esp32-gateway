# Vorgebaute Firmware

- `esp32-wroom-32d/firmware.bin`: ältere unversionierte WROOM-OTA-Datei; sie bleibt als Bestand erhalten.
- `esp32-wroom-32d/eq3-gateway-wroom-vVERSION-ota.bin`: versionierte OTA-Dateien für die Weboberfläche.
- `esp32-wroom-32d/eq3-gateway-wroom-vVERSION-factory.bin`: vollständige Images für den ersten USB-Flash ab Adresse `0x0`.
- Ältere Dateien ohne `-ota` oder `-factory` im Namen stammen aus vorherigen Versionen und bleiben erhalten.
- `esp32-c6/firmware.bin`: OTA-Datei für ESP32-C6.
- `esp32-c6/firmware.factory.bin`: Kombinierte C6-Datei für den ersten Flash inklusive Bootloader und Partitionstabelle.

Die aktuelle Entwicklung und Prüfung erfolgt ausschließlich für das Ziel `esp32-wroom-32d`. Die vorhandenen C6-Dateien stammen von einem älteren Quellstand.

Aktuelle WROOM-Version: **1.4.0**. Sie ermöglicht das nachträgliche Ändern von Thermostatnamen und Pairing-PINs.


