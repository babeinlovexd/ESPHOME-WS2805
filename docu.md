# 💡 WS2805 LED Strip Component Documentation
<div align="center">
  <img src="https://img.shields.io/github/v/release/babeinlovexd/ESPHOME-WS2805?style=for-the-badge&color=2ecc71" alt="Latest Release">
  <img src="https://img.shields.io/badge/Status-Stable-2ecc71?style=for-the-badge" alt="Status">
  <img src="https://img.shields.io/badge/ESPHome-Ready-03A9F4?style=for-the-badge&logo=esphome" alt="ESPHome">
  <img src="https://img.shields.io/badge/ESP--IDF-Ready-E7352C?style=for-the-badge&logo=espressif" alt="ESP-IDF Ready">
  <img src="https://img.shields.io/badge/Arduino-Ready-00979D?style=for-the-badge&logo=arduino" alt="Arduino Ready">
  <img src="https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey?style=for-the-badge&logo=creative-commons" alt="License: CC BY-NC-SA 4.0">
</div>
<br>

The `ws2805` light platform allows you to use WS2805 5-channel (RGB + Warm White + Cold White) Addressable LED strips with ESPHome.

Since standard ESPHome `AddressableLight` instances support a maximum of 4 channels (RGBW), the WS2805 requires special handling for its 5 channels (40 bits per LED). This component functions identically to an Addressable Light for all RGB-based effects (like `addressable_rainbow`, `addressable_scan`, etc.), but maintains global, non-addressable control over the W1 (Warm White) and W2 (Cold White) channels. This ensures that the Color Temperature (CCT) sliders in Home Assistant work smoothly without skipping or resetting.

## 🔌 Hardware Requirements
This component utilizes the highly optimized `esp32_rmt_led_strip` architecture. It is exclusively compatible with the ESP32 family of microcontrollers (ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6).

**Note:** Depending on your ESP32 variant, there is a hardware limit to how many concurrent WS2805 strips (RMT channels) you can define:
- **ESP32:** Max 8 channels
- **ESP32-S2 / ESP32-S3:** Max 4 channels
- **ESP32-C3 / ESP32-C6:** Max 2 channels

## ⚙️ Configuration Variables

```yaml
light:
  - platform: ws2805
    id: ws2805_light
    name: "WS2805 LED Strip"
    pin: GPIO4
    num_leds: 60
    # Optional parameters
    channel_order: GRBWWCW
    color_interlock: false
    constant_brightness: false
    cold_white_color_temperature: 153 mireds
    warm_white_color_temperature: 500 mireds
    cct_transition_speed: 3s
    max_refresh_rate: 4ms
    dithering: true
    isr_priority: 3
    gamma_correct: 2.2
```

