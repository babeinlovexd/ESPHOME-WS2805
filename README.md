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

🌍 **[Lies dies auf Deutsch](README_de.md)**

This is an external component for ESPHome that provides support for **WS2805** 5-channel (RGB + Warm White + Cold White) LED strips.

ESPHome's built-in `AddressableLight` primarily maps to a maximum of 4 channels (RGBW). Since WS2805 requires 5 channels (40 bits per pixel) for RGBCCT support, this component operates as an `AddressableLight` for RGB effects, while maintaining global control over the W1 and W2 channels.

This maps perfectly to the Home Assistant UI, providing correct Addressable RGB effects and global CCT (Color Temperature) control without jumping sliders.

### 🔥 What can it DO?
- **Addressable RGB Effects:** Since the component inherits from `AddressableLight`, you can add and use all Addressable light effects such as `addressable_rainbow`, `addressable_scan`, etc.
- **Global CCT Control:** The Warm White and Cold White channels are globally set across the entire strip according to your CCT sliders in Home Assistant. This is exactly how tools like WLED manage RGBCCT setups.
- **Multi-Strip Support (Native RMT):** Uses ESPHome's highly optimized, native `esp32_rmt_led_strip` architecture instead of `NeoPixelBus`. This safely manages RMT channels, interrupt flags, and SRAM, allowing up to 8 parallel instances without `ESP_ERR_INVALID_STATE` limits on modern chips like the ESP32-S3.
- **Brightness Scaling:** Proper mapping and scaling of the 5 channels relative to overall brightness.

---

## 🛠️ Usage in ESPHome

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
      url: https://github.com/babeinlovexd/ESPHOME-WS2805
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

---

## ⚙️ Configuration Variables

You can use all standard ESPHome variables (like `name`, `id`, `gamma_correct`, `effects`), plus the following WS2805-specific arguments:

* **`pin`** *(Required)*: The GPIO pin your data line is connected to.
* **`num_leds`** *(Required)*: Total number of LEDs on the strip.
* **`color_interlock`** *(Optional, boolean)*: Prevents white LEDs and RGB LEDs from being at full brightness simultaneously (useful for power supply management or thermal limits). Defaults to `false`.
* **`cold_white_color_temperature`** *(Optional)*: The color temperature of your cold white LEDs in mireds. Default value is `153 mireds` (~6500K).
* **`warm_white_color_temperature`** *(Optional)*: The color temperature of your warm white LEDs in mireds. Default value is `500 mireds` (~2000K).

---

## ☕ Support this Project

If you like this ESPHome component and want to support my work, I'd be absolutely thrilled about a virtual coffee!

<a href="https://www.paypal.me/babeinlovexd">
  <img src="https://img.shields.io/badge/Donate-PayPal-blue.svg?style=for-the-badge&logo=paypal" alt="Donate with PayPal">
</a>

---

## 👨‍💻 Developed by

| [<img src="https://avatars.githubusercontent.com/u/43302033?v=4" width="100"><br><sub>**Christopher**</sub>](https://github.com/babeinlovexd) |
| :---: |

---
