# ESPHome WS2805 External Component

*[Deutsche Version unten](#deutsche-version)*

This is an external component for ESPHome that provides support for **WS2805** 5-channel (RGB + Warm White + Cold White) LED strips.

ESPHome's built-in `AddressableLight` primarily maps to a maximum of 4 channels (RGBW). Since WS2805 requires 5 channels (40 bits per pixel) for RGBCCT support, this component operates as an `AddressableLight` for RGB effects, while maintaining global control over the W1 and W2 channels.

This maps perfectly to the Home Assistant UI, providing correct Addressable RGB effects and global CCT (Color Temperature) control without jumping sliders.

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
    pin: GPIO19 # The GPIO pin your data line is connected to
    num_leds: 100 # Total number of LEDs on the strip
    effects:
      - pulse:
      - random:
      - addressable_rainbow:
```

## Features
- **Addressable RGB Effects:** Since the component inherits from `AddressableLight`, you can add and use all Addressable light effects such as `addressable_rainbow`, `addressable_scan`, etc.
- **Global CCT Control:** The Warm White and Cold White channels are globally set across the entire strip according to your CCT sliders in Home Assistant. This is exactly how tools like WLED manage RGBCCT setups.
- **Hardware Agnostic (NeoPixelBus):** Uses the highly optimized Makuna `NeoPixelBus` library under the hood (`NeoWs2805Method`).
- **Brightness Scaling:** Proper mapping and scaling of the 5 channels relative to overall brightness.

---

# Deutsche Version

Dies ist eine externe Komponente (External Component) für ESPHome, die Unterstützung für **WS2805** 5-Kanal (RGB + Warmweiß + Kaltweiß) LED-Streifen bietet.

Da ESPHome standardmäßig bei `AddressableLight` maximal 4 Kanäle (RGBW) unterstützt, der WS2805 aber 5 Kanäle (40 Bits pro LED) für RGBCCT nutzt, verhält sich diese Komponente bei RGB-Effekten wie ein normales `AddressableLight`. Gleichzeitig behält sie jedoch die globale Kontrolle über die Kanäle W1 und W2.

Dadurch wird der Home Assistant Farbwähler (UI) perfekt unterstützt: Es gibt keine "springenden" Regler mehr bei der Farbtemperatur (CCT), und adressierbare RGB-Effekte funktionieren einwandfrei.

## Verwendung in ESPHome

Um diese Komponente zu nutzen, kannst du sie direkt von GitHub über den `external_components` Block in deine Konfiguration einbinden.

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/babeinlovexd/esph-ws2805
      ref: main
    components: [ ws2805 ]

light:
  - platform: ws2805
    name: "Mein WS2805 Streifen"
    pin: GPIO19 # Der GPIO-Pin, an den deine Datenleitung angeschlossen ist
    num_leds: 100 # Gesamtzahl der LEDs auf dem Streifen
    effects:
      - pulse:
      - random:
      - addressable_rainbow:
```

## Funktionen
- **Adressierbare RGB-Effekte:** Da die Komponente von `AddressableLight` erbt, kannst du alle adressierbaren Lichteffekte wie `addressable_rainbow`, `addressable_scan` usw. nutzen.
- **Globale CCT-Steuerung:** Die Warmweiß- und Kaltweiß-Kanäle werden für den gesamten Streifen anhand der CCT-Regler in Home Assistant global gesteuert (Genau so, wie WLED RGBCCT-Setups verwaltet).
- **Hardware-Unabhängig (NeoPixelBus):** Verwendet im Hintergrund die stark optimierte Makuna `NeoPixelBus` Bibliothek (`NeoWs2805Method`).
- **Helligkeitsskalierung:** Korrektes Mapping und Skalierung der 5 Kanäle relativ zur Gesamthelligkeit.
