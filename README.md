# ESPHome WS2805 External Component

This is an external component for ESPHome that provides support for **WS2805** 5-channel (RGB + Warm White + Cold White) LED strips.

ESPHome's built-in `AddressableLight` primarily maps to a maximum of 4 channels (RGBW). Since WS2805 requires 5 channels (40 bits per pixel) for RGBCCT support, this component exposes the entire strip as a standard `RGB_COLD_WARM_WHITE` light output using the [NeoPixelBus](https://github.com/Makuna/NeoPixelBus) library.

This maps perfectly to the Home Assistant UI, providing correct RGB color selection and CCT (Color Temperature) control without jumping sliders.

## Usage in ESPHome

To use this component, you can include it directly from GitHub using the `external_components` block.

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/babeinlovexd/esph-ws2805 
      ref: main
    components: [ ws2805 ]

light:
  - platform: ws2805
    name: "My WS2805 Strip"
    pin: 4 # The GPIO pin your data line is connected to
    num_leds: 100 # Total number of LEDs on the strip
```


## Features
- **Full RGBCCT control:** Allows setting Red, Green, Blue, Cold White, and Warm White channels simultaneously.
- **Hardware Agnostic (NeoPixelBus):** Uses the highly optimized Makuna `NeoPixelBus` library under the hood (`NeoWs2805Method`).
- **Brightness Scaling:** Proper mapping and scaling of the 5 channels relative to overall brightness.
