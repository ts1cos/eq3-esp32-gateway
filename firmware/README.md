# Vorgebaute Firmware

- `esp32-wroom-32d/firmware.bin`: ältere unversionierte WROOM-OTA-Datei; sie bleibt als Bestand erhalten.
- `esp32-wroom-32d/eq3-gateway-wroom-vVERSION-ota.bin`: versionierte OTA-Dateien für die Weboberfläche.
- `esp32-wroom-32d/eq3-gateway-wroom-vVERSION-factory.bin`: vollständige Images für den ersten USB-Flash ab Adresse `0x0`.
- Ältere Dateien ohne `-ota` oder `-factory` im Namen stammen aus vorherigen Versionen und bleiben erhalten.
- `latest.json`: kleine Versionsinformation, die das Gateway für die Updateprüfung abruft.

Entwicklung, Prüfung und Veröffentlichung erfolgen ausschließlich für das Ziel `esp32-wroom-32d`. Für den ESP32-C6 wird keine Firmware mehr bereitgestellt.

Aktuelle WROOM-Version: **1.5.0**. Das Gateway kann diese Version auf GitHub prüfen und ein verfügbares Update in der Weboberfläche anzeigen.



