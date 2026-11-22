<p align="center">
  <a href="https://github.com/babeinlovexd"><img src="https://raw.githubusercontent.com/babeinlovexd/babeinlovexd/main/bxd-github-header.svg" width="520" alt="BxD / BabeinlovexD"></a>
</p>

# ESPHome WS2805

[Deutsch](README_de.md) · [Issues](https://github.com/babeinlovexd/ESPHOME-WS2805/issues) · [Releases](https://github.com/babeinlovexd/ESPHOME-WS2805/releases)

External ESPHome component for **WS2805 five-channel RGBCCT LED strips**. RGB effects are addressable; warm and cold white are controlled globally across the strip.

## Features

- ESPHome addressable RGB effects with global white-channel/CCT control.
- Manual white-channel mixing during effects, with Home Assistant state synchronization.
- Configurable channel order, white transitions, temporal dithering and color interlock.
- A custom native ESP32 RMT driver; no NeoPixelBus dependency.

## Installation

Add this to your ESPHome device configuration; also configure the device name, network and API as needed:

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

Version **0.6.8** adds the color-accessor API required by **ESPHome 2026.11.0** while retaining the previous interface for **2026.10.2**. Versions up to 0.6.7 use the previous API and do not compile with 2026.11.0.

The build matrix compiles both pinned ESPHome versions and the latest stable version for **ESP32** and **ESP32-S3**. Native regression tests cover five-byte pixel addressing, all six channel orders, RGB correction, effect data and isolation of both global white channels. See [Issue #59](https://github.com/babeinlovexd/ESPHOME-WS2805/issues/59).

## Configuration

| Option | Default / requirement |
| :--- | :--- |
| `pin` | Required data GPIO |
| `num_leds` | Required positive LED count |
| `channel_order` | `GRBWWCW`; also `RGBWWCW`, `RGBCWWW`, `GRBCWWW`, `BRGWWCW`, `BRGCWWW` |
| `color_interlock` | `false` |
| `constant_brightness` | `false` |
| `cold_white_color_temperature` | `153 mireds` |
| `warm_white_color_temperature` | `500 mireds` |
| `cct_transition_speed` | `3s` |
| `dithering` | `false` |
| `isr_priority` | `3`, allowed range 1–3 |
| `rmt_encoder` | `copy`; `simple` opts into on-the-fly (RAM-efficient) encoding |
| `fdin_pin` | Optional backup data output for strips with a redundant input |

In the current schema, `pin` remains required even when `din_pin` is supplied; `din_pin` overrides the selected data GPIO. Use `pin` in ordinary configurations. A backup data output is not a replacement for correct power and ground wiring.

### RMT encoder (speed vs RAM)

`rmt_encoder` selects how LED bytes are turned into RMT symbols:

- `copy` (default) — pre-expands the symbols up front and memcpy's them in the
  RMT refill ISR. This is the fast, robust choice; it avoids underruns on the
  classic ESP32 (no RMT DMA) at the cost of a larger frame buffer.
- `simple` — expands bytes on-the-fly, keeping the frame buffer ~32× smaller
  (useful for very large strips). On targets without RMT DMA it can underrun
  under WiFi/BLE contention and cause flicker or wrong first LEDs, so prefer
  `copy` unless RAM is the constraint.

`simple` requires ESP-IDF 5.3+; older frameworks fall back to `copy`.

### Multiple strips

The component's validator permits **8 instances on ESP32**, **4 on ESP32-S2/S3**, and **2 on ESP32-C3/C6**. Other users of RMT and the chosen chip/framework can further constrain the usable resources. Each strip needs its own data GPIO. There is no guarantee of eight independent strips on an S3.

### Timing & diagnostics

Advanced timing defaults are `bit0_high_ns: 300`, `bit0_low_ns: 800`, `bit1_high_ns: 800`, `bit1_low_ns: 800` and `reset_pulse_us: 300`. Change them only when the strip's timing requirements call for it.

Template sensors can call `get_num_leds()`, `get_frame_bytes()`, `get_rmt_resolution_hz()`, `get_last_frame_ms()`, `get_max_refresh_hz()` and `get_tx_error_count()` on the light ID. These are diagnostics, not a hardware compatibility certificate.

## Examples & troubleshooting

- [example.yaml](example.yaml) provides a complete example device configuration; [docu.md](docu.md) explains white transitions and dithering.
- **Wrong colors:** Check `channel_order` against your strip.
- **White channels behave unexpectedly:** Check color interlock and the manually selected white levels during effects.
- **Flashes or RMT errors:** Check power supply, common ground, signal levels, strip length, RMT allocation and framework build logs.
- **Flicker or wrong first LEDs:** On the classic ESP32 (no RMT DMA) use the default `rmt_encoder: copy`; `simple` can underrun there.
- **Compile errors after updating:** Include ESPHome version, board, framework and a minimal YAML example in the issue.

## License

[CC BY-NC-SA 4.0](LICENSE). The existing project license applies; third-party components retain their own licenses.

---

## Builds and releases

Version: **0.6.8** ([VERSION](VERSION)). [GitHub Actions](.github/workflows/release.yml) checks each push and pull request. A tag matching `v` + `VERSION` publishes a release after all jobs succeed; manual runs provide build artifacts. [Downloads](https://github.com/babeinlovexd/ESPHOME-WS2805/releases).

ESPHome CI uses dummy credentials and does not flash devices. Configure your own secrets and pin assignments before deployment.

## Contributing

Questions, bug reports and contributions are welcome. Include version, setup and reproduction steps in an [Issue](https://github.com/babeinlovexd/ESPHOME-WS2805/issues). Please discuss larger changes before a pull request.

Support this hobby project: [PayPal](https://www.paypal.me/babeinlovexd).

[BxD / BabeinlovexD](https://github.com/babeinlovexd) · [Website](https://babeinlovexd.de/)
