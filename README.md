<p align="center">
  <a href="https://github.com/babeinlovexd"><img src="https://raw.githubusercontent.com/babeinlovexd/babeinlovexd/main/bxd-github-header.svg" width="520" alt="BxD / BabeinlovexD"></a>
</p>

# ESPHome WS2805

[Deutsch](README_de.md) · [Configuration reference](docu.md) · [Downloads](https://github.com/babeinlovexd/ESPHOME-WS2805/releases)

External ESPHome component for **WS2805 five-channel RGBCCT LED strips**. Each pixel has individual RGB control. Warm white and cold white are set across the strip.

## Features

- ESPHome RGB effects and Home Assistant control of brightness, color and white temperature.
- Configurable channel order, white-channel mixing and temporal dithering.
- ESP32 RMT output with optional transmission tuning and sparse dimming.

## Installation

Add this to your ESPHome device configuration. Select the board and data GPIO for your device, and configure its name, network and API as needed.

```yaml
esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf

external_components:
  - source:
      type: git
      url: https://github.com/babeinlovexd/ESPHOME-WS2805
      ref: main
    components: [ws2805]

light:
  - platform: ws2805
    id: ws2805_light
    output_id: ws2805_output
    name: "WS2805"
    pin: GPIO4
    num_leds: 60
    channel_order: GRBWWCW
    color_interlock: false
    constant_brightness: false
    default_transition_length: 1s
    dithering: false
    effects:
      - addressable_rainbow:
      - addressable_scan:
```

## ESPHome compatibility

The component supports **ESPHome 2026.10.2** and **2026.11.0**. The examples use ESP-IDF.

## Home Assistant

Enable `api:` in the device configuration and add the device through the [ESPHome integration](https://www.home-assistant.io/integrations/esphome/). The strip is available as a light in dashboards, scenes and automations.

- Set `default_transition_length` for everyday fades. In a Home Assistant action, set `transition` in seconds to override it for that action, including `0` for an immediate change.
- With `color_interlock: false`, the light exposes RGBWW and can mix cold white and warm white with static colors or RGB effects.
- With `color_interlock: true`, the light offers separate RGB and color-temperature modes. Set the warm and cold limits to match your strip.
- Use `light.addressable_set` for individual RGB pixels in ESPHome automations. Use [light partitions](https://esphome.io/components/light/partition/) to expose RGB segments as separate lights in Home Assistant. For segment control, give the source strip an ID and set `internal: true` and `sparse_dimming: false`. Home Assistant may show white controls inherited from the source on each segment. To set either white channel, send a command to the whole strip.

The optional ESPHome `power_supply` setting can switch the strip's supply. ESPHome's `power_estimation` option is unavailable for WS2805.

## Configuration

| Option | Default / requirement |
| :--- | :--- |
| `pin` | Data GPIO. Set `pin` or `din_pin`. |
| `num_leds` | Required number of addressable pixels. Count addressable groups on strips with several LEDs per pixel. |
| `channel_order` | `GRBWWCW`; also `RGBWWCW`, `RGBCWWW`, `GRBCWWW`, `BRGWWCW`, `BRGCWWW` |
| `color_interlock` | `false` |
| `constant_brightness` | `false` |
| `cold_white_color_temperature` | `153 mireds` (about 6,536 K) |
| `warm_white_color_temperature` | `500 mireds` (2,000 K). Use a lower Kelvin value than for cold white. |
| `default_transition_length` | ESPHome default `1s` for color, brightness and white transitions. |
| `cct_transition_speed` | Legacy option. An explicit value sets the default transition length when `default_transition_length` is omitted. |
| `dithering` | `false` |
| `isr_priority` | `3`. Use `0` to let the RMT driver choose, or set `1`–`3`. |
| `din_pin` | Primary data GPIO when set. Takes precedence over `pin`. |
| `fdin_pin` | Optional second data output for strips with a redundant input |
| `rmt_preencode` | `false`. Prepare the RMT symbols before transmission on ESP-IDF 5.3 and later. |
| `rmt_no_dma_blocks` | `2`. Choose `2` or `4` memory blocks per output that runs without DMA on ESP-IDF 5 or later. |
| `sparse_dimming` | `false`. Enable spatial dimming at very low brightness. |

DIN and FDIN each use an RMT output. Use GPIOs that support non-inverted, push-pull output on your board. The number of outputs depends on the ESP32 variant, the memory reserved per output and other RMT components on the device.

On ESP32 variants with RMT DMA, the first WS2805 DIN output uses DMA. FDIN and any additional WS2805 DIN outputs use RMT hardware memory. If the optional FDIN output cannot be initialized, the component logs the failure and continues through DIN.

With the default allocations on ESP-IDF 5 or later and no other RMT devices, an ESP32-C3 or ESP32-C6 has room for a single WS2805 DIN output. An ESP32-S3 can use one DMA DIN output and one additional non-DMA output. The additional output can serve another strip or the backup input of the first strip.

### Transmission tuning

Both options belong inside the `ws2805` light entry and can be tested independently.

- **`rmt_preencode: true`** prepares the full frame before sending it, reducing the work done when the RMT peripheral needs more symbols. On ESP-IDF 5.3 and later, the three data buffers use roughly 11 kB for 1,000 pixels with the default setting or 166 kB with pre-encoding. The transmission buffer needs a contiguous allocation in internal RAM. Older ESP-IDF paths already prepare symbols ahead of transmission.
- **`rmt_no_dma_blocks: 4`** gives each output running without DMA more symbols between refill interrupts. Each such DIN or FDIN output reserves its own blocks. Larger reservations leave less space for other RMT outputs.

Try one setting at a time if you see short flashes or wrong colors under load. For setups with other RMT devices, `isr_priority: 0` lets the RMT driver choose or inherit the shared interrupt priority. The configured bit timings determine the signal speed.

### Sparse dimming

Set `sparse_dimming: true` for a static night light. This setting uses raw PWM values for static colors, so their brightness can look different from normal gamma-corrected output. At the bottom of the range, a subset of the pixels lights up at a reference PWM level of 1 for a fully requested channel. Dimmer channel values in a color mix may round to zero.

The pattern uses a maximum spacing of eight pixels. The number and positions of illuminated pixels can change as you dim. A diffuser can make the pattern less visible. The controller still sends data for every pixel. Sparse dimming is bypassed while a light effect is active.

### Timing & diagnostics

The defaults are 300 ns high and 975 ns low for a zero bit, 800 ns high and 800 ns low for a one bit, and a reset of 300 µs. The zero bit nominally totals 1,275 ns. Actual pulse widths are rounded to the RMT clock's resolution. For strips that require different timings, use the [timing overrides](docu.md#advanced-timing).

Template sensors can read frame size, RMT clock, frame time and transmission errors via `output_id`. See the [diagnostic example](docu.md#diagnostics).

## Examples & troubleshooting

- [example.yaml](example.yaml) provides a complete example device configuration.
- **Wrong colors:** Check `channel_order` against your strip.
- **White channels behave unexpectedly:** Check color interlock and the manually selected white levels during effects.
- **Flashes or RMT errors:** Check power supply, common ground, signal levels, strip length and the device's RMT logs.
- **Unexpected output after an OTA update:** If symptoms persist, switch off power to the controller and the strip, including any separate strip supply. Power them up again before retesting.

## License

[CC BY-NC-SA 4.0](LICENSE). The existing project license applies; third-party components retain their own licenses.

Support this hobby project: [PayPal](https://www.paypal.me/babeinlovexd).

[BxD / BabeinlovexD](https://github.com/babeinlovexd) · [Website](https://babeinlovexd.de/)
