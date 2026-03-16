# ESPHome WS2805 External Component

*[Deutsche Version unten](#deutsche-version)*

This is an external component for ESPHome that provides support for **WS2805** 5-channel (RGB + Warm White + Cold White) LED strips.

ESPHome's built-in `AddressableLight` primarily maps to a maximum of 4 channels (RGBW). Since WS2805 requires 5 channels (40 bits per pixel) for RGBCCT support, this component operates as an `AddressableLight` for RGB effects, while maintaining global control over the W1 and W2 channels.

This maps perfectly to the Home Assistant UI, providing correct Addressable RGB effects and global CCT (Color Temperature) control without jumping sliders.

## Usage in ESPHome

To use this component, you can include it directly from GitHub using the `external_components` block. 
You can define multiple zones/strips running in parallel on the ESP32 (e.g., ESP32-S3) without RMT crashes.

```yaml
esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf # Also compatible with arduino

external_components:
  - source:
      type: git
      url: https://github.com/babeinlovexd/esph-ws2805
      ref: main
    components: [ ws2805 ]

light:
  - platform: ws2805
    id: ws2805_zone_1
    name: "My WS2805 Strip - Zone 1"
    pin: GPIO4 # The GPIO pin your data line is connected to
    num_leds: 100 # Total number of LEDs on the strip
    color_interlock: false
    cold_white_color_temperature: 153 mireds
    warm_white_color_temperature: 500 mireds
    gamma_correct: 2.2
    effects:
      - addressable_rainbow:

  - platform: ws2805
    id: ws2805_zone_2
    name: "My WS2805 Strip - Zone 2"
    pin: GPIO5
    num_leds: 100

  - platform: ws2805
    id: ws2805_zone_3
    name: "My WS2805 Strip - Zone 3"
    pin: GPIO6
    num_leds: 100
```
## Configuration Variables

You can use all standard ESPHome variables (like `name`, `id`, `gamma_correct`, `effects`), plus the following WS2805-specific arguments:

* **`pin`** *(Required)*: The GPIO pin your data line is connected to.
* **`num_leds`** *(Required)*: Total number of LEDs on the strip.
* **`color_interlock`** *(Optional, boolean)*: Prevents white LEDs and RGB LEDs from being at full brightness simultaneously (useful for power supply management or thermal limits). Defaults to `false`.
* **`cold_white_color_temperature`** *(Optional)*: The color temperature of your cold white LEDs in mireds. Default value is `153 mireds` (~6500K).
* **`warm_white_color_temperature`** *(Optional)*: The color temperature of your warm white LEDs in mireds. Default value is `500 mireds` (~2000K).

---
## Features
- **Addressable RGB Effects:** Since the component inherits from `AddressableLight`, you can add and use all Addressable light effects such as `addressable_rainbow`, `addressable_scan`, etc.
- **Global CCT Control:** The Warm White and Cold White channels are globally set across the entire strip according to your CCT sliders in Home Assistant. This is exactly how tools like WLED manage RGBCCT setups.
- **Multi-Strip Support (Native RMT):** Uses ESPHome's highly optimized, native `esp32_rmt_led_strip` architecture instead of `NeoPixelBus`. This safely manages RMT channels, interrupt flags, and SRAM, allowing up to 8 parallel instances without `ESP_ERR_INVALID_STATE` limits on modern chips like the ESP32-S3.
- **Brightness Scaling:** Proper mapping and scaling of the 5 channels relative to overall brightness.

---

# Deutsche Version

Dies ist eine externe Komponente (External Component) für ESPHome, die Unterstützung für **WS2805** 5-Kanal (RGB + Warmweiß + Kaltweiß) LED-Streifen bietet.

Da ESPHome standardmäßig bei `AddressableLight` maximal 4 Kanäle (RGBW) unterstützt, der WS2805 aber 5 Kanäle (40 Bits pro LED) für RGBCCT nutzt, verhält sich diese Komponente bei RGB-Effekten wie ein normales `AddressableLight`. Gleichzeitig behält sie jedoch die globale Kontrolle über die Kanäle W1 und W2.

Dadurch wird der Home Assistant Farbwähler (UI) perfekt unterstützt: Es gibt keine "springenden" Regler mehr bei der Farbtemperatur (CCT), und adressierbare RGB-Effekte funktionieren einwandfrei.

## Verwendung in ESPHome

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
## Konfigurations-Variablen

Du kannst alle Standard-ESPHome-Variablen (wie `name`, `id`, `gamma_correct`, `effects`) nutzen, zuzüglich folgender WS2805-spezifischer Argumente:

* **`pin`** *(Erforderlich)*: Der GPIO-Pin, an den deine Datenleitung angeschlossen ist.
* **`num_leds`** *(Erforderlich)*: Gesamtzahl der LEDs auf dem Streifen.
* **`color_interlock`** *(Optional, Boolean)*: Verhindert, dass die weißen LEDs und die RGB-LEDs gleichzeitig mit voller Kraft leuchten (nützlich für das Netzteil-Management oder thermische Limits). Standard ist `false`.
* **`cold_white_color_temperature`** *(Optional)*: Die Farbtemperatur deiner Kaltweiß-LEDs in Mireds. Standardwert ist `153 mireds` (~6500K).
* **`warm_white_color_temperature`** *(Optional)*: Die Farbtemperatur deiner Warmweiß-LEDs in Mireds. Standardwert ist `500 mireds` (~2000K).
## Funktionen
- **Adressierbare RGB-Effekte:** Da die Komponente von `AddressableLight` erbt, kannst du alle adressierbaren Lichteffekte wie `addressable_rainbow`, `addressable_scan` usw. nutzen.
- **Globale CCT-Steuerung:** Die Warmweiß- und Kaltweiß-Kanäle werden für den gesamten Streifen anhand der CCT-Regler in Home Assistant global gesteuert (Genau so, wie WLED RGBCCT-Setups verwaltet).
- **Multi-Strip Support (Nativer RMT):** Nutzt nun ESPHomes hochoptimierte `esp32_rmt_led_strip` Architektur anstelle von `NeoPixelBus`. So werden RMT-Kanäle, Interrupt-Flags und SRAM ressourcenschonend verwaltet. Das ermöglicht bis zu 8 parallele Instanzen ohne `ESP_ERR_INVALID_STATE` Limits, speziell auf modernen Chips wie dem ESP32-S3.
- **Helligkeitsskalierung:** Korrektes Mapping und Skalierung der 5 Kanäle relativ zur Gesamthelligkeit.
