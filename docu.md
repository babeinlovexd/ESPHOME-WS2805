# WS2805 configuration reference

[Setup guide](README.md) · [Deutsch](README_de.md)

The `ws2805` platform drives five-channel RGBCCT strips over the ESP32 RMT peripheral. Each addressable pixel receives 40 bits: red, green, blue, warm white and cold white. ESPHome effects can control RGB per pixel. The two white channels are set for the entire strip.

## Hardware and configuration

Set `num_leds` to the number of addressable pixels. Some strips group several physical LEDs into one pixel. Use a data GPIO supported by your board and connect the controller and strip grounds.

Use `pin` or `din_pin` to select the primary data GPIO. If both are set, `din_pin` takes precedence and the unused pin is available to another component. Use `fdin_pin` if the strip has a redundant data input. DIN and FDIN each reserve an RMT output. Where the hardware and driver provide a synchronization manager, both outputs start together. The number of available outputs depends on the ESP32 variant, allocations and other RMT peripherals. If FDIN cannot start, the strip continues to receive data on DIN.

See the [configuration table](README.md#configuration) for option names and defaults.

## White control and transitions

- **Color interlock** switches between the RGB and white channels when `color_interlock: true`. Size the power supply and wiring for the actual load.
- **Constant brightness** changes how ESPHome mixes cold and warm white. When enabled, the white channels share a combined level based on the larger of their requested levels. When disabled, each white channel follows its requested level scaled by the overall brightness.
- **Color temperature endpoints** describe the cold and warm LEDs on your strip. Use a higher Kelvin value for cold white than for warm white.
- **Transition time** follows ESPHome's `default_transition_length` (default `1s`) for RGB and both white channels. Home Assistant actions can override it with `transition` in seconds. An explicit legacy `cct_transition_speed` supplies the default when `default_transition_length` is omitted.
- **Dithering** is disabled by default. During a white-channel fade, `dithering: true` carries the rounding error into the next frame. For example, a value of 12.5 can alternate between codes 13 and 12. Output at the finished transition target uses ordinary rounding. The visible result depends on the strip and frame rate.

```yaml
light:
  - platform: ws2805
    name: "Cove Lighting"
    pin: GPIO4
    num_leds: 60
    cold_white_color_temperature: 153 mireds
    warm_white_color_temperature: 370 mireds
    default_transition_length: 5s
    dithering: true
    effects:
      - addressable_scan:
```

## Transmission tuning

The [transmission options](README.md#transmission-tuning) adjust the work and memory available for sending data. Test them separately on your hardware if the strip flashes or displays incorrect colors under load. The standard settings are `rmt_preencode: false` and `rmt_no_dma_blocks: 2`. Pre-encoding trades internal RAM for less work during the refill. Additional blocks consume more of the shared non-DMA RMT memory. Leave room for other RMT devices and any FDIN output. If ESPHome reports competing RMT allocations, check `rmt_no_dma_blocks` on WS2805 and the RMT allocation settings of the other component.

## Sparse dimming

Set `sparse_dimming: true` in a `ws2805` light entry to dim static light below the level produced by lighting the entire strip at the lowest PWM code. At very low brightness, selected pixels light up and the remaining pixels stay dark. With the maximum spacing of eight pixels, three of 24 addressable pixels can light up at the lowest nonzero setting for a fully requested channel. Increasing brightness reduces the spacing and eventually increases the PWM level across the whole strip.

Sparse dimming uses raw PWM values for static colors. The low-level reference is PWM 1 for a fully requested channel before applying the RGB channel factors from `color_correct`. Small channel values can round to zero and change the appearance of a color mix. The illuminated positions can shift as the spacing changes. Sparse dimming is bypassed while a light effect is active. Each transmission still contains data for every pixel.

## Advanced timing

The RMT clock rounds each pulse to the nearest hardware tick. Override individual values in the light entry if required by your strip.

| Setting | Default | Meaning |
| :--- | ---: | :--- |
| `bit0_high_ns` | 300 ns | High pulse for a zero bit |
| `bit0_low_ns` | 975 ns | Low pulse for a zero bit |
| `bit1_high_ns` | 800 ns | High pulse for a one bit |
| `bit1_low_ns` | 800 ns | Low pulse for a one bit |
| `reset_pulse_us` | 300 µs | Low pulse between frames |

At their nominal values, a zero bit takes 1,275 ns and a one bit takes 1,600 ns. See the [WS2805 datasheet](https://dl.lightstream.pro/PDF/IC/WS2805_EN.pdf#page=3) for signal limits.

If the strip behaves unexpectedly after an OTA update, switch off power to the controller and the entire strip, including separate power supplies. Power them up again before retesting.

## Diagnostics

Use the light entry's `output_id` when calling component-specific diagnostics from a template sensor.

| Method | Value |
| :--- | :--- |
| `get_num_leds()` | Number of addressable pixels |
| `get_frame_bytes()` | Payload bytes per frame, five per pixel |
| `get_rmt_resolution_hz()` | RMT timing resolution, in hertz |
| `get_last_frame_ms()` | Time spent encoding, sending and waiting for the last measured frame, in milliseconds |
| `get_max_refresh_hz()` | Theoretical refresh rate based on the last measured frame time |
| `get_tx_error_count()` | Cumulative transmission errors and timeouts tracked by the driver |

```yaml
light:
  - platform: ws2805
    name: "WS2805 Strip"
    pin: GPIO4
    num_leds: 60
    output_id: ws2805_output

sensor:
  - platform: template
    name: "WS2805 Frame Time"
    unit_of_measurement: "ms"
    accuracy_decimals: 2
    update_interval: 10s
    lambda: |-
      return id(ws2805_output).get_last_frame_ms();
```
