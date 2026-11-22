# WS2805 configuration and usage notes

[Configuration overview](README.md) · [Deutsche Übersicht](README_de.md) · [Downloads](https://github.com/babeinlovexd/ESPHOME-WS2805/releases)

The `ws2805` light platform controls five-channel RGBCCT strips with ESPHome. Its own ESP32 RMT driver sends five bytes per pixel. ESPHome effects receive an addressable RGB view; warm and cold white are controlled globally across the strip.

## Hardware and outputs

Use an ESP32-family target with an RMT peripheral. The examples below use ESP-IDF; choose a board and pin assignment appropriate to your device. The component's validator permits up to eight instances on ESP32, four on ESP32-S2/S3, and two on ESP32-C3/C6. Available RMT memory, other RMT users and optional backup outputs can reduce the usable number of strips.

`pin` is required. If `din_pin` is also supplied, it overrides the selected data GPIO; it does not remove the requirement for `pin`.

`fdin_pin` is an optional second output carrying the same frame for a strip's redundant data input. Hardware synchronization is used when the chip and framework support it. On a classic ESP32, or when a synchronization manager is unavailable, the outputs are started consecutively and are not guaranteed to be bit-aligned. Check the startup log to see which mode is active. Recovery through the strip's backup input depends on its hardware and wiring.

## Basic light configuration

This is a light section to add to an ESPHome device configuration. Configure the ESP32 board, external component source, network and API as shown in the [installation overview](README.md#installation).

```yaml
light:
  - platform: ws2805
    id: ws2805_light
    output_id: ws2805_output
    name: "WS2805 LED Strip"
    pin: GPIO4
    num_leds: 60
    channel_order: GRBWWCW
    color_interlock: false
    constant_brightness: false
    cold_white_color_temperature: 153 mireds
    warm_white_color_temperature: 500 mireds
    cct_transition_speed: 3s
    dithering: false
    isr_priority: 3
    rmt_preencode: false
    sparse_dimming: false
```

- `num_leds` is the positive number of WS2805 pixels, each with five channels.
- `channel_order` must match the strip. Supported orders are `RGBWWCW`, `RGBCWWW`, `GRBWWCW`, `GRBCWWW`, `BRGWWCW` and `BRGCWWW`; the default is `GRBWWCW`.
- `color_interlock: true` makes RGB and white output mutually exclusive. It does not measure current, limit power or provide thermal protection.
- `constant_brightness` controls the balancing of the two white channels through ESPHome's RGBWW conversion. With `false`, each white share is scaled independently. With `true`, the combined white output is balanced according to the stronger white share. This setting does not throttle RGB or enforce a power limit.
- The cold/warm color-temperature settings describe the strip's white endpoints. Defaults are `153 mireds` (about 6500 K) and `500 mireds` (about 2000 K).
- `cct_transition_speed` is the duration of the component's white-channel fade; the default is `3s`. A value of `0s` applies white targets immediately.
- `isr_priority` selects interrupt priority 1–3, default `3`. It affects interrupt scheduling; it cannot guarantee that every transmission disturbance is avoided.

For the complete current option list, use the [configuration table](README.md#configuration).

## Optional transmission and dimming modes

`rmt_preencode` defaults to `false`, preserving the RAM-efficient path on ESP-IDF 5.3 and newer. Setting it to `true` prepares the RMT symbols before transmission and can reduce refill work on classic ESP32 devices. It requires substantially more contiguous internal RAM. Earlier frameworks already use pre-expanded symbols. See [memory requirements](README.md#optional-pre-encoding) before enabling it for long strips or multiple outputs.

`sparse_dimming` also defaults to `false`. When enabled without an active addressable effect, it lowers average brightness by leaving some pixels off in a static, centered pattern. `sparse_max_step` defaults to `8` and accepts 2–32. Short strips provide fewer distinct density levels, and gaps can remain visible.

`sparse_floor_pwm` defaults to `2` and accepts 1–255. It is the reference PWM level for a channel selected at full intensity in the sparse range. Smaller color and white shares remain proportional, so individual channels can fall below that reference or round to zero. Setting it to `1` retains the minimum used by component version 0.6.10. Higher values brighten the low end and may reduce uneven illumination on some strips; results depend on the hardware. The setting has no effect while sparse dimming is disabled.

Sparse dimming does not allocate another large frame buffer. Brightness remapping happens before the white-channel fade and optional temporal dithering. When switching off, the previous sparse pattern remains until the white fade finishes. See the [sparse dimming example](README.md#optional-sparse-dimming).

## White-channel temporal dithering

The component calculates white brightness with fractional values, while the strip receives 8-bit values from 0 to 255. With `dithering: false`, those values are rounded for transmission.

With `dithering: true`, rounding error is carried into the next frame **while a white-channel fade is active**. Starting with no carried error, two consecutive calculated values of 12.5 produce 13 and then 12 rather than always rounding in the same direction. During a real fade, the calculated value also changes over time.

The component requests another render while the white fade is active. Once the fade ends, including at intermediate brightness settings, it returns to ordinary rounding rather than continuously dithering a stationary fractional target. Frame rate depends on strip length, transmission time and ESPHome scheduling. Dithering can change the appearance of low-level fades; it does not guarantee invisible stepping or flicker on every strip. Addressable RGB effects use their own rendering path.

## Timing overrides

The defaults are `bit0_high_ns: 300`, `bit0_low_ns: 975`, `bit1_high_ns: 800`, `bit1_low_ns: 800` and `reset_pulse_us: 300`. The first four settings use nanoseconds; reset uses microseconds.

RMT durations are rounded to hardware ticks. With these defaults, a zero bit takes 1,275 ns at 80 or 40 MHz, or 1,300 ns on the legacy 10 MHz path. A one bit takes 1,600 ns. The [manufacturer's timing specification](https://dl.lightstream.pro/PDF/IC/WS2805_EN.pdf#page=3) requires a total bit duration of at least 1,250 ns as well as valid high and low pulse widths.

Change timings only when your strip requires it. Bit timings that cannot be represented at the selected RMT clock are rejected with an error message. Very long reset pulses are capped by the hardware limit, with the adjustment reported in the log.

## Diagnostics

The diagnostic getters belong to the light **output**, identified by `output_id`. The ordinary light `id` identifies the ESPHome light state instead.

- `get_num_leds()`: configured number of pixels.
- `get_frame_bytes()`: five data bytes per pixel, excluding the expanded RMT symbol buffer.
- `get_rmt_resolution_hz()`: RMT clock frequency in Hz.
- `get_last_frame_ms()`: measured time for the last frame's encoding, transmission and completion wait.
- `get_max_refresh_hz()`: an estimate derived from the last measured frame time; it is not a guaranteed sustained frame rate.
- `get_tx_error_count()`: cumulative transmit API errors and timeouts recorded by the driver. It is not a general underrun or signal-quality detector; zero does not prove that the strip received every frame correctly.

For example, expose the error counter with a separate `output_id`:

```yaml
light:
  - platform: ws2805
    id: ws2805_light
    output_id: ws2805_output
    name: "WS2805 Diagnostic Strip"
    pin: GPIO4
    num_leds: 60

sensor:
  - platform: template
    name: "WS2805 Transmit Errors"
    accuracy_decimals: 0
    update_interval: 10s
    lambda: |-
      return id(ws2805_output).get_tx_error_count();
```

## More examples

Each example is a separate device-configuration fragment. Adapt GPIOs, strip length and white endpoints to your setup.

### Kitchen lighting with mutually exclusive RGB and white

This configuration enables `color_interlock` to select RGB or white output. Power-supply capacity, wiring and thermal design still need to match the hardware.

```yaml
light:
  - platform: ws2805
    name: "Kitchen Under-Cabinet Lighting"
    pin: GPIO16
    num_leds: 150
    color_interlock: true
    cold_white_color_temperature: 153 mireds
    warm_white_color_temperature: 370 mireds
    effects:
      - addressable_rainbow:
```

### Living-room lighting with longer white fades

This example selects a five-second white-channel transition and enables temporal dithering during that transition. With `color_interlock: false`, manually selected white output can be mixed with RGB.

```yaml
light:
  - platform: ws2805
    name: "Living Room Cove Lighting"
    pin: GPIO4
    num_leds: 300
    cct_transition_speed: 5s
    dithering: true
    color_interlock: false
    effects:
      - addressable_scan:
      - addressable_twinkle:
```

### Two strips on an ESP32-S3, with one backup output

The first strip uses an additional RMT output for its redundant input. The usable number of strips also depends on the other RMT resources required by your device.

```yaml
esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf

light:
  - platform: ws2805
    name: "Bookshelf Left"
    pin: GPIO4
    fdin_pin: GPIO5
    num_leds: 50
    dithering: true

  - platform: ws2805
    name: "Bookshelf Right"
    pin: GPIO6
    num_leds: 50
    dithering: true
```

## License and support

[CC BY-NC-SA 4.0](LICENSE). The existing project license applies; third-party components retain their own licenses.

Support this hobby project: [PayPal](https://www.paypal.me/babeinlovexd).

[BxD / BabeinlovexD](https://github.com/babeinlovexd) · [Website](https://babeinlovexd.de/)
