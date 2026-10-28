<p align="center">
  <img src="docs/icon-preview.png" alt="MilleIpSwitch" width="420">
</p>

<h1 align="center">MilleIpSwitch</h1>

<p align="center">
  Schneller, schlanker Netzwerkprofil-Umschalter für Windows – eine kleine Alternative zu NetSetMan.<br>
  <a href="README.md">English</a> · <b>Deutsch</b>
</p>

| Tray-Menü | Profilverwaltung | Netzwerkstatus |
|---|---|---|
| ![Tray-Menü](docs/screenshot-tray.png) | ![Profilverwaltung](docs/screenshot-profiles.png) | ![Netzwerkstatus](docs/screenshot-status.png) |

## Funktionen

- **IPv4-Profile in Millisekunden umschalten** – feste IP (auch mehrere pro Adapter), Subnetzmaske, Gateway, DNS oder DHCP
- **Tray-Menü** mit Markierung der aktiven Profile; pro Adapter kann gleichzeitig ein Profil aktiv sein
- **Kurzübersicht** beim Überfahren des Tray-Symbols: verbundene Adapter, IPs, Geschwindigkeit und aktive Profile
- **Profilverwaltung** im Stil des Windows-IPv4-Dialogs, inklusive „aktuelle Einstellungen des Adapters übernehmen“
- **Netzwerkstatus** aller Adapter, gruppiert nach Ethernet, WLAN, VPN, virtuell und Sonstige – mit DHCP-Server und MAC, Ein/Aus-Schalter pro Adapter und Sprung in die Adaptereigenschaften
- **Kommandozeile** für Skripte, Hotkeys und das Stream Deck: `milleipswitch.exe "Labor"`
- **Autostart ohne UAC-Abfrage** (Aufgabenplanung) – danach brauchen auch Kommandozeilenaufrufe keine Adminabfrage
- Deutsch, Englisch, Spanisch und Französisch
- Eine einzelne portable `.exe` (~500 KB), kein Installer, keine Laufzeitumgebung, kein Dienst

## Warum es schnell ist

MilleIpSwitch setzt Adressen, Gateway und DNS direkt über die IP-Helper-API statt über `netsh`/WMI und ändert nur, was sich vom Ist-Zustand unterscheidet. Nur der Wechsel zwischen DHCP und statisch läuft über einen einzigen `netsh`-Aufruf. Die benötigte Zeit steht in der Benachrichtigung.

## Download

`milleipswitch.exe` aus dem [neuesten Release](../../releases/latest) in einen beliebigen Ordner legen, z. B. `C:\Tools\MilleIpSwitch\`. Die Profile liegen in `profiles.ini` neben der EXE.

> Die EXE ist nicht signiert; Windows SmartScreen kann beim ersten Start warnen („Weitere Informationen“ → „Trotzdem ausführen“).
> Da MilleIpSwitch Netzwerkeinstellungen ändert und eine Autostart-Aufgabe mit Adminrechten anlegen kann, melden manche
> Virenscanner es heuristisch. Der komplette Quellcode liegt in diesem Repository; du kannst es auch selbst bauen.

## Bedienung

| Aufruf | Wirkung |
|---|---|
| `milleipswitch.exe` | Startet MilleIpSwitch: Tray-Symbol und Profilverwaltung |
| `milleipswitch.exe --tray` | Nur Tray-Symbol (für den Autostart) |
| `milleipswitch.exe "<Profil>"` | Profil anwenden |
| `milleipswitch.exe --list` | Profile auflisten (`*` = aktiv) |
| `milleipswitch.exe --status` | Alle Adapter anzeigen |
| `milleipswitch.exe --edit` | Profilverwaltung öffnen |
| `milleipswitch.exe --version` | Version anzeigen |

Mit **Mit Windows starten** im Tray-Menü startet MilleIpSwitch bei der Anmeldung mit Adminrechten, aber ohne UAC-Abfrage.

## Selbst bauen

Siehe [README.md → Building](README.md#building). Kurz: Ordner in Visual Studio oder VS Code (CMake-Erweiterung) öffnen und bauen, oder `build.bat` in der *x64 Native Tools Command Prompt* ausführen.

## Lizenz

[MIT](LICENSE)
