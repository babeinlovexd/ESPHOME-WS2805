<p align="center">
  <a href="https://github.com/babeinlovexd"><img src="https://raw.githubusercontent.com/babeinlovexd/babeinlovexd/main/bxd-github-header.svg" width="520" alt="BxD / BabeinlovexD"></a>
</p>

# ESPHome WS2805

[English](README.md) · [Fragen & Fehler](https://github.com/babeinlovexd/ESPHOME-WS2805/issues) · [Releases](https://github.com/babeinlovexd/ESPHOME-WS2805/releases)

Externe ESPHome-Komponente für **WS2805-LED-Streifen mit fünf RGBCCT-Kanälen**. RGB-Effekte sind adressierbar; Warmweiß und Kaltweiß werden global für den ganzen Streifen gesteuert.

## Funktionen

- Adressierbare RGB-Effekte von ESPHome und globale Weißkanal-/CCT-Steuerung.
- Manuelles Beimischen der Weißkanäle während Effekten mit Synchronisierung der Home-Assistant-Anzeige.
- Einstellbare Kanalreihenfolge, Weißübergänge, temporales Dithering und Farbverriegelung.
- Eigener nativer ESP32-RMT-Treiber ohne NeoPixelBus-Abhängigkeit.

## Installation

Ergänze deine ESPHome-Gerätekonfiguration um diesen Block. Gerätename, Netzwerk und API werden zusätzlich passend zum Gerät konfiguriert:

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
    name: "WS2805"
    pin: GPIO4
    num_leds: 60
    channel_order: GRBWWCW
    color_interlock: false
    constant_brightness: false
    cct_transition_speed: 3s
    dithering: false
    effects:
      - addressable_rainbow:
      - addressable_scan:
```

## ESPHome-Kompatibilität

Version **0.6.8** ergänzt die von **ESPHome 2026.11.0** benötigte Color-Accessor-API und erhält die bisherige Schnittstelle für **2026.10.2**. Versionen bis einschließlich 0.6.7 verwenden die alte API und kompilieren mit 2026.11.0 nicht.

Die Build-Matrix kompiliert beide festgelegten ESPHome-Versionen sowie die neueste stabile Version für **ESP32** und **ESP32-S3**. Native Regressionstests prüfen die Fünf-Byte-Pixeladressierung, alle sechs Kanalreihenfolgen, RGB-Korrektur, Effektdaten und die Trennung der beiden globalen Weißkanäle. Siehe [Issue #59](https://github.com/babeinlovexd/ESPHOME-WS2805/issues/59).

## Konfiguration

| Option | Standard / Voraussetzung |
| :--- | :--- |
| `pin` | Erforderlicher Daten-GPIO |
| `rmt_preencode` | `false`; `true` bereitet die Sendesymbole vorab auf |
| `num_leds` | Erforderliche positive LED-Anzahl |
| `channel_order` | `GRBWWCW`; außerdem `RGBWWCW`, `RGBCWWW`, `GRBCWWW`, `BRGWWCW`, `BRGCWWW` |
| `color_interlock` | `false` |
| `constant_brightness` | `false` |
| `cold_white_color_temperature` | `153 mireds` |
| `warm_white_color_temperature` | `500 mireds` |
| `cct_transition_speed` | `3s` |
| `dithering` | `false` |
| `isr_priority` | `3`, zulässiger Bereich 1–3 |
| `fdin_pin` | Optionaler zweiter Datenausgang für Streifen mit redundantem Eingang |

Im aktuellen Schema bleibt `pin` auch bei Angabe von `din_pin` erforderlich; `din_pin` überschreibt dann den verwendeten Daten-GPIO. Für normale Konfigurationen genügt `pin`. Eine zweite Datenleitung ersetzt keine korrekte Spannungsversorgung und Masseverbindung.

### Mehrere Streifen

Der Komponenten-Validator erlaubt **8 Instanzen auf ESP32**, **4 auf ESP32-S2/S3** und **2 auf ESP32-C3/C6**. Weitere RMT-Nutzer sowie Chip und Framework können die nutzbaren Ressourcen zusätzlich begrenzen. Jeder Streifen benötigt einen eigenen Daten-GPIO. Acht unabhängige Streifen auf einem S3 sind damit nicht zugesichert.

### Timing & Diagnose

Erweiterte Timing-Standardwerte sind `bit0_high_ns: 300`, `bit0_low_ns: 800`, `bit1_high_ns: 800`, `bit1_low_ns: 800` und `reset_pulse_us: 300`. Diese Werte nur bei abweichenden Anforderungen des LED-Streifens ändern.

Template-Sensoren können über die Licht-ID `get_num_leds()`, `get_frame_bytes()`, `get_rmt_resolution_hz()`, `get_last_frame_ms()`, `get_max_refresh_hz()` und `get_tx_error_count()` aufrufen. Das sind Diagnosewerte, kein Nachweis einer bestimmten Hardwarekompatibilität.

## Beispiele & Fehlersuche

- [example.yaml](example.yaml) enthält ein vollständiges Beispielgerät; [docu.md](docu.md) erklärt Weißübergänge und Dithering.
- **Falsche Farben:** `channel_order` mit dem Streifen abgleichen.
- **Unerwartete Weißkanäle:** Farbverriegelung und manuell eingestellte Weißwerte während Effekten prüfen.
- **Blitzen oder RMT-Fehler:** Versorgung, gemeinsame Masse, Signalpegel, Streifenlänge, RMT-Belegung und Framework-Buildlog prüfen.
- **Kompilierfehler nach Updates:** ESPHome-Version, Board, Framework und eine minimale YAML im Issue angeben.

## Lizenz

[CC BY-NC-SA 4.0](LICENSE). Es gilt die bestehende Projektlizenz. Drittanbieter-Komponenten behalten ihre jeweiligen Lizenzen.

---

## Builds und Releases

Version: **0.6.9** ([VERSION](VERSION)). [GitHub Actions](.github/workflows/release.yml) prüft Pushes und Pull Requests. Ein Tag passend zu `v` + `VERSION` veröffentlicht das Release erst nach erfolgreichen Jobs; manuelle Läufe liefern Build-Artefakte. [Downloads](https://github.com/babeinlovexd/ESPHOME-WS2805/releases).

ESPHome-CI verwendet Platzhalter-Zugangsdaten und flasht keine Geräte. Vor dem Einsatz eigene Secrets und Pinbelegung prüfen.

## Mitmachen

Fragen, Fehlerberichte und Beiträge sind willkommen. Nenne Version, Aufbau und Schritte zum Nachstellen im [Issue](https://github.com/babeinlovexd/ESPHOME-WS2805/issues). Größere Änderungen besprechen wir am besten vor einem Pull Request.

Dieses Hobbyprojekt unterstützen: [PayPal](https://www.paypal.me/babeinlovexd).

[BxD / BabeinlovexD](https://github.com/babeinlovexd) · [Website](https://babeinlovexd.de/)

## Optionale Vorberechnung der Sendesymbole

Standardmäßig bleibt `rmt_preencode: false`: Bestehende Konfigurationen behalten unter ESP-IDF ab 5.3 ihren bisherigen sparsamen Speicherbedarf. Bei Flackern oder falschen ersten LEDs auf klassischen ESP32 kann im Licht-Eintrag `rmt_preencode: true` helfen. Das entlastet die Nachfüll-Interrupts, ist aber keine Garantie gegen jede Übertragungsstörung.

Die drei Datenpuffer benötigen mit `false` etwa `11 × num_leds` Bytes, mit `true` dagegen `166 × num_leds + 4` Bytes: bei 1.000 LEDs etwa 11 kB statt 166 kB. Dafür ist zusammenhängender interner RAM erforderlich; vorhandener PSRAM ersetzt diesen nicht. Bei langen oder mehreren Streifen den Standard beibehalten, sofern nicht genügend interner RAM frei ist. Unter ESP-IDF vor 5.3 werden die Symbole wie bisher immer vorab berechnet.
