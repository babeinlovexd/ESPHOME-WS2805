# ESPHome WS2805 (unreleased)

- New `rmt_encoder` option (default `copy`). `copy` pre-expands symbols and
  memcpy's them in the RMT refill ISR — robust, and avoids underruns on the
  no-DMA classic ESP32. `simple` expands on-the-fly (RAM-efficient for large
  strips) but can underrun without RMT DMA. `simple` requires ESP-IDF 5.3+.
- Fixes intermittent flicker and wrong first-LED colors caused by the `simple`
  encoder underrunning the no-DMA RMT buffer under WiFi/BLE contention.
- CI compiles both encoder paths.

---

# ESPHome WS2805 v0.6.8

- Kompatibilität mit ESPHome 2026.11.0: neuer ESPColorAccessor für das WS2805-Fünf-Byte-Format. RGB-Effekte greifen ausschließlich auf RGB und ihre separaten Effektdaten zu; WW/CW bleiben global gesteuert.
- Die bisherige get_view_internal-Schnittstelle bleibt für ESPHome 2026.10.2 erhalten.
- Korrektur der im Issue gemeldeten Initialisierungsreihenfolge im Konstruktor.
- CI-Matrix für ESP32 und ESP32-S3 mit 2026.10.2, 2026.11.0 und der neuesten stabilen ESPHome-Version.
- Regressionstests für alle sechs Kanalreihenfolgen, Weißkanal-Isolation, Farbkorrektur und Puffer-Lebenszyklus.

Details: https://github.com/babeinlovexd/ESPHOME-WS2805/issues/59

Die Validierung umfasst Firmware-Kompilierung und native Puffertests. Ein Test an einem realen LED-Streifen wurde bei dieser Korrektur nicht durchgeführt.
