# Versionshistorie

## 1.5.0

- Manuelle und automatische Prüfung auf neue Firmware über GitHub
- Anzeige der installierten und der auf GitHub verfügbaren Version
- Direkter Link zum neuesten GitHub Release in der Weboberfläche
- Komprimierte Auslieferung der Weboberfläche, damit das bestehende OTA-Partitionslayout erhalten bleibt
- Altes ESP32-C6-Firmwarearchiv entfernt; veröffentlicht wird ausschließlich WROOM-32D-Firmware

## 1.4.1

- Web-Anmeldung erhält einen eindeutigen Ein-/Aus-Schalter und ist standardmäßig ausgeschaltet
- Deaktivieren des Schalters löscht ein zuvor gespeichertes Web-Passwort
- Passwortmanager können das Web-Passwort beim Setup nicht mehr unbemerkt aktivieren
- Bei aktivierter Web-Anmeldung wird der Benutzername `admin` direkt angezeigt

## 1.4.0

- Thermostatnamen nachträglich ändern
- Pairing-PIN nachträglich ändern oder löschen
- Neue Namen sofort per MQTT und Home Assistant Discovery veröffentlichen

## 1.3.1

- Zu frühen Neustart nach OTA behoben
- OTA-Upload läuft im Hintergrund auf der Hauptseite
- Automatische Rückkehr nach dem Neustart zuverlässiger gemacht
- Browser-Cache für die Hauptseite deaktiviert

## 1.3.0

- Einstellbares MQTT-Aktualisierungsintervall, Standard 5 Sekunden
- Getrennte, versionierte OTA- und Factory-Dateien
- Vollständiges Factory-Image für Flash-Adresse `0x0`

## 1.2.0

- Umschaltbare Konfiguration für DHCP oder feste IPv4-Adresse
- Einstellungen für Subnetz, Gateway und zwei DNS-Server
- Aktuelle IP-Adresse in der Weboberfläche

## 1.1.0

- Änderbarer Netzwerk- und mDNS-Name
- Anzeige von Version und Ersteller
- Versioniertes Firmwarearchiv

## 1.0.0

- Einrichtungs-WLAN und lokale Weboberfläche
- Bis zu drei eQ-3/Eqiva-Thermostate
- MQTT und Home Assistant Discovery
- OTA-Update und Neustart-Funktion


