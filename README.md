# ESPHome WS2805 External Component

This is an external component for ESPHome that provides support for **WS2805** 5-channel (RGB + Warm White + Cold White) LED strips.

ESPHome's built-in `AddressableLight` primarily maps to a maximum of 4 channels (RGBW). Since WS2805 requires 5 channels (40 bits per pixel) for RGBCCT support, this component operates as an `AddressableLight` for RGB effects, while maintaining global control over the W1 and W2 channels.

This maps perfectly to the Home Assistant UI, providing correct Addressable RGB effects and global CCT (Color Temperature) control without jumping sliders.

## Usage in ESPHome

To use this component, you can include it directly from GitHub using the `external_components` block.

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/YOUR_USERNAME/YOUR_REPO_NAME # Update with the actual repository URL
      ref: main
    components: [ ws2805 ]

light:
  - platform: ws2805
    name: "My WS2805 Strip"
    pin: GPIO4 # The GPIO pin your data line is connected to
    num_leds: 100 # Total number of LEDs on the strip
    effects:
      - pulse:
      - random:
      - addressable_rainbow:
```

*(Note: Replace `YOUR_USERNAME/YOUR_REPO_NAME` with the URL of the repository where this code is hosted.)*

## Features
- **Addressable RGB Effects:** Since the component inherits from `AddressableLight`, you can add and use all Addressable light effects such as `addressable_rainbow`, `addressable_scan`, etc.
- **Global CCT Control:** The Warm White and Cold White channels are globally set across the entire strip according to your CCT sliders in Home Assistant. This is exactly how tools like WLED manage RGBCCT setups.
- **Hardware Agnostic (NeoPixelBus):** Uses the highly optimized Makuna `NeoPixelBus` library under the hood (`NeoWs2805Method`).
- **Brightness Scaling:** Proper mapping and scaling of the 5 channels relative to overall brightness.
