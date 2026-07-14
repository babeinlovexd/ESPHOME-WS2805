# 💡 ESPHome WS2805 External Component
<div align="center">
  <img src="https://img.shields.io/github/v/release/babeinlovexd/ESPHOME-WS2805?style=for-the-badge&color=2ecc71" alt="Latest Release">
  <img src="https://img.shields.io/badge/Status-Stable-2ecc71?style=for-the-badge" alt="Status">
  <img src="https://img.shields.io/badge/ESPHome-Ready-03A9F4?style=for-the-badge&logo=esphome" alt="ESPHome">
  <img src="https://img.shields.io/badge/ESP--IDF-Ready-E7352C?style=for-the-badge&logo=espressif" alt="ESP-IDF Ready">
  <img src="https://img.shields.io/badge/Arduino-Ready-00979D?style=for-the-badge&logo=arduino" alt="Arduino Ready">
  <img src="https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey?style=for-the-badge&logo=creative-commons" alt="License: CC BY-NC-SA 4.0">
</div>
<br>

🌍 **[Read this in English](README.md)**

Dies ist eine externe Komponente (External Component) für ESPHome, die Unterstützung für **WS2805** 5-Kanal (RGB + Warmweiß + Kaltweiß) LED-Streifen bietet.

Da ESPHome standardmäßig bei `AddressableLight` maximal 4 Kanäle (RGBW) unterstützt, der WS2805 aber 5 Kanäle (40 Bits pro LED) für RGBCCT nutzt, verhält sich diese Komponente bei RGB-Effekten wie ein normales `AddressableLight`. Gleichzeitig behält sie jedoch die globale Kontrolle über die Kanäle W1 und W2.

Dadurch wird der Home Assistant Farbwähler (UI) perfekt unterstützt: Es gibt keine "springenden" Regler mehr bei der Farbtemperatur (CCT), und adressierbare RGB-Effekte funktionieren einwandfrei.

### 🔥 Was kann das Teil ALLES?
- **Adressierbare RGB-Effekte:** Da die Komponente von `AddressableLight` erbt, kannst du alle adressierbaren Lichteffekte wie `addressable_rainbow`, `addressable_scan` usw. nutzen.
- **Globale CCT-Steuerung:** Die Warmweiß- und Kaltweiß-Kanäle werden für den gesamten Streifen anhand der CCT-Regler in Home Assistant global gesteuert (Genau so, wie WLED RGBCCT-Setups verwaltet).
- **Multi-Strip Support (Nativer RMT):** Nutzt nun ESPHomes hochoptimierte `esp32_rmt_led_strip` Architektur anstelle von `NeoPixelBus`. So werden RMT-Kanäle, Interrupt-Flags und SRAM ressourcenschonend verwaltet. Das ermöglicht bis zu 8 parallele Instanzen ohne `ESP_ERR_INVALID_STATE` Limits, speziell auf modernen Chips wie dem ESP32-S3.
- **Helligkeitsskalierung:** Korrektes Mapping und Skalierung der 5 Kanäle relativ zur Gesamthelligkeit.

---

## 🛠️ Verwendung in ESPHome

Um diese Komponente zu nutzen, kannst du sie direkt von GitHub über den `external_components` Block in deine Konfiguration einbinden. 
Dabei können problemlos mehrere Zonen bzw. Streifen parallel auf einem ESP32 (z.B. ESP32-S3) laufen, ohne dass es zu RMT-Crashes kommt.

```yaml
esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf # Kompatibel mit arduino und esp-idf

external_components:
  - source:
      type: git
      url: https://github.com/babeinlovexd/ESPHOME-WS2805
      ref: main
    components: [ ws2805 ]

light:
  - platform: ws2805
    id: ws2805_zone_1
    name: "Mein WS2805 Streifen - Zone 1"
    pin: GPIO4 # Der GPIO-Pin, an den deine primäre Datenleitung angeschlossen ist (din_pin geht auch)
    # fdin_pin: GPIO5 # Optional. Backup-Datenleitung für WS2805 Streifen mit redundantem Eingang
    num_leds: 100 # Gesamtzahl der LEDs auf dem Streifen
    # Optionale Konfigurationen mit Standardwerten
    channel_order: GRBWWCW # Optional. Unterstützt RGBWWCW, RGBCWWW, GRBWWCW, GRBCWWW. Standard: GRBWWCW
    color_interlock: false # Optional. Verhindert gleichzeitige maximale Helligkeit von Weiß und RGB. Standard: false
    constant_brightness: false # Optional. Verhindert die künstliche Drosselung der Helligkeit. Standard: false
    cold_white_color_temperature: 153 mireds # Optional. Farbtemperatur Kaltweiß. Standard: 153 mireds
    warm_white_color_temperature: 500 mireds # Optional. Farbtemperatur Warmweiß. Standard: 500 mireds
    cct_transition_speed: 3s # Optional. Geschwindigkeit für CCT-Übergänge. Standard: 3s
    dithering: true # Optional. Temporales Dithering für CCT. Standard: false
    max_refresh_rate: 4ms # Optional. Maximale Aktualisierungsrate. Standard: 4ms
    isr_priority: 3 # Optional. Hardware-Interrupt Priorität (1-3). Standard: 3
    gamma_correct: 2.2 # Optional (von ESPHome). Standard: 2.2
    effects:
      - addressable_rainbow:

  - platform: ws2805
    id: ws2805_zone_2
    name: "Mein WS2805 Streifen - Zone 2"
    pin: GPIO5
    num_leds: 100

  - platform: ws2805
    id: ws2805_zone_3
    name: "Mein WS2805 Streifen - Zone 3"
    pin: GPIO6
    num_leds: 100
```

