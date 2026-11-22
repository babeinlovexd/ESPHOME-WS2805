# ESPHome WS2805 v0.6.10

- Add optional sparse dimming from PR #61 (disabled by default): low brightness lights a centered subset of pixels.
- Fix the brightness discontinuity at the sparse/dense boundary and retain floating-point channel values until final quantization.
- Remap white targets before their hardware fade/dithering; preserve the sparse mask when fading to off.
- Keep addressable effects and existing configurations on their previous path. No additional large frame buffer is allocated.
- Test fractional brightness, all allowed spacing values, all six channel orders and white fades with native regression tests.

---

# ESPHome WS2805 v0.6.9

- Add optional `rmt_preencode: true` to reduce RMT refill interrupt work and address flicker observed on classic ESP32 devices.
- Default `false` preserves the existing RAM-efficient ESP-IDF 5.3+ path. Older frameworks retain their existing pre-expanded path.
- Document internal RAM requirements for large strips and multiple outputs.
- Fix legacy ESP-IDF 4 transmit buffer pointer types for DIN and FDIN.
- CI compiles both default and opt-in paths.

---

# ESPHome WS2805 v0.6.8

- Kompatibilität mit ESPHome 2026.11.0: neuer ESPColorAccessor für das WS2805-Fünf-Byte-Format. RGB-Effekte greifen ausschließlich auf RGB und ihre separaten Effektdaten zu; WW/CW bleiben global gesteuert.
- Die bisherige get_view_internal-Schnittstelle bleibt für ESPHome 2026.10.2 erhalten.
- Korrektur der im Issue gemeldeten Initialisierungsreihenfolge im Konstruktor.
- CI-Matrix für ESP32 und ESP32-S3 mit 2026.10.2, 2026.11.0 und der neuesten stabilen ESPHome-Version.
- Regressionstests für alle sechs Kanalreihenfolgen, Weißkanal-Isolation, Farbkorrektur und Puffer-Lebenszyklus.

Details: https://github.com/babeinlovexd/ESPHOME-WS2805/issues/59

Die Validierung umfasst Firmware-Kompilierung und native Puffertests. Ein Test an einem realen LED-Streifen wurde bei dieser Korrektur nicht durchgeführt.
