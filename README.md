# 💡 ESPHome WS2805 External Component
<div align="center">
  <img src="https://img.shields.io/github/v/release/babeinlovexd/esph-ws2805?style=for-the-badge&color=2ecc71" alt="Latest Release">
  <img src="https://img.shields.io/badge/Status-Stable-2ecc71?style=for-the-badge" alt="Status">
  <img src="https://img.shields.io/badge/ESPHome-Ready-03A9F4?style=for-the-badge&logo=esphome" alt="ESPHome">
  <img src="https://img.shields.io/badge/ESP--IDF-Ready-E7352C?style=for-the-badge&logo=espressif" alt="ESP-IDF Ready">
  <img src="https://img.shields.io/badge/Arduino-Ready-00979D?style=for-the-badge&logo=arduino" alt="Arduino Ready">
  <img src="https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey?style=for-the-badge&logo=creative-commons" alt="License: CC BY-NC-SA 4.0">
</div>
<br>

🌍 **[Read this in English](README_en.md)**

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
      url: https://github.com/babeinlovexd/esph-ws2805
      ref: main
    components: [ ws2805 ]

light:
  - platform: ws2805
    id: ws2805_zone_1
    name: "Mein WS2805 Streifen - Zone 1"
    pin: GPIO4 # Der GPIO-Pin, an den deine Datenleitung angeschlossen ist
    num_leds: 100 # Gesamtzahl der LEDs auf dem Streifen
    color_interlock: false
    cold_white_color_temperature: 153 mireds
    warm_white_color_temperature: 500 mireds
    gamma_correct: 2.2
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

* **`pin`** *(Erforderlich)*: Der GPIO-Pin, an den deine Datenleitung angeschlossen ist.
* **`num_leds`** *(Erforderlich)*: Gesamtzahl der LEDs auf dem Streifen.
* **`color_interlock`** *(Optional, Boolean)*: Verhindert, dass die weißen LEDs und die RGB-LEDs gleichzeitig mit voller Kraft leuchten (nützlich für das Netzteil-Management oder thermische Limits). Standard ist `false`.
* **`cold_white_color_temperature`** *(Optional)*: Die Farbtemperatur deiner Kaltweiß-LEDs in Mireds. Standardwert ist `153 mireds` (~6500K).
* **`warm_white_color_temperature`** *(Optional)*: Die Farbtemperatur deiner Warmweiß-LEDs in Mireds. Standardwert ist `500 mireds` (~2000K).

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
