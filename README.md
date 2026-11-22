<p align="center">
  <a href="https://github.com/babeinlovexd"><img src="https://raw.githubusercontent.com/babeinlovexd/babeinlovexd/main/bxd-github-header.svg" width="520" alt="BxD / BabeinlovexD"></a>
</p>

# ESPHome WS2805

[Deutsch](README_de.md) · [Downloads](https://github.com/babeinlovexd/ESPHOME-WS2805/releases)

External ESPHome component for **WS2805 five-channel RGBCCT LED strips**. RGB effects are addressable; warm and cold white are controlled globally across the strip.

## Features

- ESPHome addressable RGB effects with global white-channel/CCT control.
- Manual white-channel mixing during effects, with Home Assistant state synchronization.
- Configurable channel order, white transitions, temporal dithering and color interlock.
- Optional sparse dimming for lower average brightness with static lighting.
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
    output_id: ws2805_output
    name: "WS2805"
    pin: GPIO4
    num_leds: 60
    channel_order: GRBWWCW
    color_interlock: false
    constant_brightness: false
    cct_transition_speed: 3s
    dithering: false
    rmt_preencode: false
    sparse_dimming: false
    effects:
      - addressable_rainbow:
      - addressable_scan:
```

## ESPHome compatibility

Component versions **0.6.8 and newer** support the color-accessor API used by **ESPHome 2026.11.0** and retain compatibility with **ESPHome 2026.10.2**.

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
| `rmt_preencode` | `false`; set `true` to pre-expand transmission symbols |
| `fdin_pin` | Optional backup data output for strips with a redundant input |
| `sparse_dimming` | `false`; enable spatial dimming for static lighting |
| `sparse_max_step` | `8`, allowed range 2–32; largest spacing between active LEDs |

In the current schema, `pin` remains required even when `din_pin` is supplied; `din_pin` overrides the selected data GPIO. Use `pin` in ordinary configurations. A backup data output is not a replacement for correct power and ground wiring.

### Optional pre-encoding

Existing configurations keep their RAM-efficient transmission path on ESP-IDF 5.3+. If a classic ESP32 flickers or shows incorrect first LEDs, try:

```yaml
    rmt_preencode: true
```

The default is `false`. Setting `true` prepares all RMT symbols before transmission, reducing work in the refill interrupt. This can reduce underruns, but does not guarantee glitch-free operation under all loads.

**Memory:** the three data buffers together require approximately `11 × num_leds` bytes with `false`, versus `166 × num_leds + 4` bytes with `true` (11 kB versus 166 kB at 1,000 LEDs). The larger buffer requires contiguous internal RAM, including on boards with PSRAM. Use the default for large strips or multiple outputs unless sufficient internal RAM is available. ESP-IDF below 5.3 always uses pre-expanded symbols, as before; the option does not change that older path.

### Optional sparse dimming

Sparse dimming lowers the strip's average brightness by leaving some LEDs off at low settings. It is **disabled by default** and applies only while no addressable effect is active. To enable it, configure the light entry, for example:

```yaml
light:
  - platform: ws2805
    name: "WS2805 Night Light"
    pin: GPIO4
    num_leds: 60
    sparse_dimming: true
    sparse_max_step: 8
```

`sparse_max_step` sets the largest spacing between active LEDs. With `8`, the lowest settings light approximately every eighth LED. As brightness rises, more LEDs participate before the output moves into ordinary PWM dimming. The pattern is static and centered on the strip; gaps can remain visible. Full brightness is preserved, and the brightness control still uses 0–100%.

At the bottom of the range the lit pixels use PWM `1`, the minimum nonzero code.

This is spatial dimming, so the number of LEDs limits the available steps. Very short strips offer fewer distinct levels, and there is a minimum nonzero brightness rather than unlimited dimming. RGB and both white channels use the selected-pixel pattern; warm and cold white remain global controls rather than independently configurable white segments. Sparse dimming does not allocate another large frame buffer and is independent of `rmt_preencode`.

White-channel transitions and optional temporal dithering run after brightness remapping. When switching off, the last sparse pattern remains until the white-channel fade finishes.

### Multiple strips

The component's validator permits **8 instances on ESP32**, **4 on ESP32-S2/S3**, and **2 on ESP32-C3/C6**. Other users of RMT and the chosen chip/framework can further constrain the usable resources. Each strip needs its own data GPIO. There is no guarantee of eight independent strips on an S3.

### Timing & diagnostics

Advanced timing defaults are `bit0_high_ns: 300`, `bit0_low_ns: 975`, `bit1_high_ns: 800`, `bit1_low_ns: 800` and `reset_pulse_us: 300`. The RMT clock rounds durations to hardware ticks: with these defaults, a zero bit takes 1,275 ns at 80 or 40 MHz, or 1,300 ns on the legacy 10 MHz path. A one bit takes 1,600 ns.

The [manufacturer's timing specification](https://dl.lightstream.pro/PDF/IC/WS2805_EN.pdf#page=3) lists 220–380 ns for the zero-bit high pulse and 580–1,000 ns for the other three pulses, with a total bit duration of at least 1,250 ns. Change timing parameters only when your strip requires it, keeping the pulse widths and total duration within specification.

Bit timings that cannot be represented at the selected RMT clock are rejected with an error message. Very long reset pulses are capped by the hardware limit, with the adjustment reported in the log.

> **Testing note:** OTA-flashing a WS2805 strip can leave the first chip(s) latched in their old state. After changing timing or firmware, **hard power-cycle the strip** before judging the result.

Template sensors can call `get_num_leds()`, `get_frame_bytes()`, `get_rmt_resolution_hz()`, `get_last_frame_ms()`, `get_max_refresh_hz()` and `get_tx_error_count()` through the separate `output_id` (for example, `ws2805_output` above). The light `id` identifies the light state rather than the driver output. See the [diagnostic example](docu.md#diagnostics). These values do not certify signal quality or hardware compatibility.

## Examples & troubleshooting

- [example.yaml](example.yaml) provides a complete example device configuration; [docu.md](docu.md) explains white transitions and dithering.
- **Wrong colors:** Check `channel_order` against your strip.
- **White channels behave unexpectedly:** Check color interlock and the manually selected white levels during effects.
- **Gaps at very low brightness:** Expected with `sparse_dimming: true`. Reduce `sparse_max_step` for a denser pattern or set `sparse_dimming: false` for ordinary full-strip dimming.
- **Uneven light at the lowest sparse settings:** Confirm the bit timing is within specification (see [Timing & diagnostics](#timing--diagnostics)) — an out-of-spec "1"-bit low time makes PWM 1 look patchy.
- **Flashes or RMT errors:** Check power supply, common ground, signal levels, strip length, RMT allocation and framework logs.
- **Flicker or wrong first LEDs:** On classic ESP32, try `rmt_preencode: true` if sufficient internal RAM is available.
- **Support details:** Include the component and ESPHome versions, board, framework, relevant logs and a minimal YAML example when reporting a problem.

## License

[CC BY-NC-SA 4.0](LICENSE). The existing project license applies; third-party components retain their own licenses.

## Contributing

Questions, feedback and contributions are welcome. Describe your setup and the steps needed to reproduce a problem. Please discuss larger changes before implementation.

Support this hobby project: [PayPal](https://www.paypal.me/babeinlovexd).

[BxD / BabeinlovexD](https://github.com/babeinlovexd) · [Website](https://babeinlovexd.de/)
