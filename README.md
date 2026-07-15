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
    # Optional configurations with default values
    channel_order: GRBWWCW # Optional. Supports RGBWWCW, RGBCWWW, GRBWWCW, GRBCWWW. Default: GRBWWCW
    color_interlock: false # Optional. Prevents simultaneous maximum brightness of white and RGB. Default: false
    constant_brightness: false # Optional. Prevents artificial brightness throttling. Default: false
    cold_white_color_temperature: 153 mireds # Optional. Color temperature for cold white. Default: 153 mireds
    warm_white_color_temperature: 500 mireds # Optional. Color temperature for warm white. Default: 500 mireds
    cct_transition_speed: 3s # Optional. Speed for CCT transitions. Default: 3s
    dithering: true # Optional. Temporal dithering for CCT. Default: false
    max_refresh_rate: 4ms # Optional. Maximum refresh rate. Default: 4ms
    gamma_correct: 2.2 # Optional (from ESPHome). Default: 2.2
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

* **`pin`** *(Required)*: The GPIO pin your primary data line is connected to. As of recently, you can also use **`din_pin`** as an alias for this option.
* **`fdin_pin`** *(Optional)*: Backup data line (DIN2 / FDIN) for WS2805 strips that feature a secondary redundant input pad. This output is perfectly hardware-synchronized with the main DIN line (on supported SoCs like ESP32-S3 via ESP-IDF v5) so the backup line is automatically used if the primary line fails.
* **`num_leds`** *(Required)*: Total number of LEDs on the strip.
* **`channel_order`** *(Optional, string)*: Defines the color channel order for your LED strip. Can be `RGBWWCW`, `RGBCWWW`, `GRBWWCW`, or `GRBCWWW`. Defaults to `GRBWWCW`.
* **`color_interlock`** *(Optional, boolean)*: Prevents white LEDs and RGB LEDs from being at full brightness simultaneously (useful for power supply management or thermal limits). Defaults to `false`.
* **`constant_brightness`** *(Optional, boolean)*: Disables the ESPHome internal brightness throttling and behaves like a standard ESPHome RGBWW light (allowing 100% power on all channels). Defaults to `false`.
* **`cold_white_color_temperature`** *(Optional)*: The color temperature of your cold white LEDs in mireds. Default value is `153 mireds` (~6500K).
* **`warm_white_color_temperature`** *(Optional)*: The color temperature of your warm white LEDs in mireds. Default value is `500 mireds` (~2000K).
* **`cct_transition_speed`** *(Optional, time)*: Controls the speed of fading transitions for the white (CCT) channels. Default value is `3s`.
* **`max_refresh_rate`** *(Optional, time)*: Limits the maximum update rate to prevent RMT timeouts. Defaults to `4ms`.
* **`dithering`** *(Optional, boolean)*: Enables temporal dithering for the white (CW/WW) channels, reducing stepping/flickering at low brightness or during slow fading. Defaults to `false`. See [docu.md](docu.md) for more details.
* **`isr_priority`** *(Optional, int)*: Specifies the priority (1-3) of the RMT hardware interrupt. Defaults to `3` (highest C-level priority) to prevent the WiFi/BLE stack from starving the RMT refill and causing white flashes on the strip.

#### Advanced Timing Overrides
The component uses highly optimized defaults for RMT signal timing that center directly in the WS2805 hardware specifications. However, if you are experiencing issues with custom clone chips, you can manually override the timing using the following properties (in nanoseconds, unless specified):
* **`bit0_high_ns`** *(Optional, int)*: Default `300`.
* **`bit0_low_ns`** *(Optional, int)*: Default `800`.
* **`bit1_high_ns`** *(Optional, int)*: Default `800`.
* **`bit1_low_ns`** *(Optional, int)*: Default `800`.
* **`reset_pulse_us`** *(Optional, int)*: Microseconds. Default `300`.

#### Diagnostics API
If you want to view hardware and memory performance in Home Assistant, you can expose these internal functions via template sensors. Just use the `id` of your light component (e.g., `id(ws2805_zone_1)`):
* `get_num_leds()`: Number of pixels.
* `get_frame_bytes()`: Bytes clocked out per frame.
* `get_rmt_resolution_hz()`: Auto-detected RMT clock tick.
* `get_last_frame_ms()`: Blocking cost of the last frame.
* `get_max_refresh_hz()`: Theoretical maximum update rate.
* `get_tx_error_count()`: Number of RMT buffer drops (should be 0).

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
