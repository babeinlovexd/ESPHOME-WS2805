<p align="center">
  <a href="https://github.com/babeinlovexd"><img src="https://raw.githubusercontent.com/babeinlovexd/babeinlovexd/main/bxd-github-header.svg" width="520" alt="BxD / BabeinlovexD"></a>
</p>

# ESPHome WS2805

[Deutsch](README_de.md) · [Configuration reference](docu.md) · [Downloads](https://github.com/babeinlovexd/ESPHOME-WS2805/releases)

External ESPHome component for **WS2805 five-channel RGBCCT LED strips**. RGB effects are addressable; warm and cold white are controlled globally across the strip.

## Features

- ESPHome addressable RGB effects with global white-channel/CCT control.
- Manual white-channel mixing during effects, with Home Assistant state synchronization.
- Configurable channel order, white transitions, temporal dithering and color interlock.
- Native ESP32 RMT output and optional settings for tuning transmission.
- Optional sparse dimming for low-level static lighting.

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
    cct_transition_speed: 3s
    dithering: false
    effects:
      - addressable_rainbow:
      - addressable_scan:
```

## ESPHome compatibility

Component version **0.6.8** introduced support for the color-accessor API in **ESPHome 2026.11.0** while retaining the previous interface used by **2026.10.2**. The examples use ESP-IDF. Select a component revision that supports the options you use.

## Configuration

| Option | Default / requirement |
| :--- | :--- |
| `pin` | Required data GPIO |
| `num_leds` | Required number of addressable pixels. Count addressable groups on strips with several LEDs per pixel. |
| `channel_order` | `GRBWWCW`; also `RGBWWCW`, `RGBCWWW`, `GRBCWWW`, `BRGWWCW`, `BRGCWWW` |
| `color_interlock` | `false` |
| `constant_brightness` | `false` |
| `cold_white_color_temperature` | `153 mireds` |
| `warm_white_color_temperature` | `500 mireds` |
| `cct_transition_speed` | `3s` |
| `dithering` | `false` |
| `isr_priority` | `3`, allowed range 1–3 |
| `din_pin` | Optional override for the data GPIO selected by `pin` |
| `fdin_pin` | Optional second data output for strips with a redundant input |
| `rmt_preencode` | `false`. Prepare the RMT symbols before transmission on ESP-IDF 5.3 and later. |
| `rmt_no_dma_blocks` | `2`. Choose `2` or `4` memory blocks per output on non-DMA targets using ESP-IDF 5 or later. |
| `sparse_dimming` | `false`. Enable spatial dimming at very low brightness. |

`pin` is required even when `din_pin` is supplied. The second data line uses its own RMT channel. Available channels depend on the ESP32 variant, the memory reserved per output and other RMT components on the device.

### Transmission tuning

Both options belong inside the `ws2805` light entry and can be tested independently.

- **`rmt_preencode: true`** prepares the full frame before sending it, reducing the work done when the RMT peripheral needs more symbols. On ESP-IDF 5.3 and later, the three data buffers use roughly 11 kB for 1,000 pixels with the default setting or 166 kB with pre-encoding. The transmission buffer needs a contiguous allocation in internal RAM. Older ESP-IDF paths already prepare symbols ahead of transmission.
- **`rmt_no_dma_blocks: 4`** gives the sender more time to refill between interrupts on a target without RMT DMA. The larger allocation reduces the number of outputs and other RMT channels that fit. DIN and FDIN each need their own blocks. DMA targets and the legacy ESP-IDF 4 driver use their existing allocations.

Try one setting at a time if you see short flashes or wrong colors under load. Both settings can reduce the risk of interrupted transmission. The configured bit timings determine the signal speed.

### Sparse dimming

Set `sparse_dimming: true` for a static night light. At the bottom of the brightness range, a subset of the pixels lights up at a reference PWM level of 1 for a fully requested channel. Dimmer channel values in a color mix may round to zero.

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
