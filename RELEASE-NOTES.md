# ESPHome WS2805 v0.6.13

- Extend the default zero bit to 300/975 ns so the total duration meets the WS2805 timing specification.
- Add optional RMT pre-encoding on ESP-IDF 5.3+ and a four-block allocation for eligible RMT outputs without DMA. The existing encoder and memory allocation remain the defaults.
- Add optional sparse dimming for low-level static lighting. The brightness curve joins the full-strip range without a drop at the boundary and respects the RGB factors in `color_correct`.
- Follow ESPHome's transition duration for RGB and both white channels. The default is 1 second unless configured. The legacy `cct_transition_speed` supplies the default when `default_transition_length` is omitted.
- Preserve RGB written by addressable effects, partitions, and pixel actions. With color interlock enabled, dark frames in RGB effects stay dark.
- Keep a configured `power_supply` active for pure warm or cold white. Reject `power_estimation` until both white channels can be accounted for.
- Validate RMT allocations against the selected ESP32 variant. Use a separate non-DMA channel for FDIN and keep DIN operating if the optional backup cannot initialize.
- Resume through DIN after a synchronized FDIN send failure when the active RMT transfer can be stopped and drained safely.
- Validate configured pulses against the selected RMT clock and compute the send timeout from the maximum frame duration.
- Document when to try a complete power cycle after an OTA update and explain the resource requirements of the expert options.

---

# ESPHome WS2805 v0.6.8

- Kompatibilität mit ESPHome 2026.11.0: neuer ESPColorAccessor für das WS2805-Fünf-Byte-Format. RGB-Effekte greifen ausschließlich auf RGB und ihre separaten Effektdaten zu; WW/CW bleiben global gesteuert.
- Die bisherige get_view_internal-Schnittstelle bleibt für ESPHome 2026.10.2 erhalten.
- Korrektur der im Issue gemeldeten Initialisierungsreihenfolge im Konstruktor.
- CI-Matrix für ESP32 und ESP32-S3 mit 2026.10.2, 2026.11.0 und der neuesten stabilen ESPHome-Version.
- Regressionstests für alle sechs Kanalreihenfolgen, Weißkanal-Isolation, Farbkorrektur und Puffer-Lebenszyklus.

Details: https://github.com/babeinlovexd/ESPHOME-WS2805/issues/59

Die Validierung umfasst Firmware-Kompilierung und native Puffertests. Ein Test an einem realen LED-Streifen wurde bei dieser Korrektur nicht durchgeführt.
