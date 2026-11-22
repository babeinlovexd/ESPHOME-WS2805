# ESPHome WS2805 v0.6.12

- Preserve the `sparse_floor_pwm` option (1–255), its existing default of 2 and all explicitly configured values. Select `sparse_floor_pwm: 1` explicitly for the lowest reference.
- Keep sparse dimming disabled by default and retain the existing driver behavior and transmission timings.
- Document a complete power cycle of the controller and strip as a troubleshooting step for unexpected LED states after OTA updates.
- Add real ESPHome configuration/code-generation checks for existing explicit values, disabled sparse dimming and combined encoder settings; retain all-floor curve coverage and test mixed colors and white fades at reference 1 and 2.

---

# ESPHome WS2805 v0.6.11

- Add configurable sparse_floor_pwm (default 2; set 1 for the previous sparse minimum). Sparse dimming remains disabled by default.
- Remove brightness-ratio recovery to keep mixed colors stable at PWM rounding boundaries; keep strict monotonic regression tests.
- Correct zero-bit timing to 300/975 ns and retain the in-spec 800/800 ns one-bit timing.
- Validate RMT symbol representability and calculate transmit timeouts from actual rounded timings for long strips.
- Synchronize user documentation and cover floors, mixed colors, fades, timing conversion and packet lengths in native tests.

---

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