* **pin** (*Required*, [Pin](https://esphome.io/guides/configuration-types.html#config-pin)): The GPIO pin connected to the data line of the WS2805 strip. You can also use **din_pin** as an alias for this option.
* **fdin_pin** (*Optional*, [Pin](https://esphome.io/guides/configuration-types.html#config-pin)): Backup data line (DIN2 / FDIN) for WS2805 strips that feature a secondary redundant input pad. This output is perfectly hardware-synchronized with the main DIN line (on supported SoCs like ESP32-S3 via ESP-IDF v5) so the backup line is automatically used if the primary line fails.
* **num_leds** (*Required*, int): The total number of LEDs on the strip. Must be a positive integer > 0.
* **channel_order** (*Optional*, string): Defines the color channel order for your LED strip. Can be `RGBWWCW`, `RGBCWWW`, `GRBWWCW`, or `GRBCWWW`. Defaults to `GRBWWCW`.
* **color_interlock** (*Optional*, boolean): If `true`, the RGB channels and White channels cannot be active at the same time. If RGB colors are selected, the white LEDs turn off. If a color temperature is selected, the RGB LEDs turn off. This is highly recommended to protect your power supply and prevent the LEDs from overheating. Defaults to `false`.
* **constant_brightness** (*Optional*, boolean): Disables the ESPHome internal brightness throttling and behaves like a standard ESPHome RGBWW light (allowing 100% power on all channels). Defaults to `false`.
* **cold_white_color_temperature** (*Optional*, color temperature): The physical color temperature of the Cold White LEDs in mireds. Defaults to `153 mireds` (~6500K).
* **warm_white_color_temperature** (*Optional*, color temperature): The physical color temperature of the Warm White LEDs in mireds. Defaults to `500 mireds` (~2000K).
* **cct_transition_speed** (*Optional*, [Time](https://esphome.io/guides/configuration-types.html#config-time)): Controls the duration of the fading animation when changing the white (CCT) channels. Defaults to `3s`.
* **max_refresh_rate** (*Optional*, [Time](https://esphome.io/guides/configuration-types.html#config-time)): The minimum time between RMT data transmissions to the strip. Useful for preventing buffer overflows on fast animations. Defaults to `4ms`.
* **dithering** (*Optional*, boolean): Enables **Temporal Dithering** (Error Diffusion) for the White channels. See the dedicated section below for details. Defaults to `false`.
* **isr_priority** (*Optional*, int): Specifies the priority (1-3) of the RMT hardware interrupt. Defaults to `3` (highest C-level priority) to prevent the WiFi/BLE stack from starving the RMT refill and causing white flashes on the strip.
* **gamma_correct** (*Optional*, float): Gamma correction factor (from ESPHome). Defaults to `2.2`.

### ⏱️ Advanced Timing Overrides
The component uses highly optimized defaults for RMT signal timing that center directly in the WS2805 hardware specifications. However, if you are experiencing issues with custom clone chips, you can manually override the timing using the following properties (in nanoseconds, unless specified):
* **bit0_high_ns** (*Optional*, int): Default `300`.
* **bit0_low_ns** (*Optional*, int): Default `800`.
* **bit1_high_ns** (*Optional*, int): Default `800`.
* **bit1_low_ns** (*Optional*, int): Default `800`.
* **reset_pulse_us** (*Optional*, int): Microseconds. Default `300`.

### 📊 Diagnostics API
If you want to view hardware and memory performance in Home Assistant, you can expose these internal functions via template sensors. Just use the `id` of your light component (e.g., `id(ws2805_light)`):
* `get_num_leds()`: Number of pixels.
* `get_frame_bytes()`: Bytes clocked out per frame.
* `get_rmt_resolution_hz()`: Auto-detected RMT clock tick.
* `get_last_frame_ms()`: Blocking cost of the last frame in milliseconds.
* `get_max_refresh_hz()`: Theoretical maximum update rate.
* `get_tx_error_count()`: Number of RMT buffer drops (should be 0).

---

## ⚡ Temporal Dithering (White Channels)

### What is Temporal Dithering?
When converting 32-bit floating-point brightness calculations (used internally by ESPHome) to 8-bit integers (0-255, required by the LED hardware), fractional data is inherently lost.

For example, if Home Assistant requests a brightness transition, a calculated target at a given millisecond might be `12.4`. The hardware will truncate this to `12`. This loss of `0.4` can cause visible "stepping" or "flickering," especially noticeable during very slow fades or at extremely low brightness levels (where the difference between `1` and `2` is visually drastic).

### How it Works
When `dithering: true` is set, the component implements **Temporal Error Diffusion** specifically for the CW and WW channels.

1. **Error Accumulation**: Instead of discarding the fractional difference, the component carries this "error" over to the next rendering frame.
2. **Sub-Pixel Alternation**: If the ideal value is `12.5`, the hardware will output `12` on the first frame (carrying over a `+0.5` error). On the next frame, the `+0.5` is added, resulting in `13`.
3. **Persistence of Vision**: Because the RMT hardware sends data to the LED strip thousands of times per second, the LED rapidly alternates between `12` and `13`. To the human eye, this blends together perfectly, making the LED appear as if it is outputting exactly `12.5`.

When dithering is active, it temporarily forces continuous ESPHome rendering (`schedule_show()`) to maintain this alternating effect, stopping automatically once absolute targets (0% or 100%) are reached, preventing CPU exhaustion.

---

## 📖 Examples

### 1. Basic Kitchen Setup (High Protection)
In this example, `color_interlock` is active to prevent the user from turning on RGB and White at maximum brightness simultaneously, which could overload the power supply.

```yaml
light:
  - platform: ws2805
    name: "Kitchen Under-Cabinet Lighting"
    pin: GPIO16
    num_leds: 150
    color_interlock: true
    cold_white_color_temperature: 153 mireds # 6500K
    warm_white_color_temperature: 370 mireds # 2700K
    effects:
      - addressable_rainbow:
```

### 2. High-Fidelity Living Room Setup (Dithering & Smooth Fades)
This configuration focuses on maximum visual quality. `dithering` is enabled for buttery-smooth low-light transitions, and the `cct_transition_speed` is bumped up to a slow, cinematic 5 seconds.

```yaml
light:
  - platform: ws2805
    name: "Living Room Cove Lighting"
    pin: GPIO4
    num_leds: 300
    cct_transition_speed: 5s
    dithering: true
    color_interlock: false # Allows blending RGB with White
    max_refresh_rate: 10ms # Slightly relaxed refresh rate for long strips
    effects:
      - addressable_scan:
      - addressable_twinkle:
```

### 3. Multi-Zone Setup (ESP32-S3)
Using multiple WS2805 strips on the same ESP32. Ensure you stay within your chip's RMT channel limits. Includes backup data line.

```yaml
esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf

light:
  - platform: ws2805
    name: "Bookshelf Left"
    pin: GPIO4
    fdin_pin: GPIO5 # Backup data line
    num_leds: 50
    dithering: true

  - platform: ws2805
    name: "Bookshelf Right"
    pin: GPIO6
    num_leds: 50
    dithering: true
```

---

## ☕ Support

If you like this ESPHome component and want to support my work, I'd be absolutely thrilled about a virtual coffee!

<a href="https://www.paypal.me/babeinlovexd">
  <img src="https://img.shields.io/badge/Donate-PayPal-blue.svg?style=for-the-badge&logo=paypal" alt="Donate with PayPal">
</a>

---

## 👨‍💻 Entwickelt von

| [<img src="https://avatars.githubusercontent.com/u/43302033?v=4" width="100"><br><sub>**Christopher**</sub>](https://github.com/babeinlovexd) |
| :---: |

---