---

## ⚙️ Konfigurations-Variablen

Du kannst alle Standard-ESPHome-Variablen (wie `name`, `id`, `gamma_correct`, `effects`) nutzen, zuzüglich folgender WS2805-spezifischer Argumente:

* **`pin`** *(Erforderlich)*: Der GPIO-Pin, an den deine primäre Datenleitung angeschlossen ist. Seit neuestem kannst du alternativ auch **`din_pin`** dafür verwenden.
* **`fdin_pin`** *(Optional)*: Backup-Datenleitung (DIN2 / FDIN) für WS2805 Streifen, die über einen zweiten, redundanten Eingang verfügen. Dieses Signal wird perfekt in Hardware mit der Hauptleitung (DIN) synchronisiert (auf unterstützten SoCs wie dem ESP32-S3 via ESP-IDF v5), sodass bei einem Kabelbruch nahtlos die Ersatzleitung einspringt.
* **`num_leds`** *(Erforderlich)*: Gesamtzahl der LEDs auf dem Streifen.
* **`channel_order`** *(Optional, string)*: Legt die Reihenfolge der Farbkanäle für den LED-Strip fest. Unterstützt werden `RGBWWCW`, `RGBCWWW`, `GRBWWCW` oder `GRBCWWW`. Standard ist `GRBWWCW`.
* **`color_interlock`** *(Optional, Boolean)*: Verhindert, dass die weißen LEDs und die RGB-LEDs gleichzeitig mit voller Kraft leuchten (nützlich für das Netzteil-Management oder thermische Limits). Standard ist `false`.
* **`constant_brightness`** *(Optional, Boolean)*: Deaktiviert die ESPHome-interne Helligkeitsdrosselung der Kanäle und verhält sich wie eine Standard-ESPHome-RGBWW-Lampe (ermöglicht 100 % Leistung auf allen Kanälen). Standard ist `false`.
* **`cold_white_color_temperature`** *(Optional)*: Die Farbtemperatur deiner Kaltweiß-LEDs in Mireds. Standardwert ist `153 mireds` (~6500K).
* **`warm_white_color_temperature`** *(Optional)*: Die Farbtemperatur deiner Warmweiß-LEDs in Mireds. Standardwert ist `500 mireds` (~2000K).
* **`cct_transition_speed`** *(Optional, time)*: Steuert die Geschwindigkeit der Fade-Übergänge für die weißen (CCT) Kanäle in Sekunden/Millisekunden (z.B. `3s`). Der Standardwert ist `3s`.
* **`max_refresh_rate`** *(Optional, time)*: Limitiert die maximale Updaterate um RMT Timeouts zu verhindern. Standard ist `4ms`.
* **`dithering`** *(Optional, Boolean)*: Aktiviert temporales Dithering für die weißen (CW/WW) Kanäle, was das Ruckeln/Flimmern bei geringer Helligkeit oder langsamem Faden verringert. Standard ist `false`. Siehe [docu.md](docu.md) für weitere Details.
* **`isr_priority`** *(Optional, int)*: Legt die Priorität (1-3) des RMT-Hardware-Interrupts fest. Der Standardwert `3` (höchste C-Level Priorität) verhindert, dass der WLAN/Bluetooth-Stack des ESP32 die RMT-Datenübertragung unterbricht und ungewollte weiße Blitze auf dem LED-Streifen verursacht.

#### Erweiterte RMT Timing Einstellungen
Die Komponente nutzt optimierte Standardwerte für das Datensignal, die genau in der Mitte der WS2805-Datenblattspezifikation liegen. Solltest du allerdings Chips von Drittanbietern nutzen, die abweichen, lassen sich die Signalzeiten präzise in Nanosekunden überschreiben:
* **`bit0_high_ns`** *(Optional, int)*: Standard `300`.
* **`bit0_low_ns`** *(Optional, int)*: Standard `800`.
* **`bit1_high_ns`** *(Optional, int)*: Standard `800`.
* **`bit1_low_ns`** *(Optional, int)*: Standard `800`.
* **`reset_pulse_us`** *(Optional, int)*: In Mikrosekunden. Standard `300`.

#### Diagnostics API
Wenn du tiefergehende Hardware- und Speicherstatistiken direkt im Home Assistant einsehen möchtest, kannst du über Template Sensoren auf diese internen Funktionen zugreifen. Verwende dazu einfach die ID deiner WS2805-Lampe (z. B. `id(ws2805_zone_1)`):
* `get_num_leds()`: Anzahl der angesteuerten Pixel.
* `get_frame_bytes()`: In Bytes, wie groß ein zu sendendes Frame ist.
* `get_rmt_resolution_hz()`: Automatisch erkannte RMT Taktfrequenz.
* `get_last_frame_ms()`: Dauer (in Millisekunden), um den RMT Buffer zu füllen und zu senden.
* `get_max_refresh_hz()`: Theoretisches Maximum der Updaterate für Animationen.
* `get_tx_error_count()`: Anzahl der Übertragungsfehler (sollte bei 0 bleiben).

---

## ☕ Support dieses Projekts

Wenn dir diese ESPHome Komponente gefällt und du meine Arbeit unterstützen möchtest, freue ich mich riesig über einen virtuellen Kaffee!

<a href="https://www.paypal.me/babeinlovexd">
  <img src="https://img.shields.io/badge/Donate-PayPal-blue.svg?style=for-the-badge&logo=paypal" alt="Donate mit PayPal">
</a>

---

## 👨‍💻 Entwickelt von

| [<img src="https://avatars.githubusercontent.com/u/43302033?v=4" width="100"><br><sub>**Christopher**</sub>](https://github.com/babeinlovexd) |
| :---: |

---
