<p align="center">
  <a href="https://github.com/babeinlovexd"><img src="https://raw.githubusercontent.com/babeinlovexd/babeinlovexd/main/bxd-github-header.svg" width="520" alt="BxD / BabeinlovexD"></a>
</p>

# ESPHome WS2805

[English](README.md) · [Downloads](https://github.com/babeinlovexd/ESPHOME-WS2805/releases)

Externe ESPHome-Komponente für **WS2805-LED-Streifen mit fünf RGBCCT-Kanälen**. RGB-Effekte sind adressierbar; Warmweiß und Kaltweiß werden global für den ganzen Streifen gesteuert.

## Funktionen

- Adressierbare RGB-Effekte von ESPHome und globale Weißkanal-/CCT-Steuerung.
- Manuelles Beimischen der Weißkanäle während Effekten mit Synchronisierung der Home-Assistant-Anzeige.
- Einstellbare Kanalreihenfolge, Weißübergänge, temporales Dithering und Farbverriegelung.
- Optionales Sparse-Dimming für eine geringere mittlere Helligkeit bei statischer Beleuchtung.
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
    output_id: ws2805_output
    name: "WS2805"
    pin: GPIO4
    num_leds: 60
    channel_order: GRBWWCW
    color_interlock: false
    constant_brightness: false
    cct_transition_speed: 3s
    dithering: false
    rmt_preencode: false
    sparse_dimming: false
    effects:
      - addressable_rainbow:
      - addressable_scan:
```

## ESPHome-Kompatibilität

Komponentenversionen **ab 0.6.8** unterstützen die von **ESPHome 2026.11.0** verwendete Color-Accessor-API und bleiben mit **ESPHome 2026.10.2** kompatibel.

## Konfiguration

| Option | Standard / Voraussetzung |
| :--- | :--- |
| `pin` | Erforderlicher Daten-GPIO |
| `num_leds` | Erforderliche positive LED-Anzahl |
| `channel_order` | `GRBWWCW`; außerdem `RGBWWCW`, `RGBCWWW`, `GRBCWWW`, `BRGWWCW`, `BRGCWWW` |
| `color_interlock` | `false` |
| `constant_brightness` | `false` |
| `cold_white_color_temperature` | `153 mireds` |
| `warm_white_color_temperature` | `500 mireds` |
| `cct_transition_speed` | `3s` |
| `dithering` | `false` |
| `isr_priority` | `3`, zulässiger Bereich 1–3 |
| `rmt_preencode` | `false`; `true` bereitet die Sendesymbole vorab auf |
| `fdin_pin` | Optionaler zweiter Datenausgang für Streifen mit redundantem Eingang |
| `sparse_dimming` | `false`; räumliches Dimmen für statische Beleuchtung einschalten |
| `sparse_max_step` | `8`, zulässiger Bereich 2–32; größter Abstand zwischen aktiven LEDs |

Im aktuellen Schema bleibt `pin` auch bei Angabe von `din_pin` erforderlich; `din_pin` überschreibt dann den verwendeten Daten-GPIO. Für normale Konfigurationen genügt `pin`. Eine zweite Datenleitung ersetzt keine korrekte Spannungsversorgung und Masseverbindung.

### Optionale Vorberechnung der Sendesymbole

Standardmäßig bleibt `rmt_preencode: false`: Bestehende Konfigurationen behalten unter ESP-IDF ab 5.3 ihren bisherigen sparsamen Speicherbedarf. Bei Flackern oder falschen ersten LEDs auf klassischen ESP32 kann im Licht-Eintrag `rmt_preencode: true` helfen. Das entlastet die Nachfüll-Interrupts, ist aber keine Garantie gegen jede Übertragungsstörung.

Die drei Datenpuffer benötigen mit `false` etwa `11 × num_leds` Bytes, mit `true` dagegen `166 × num_leds + 4` Bytes: bei 1.000 LEDs etwa 11 kB beziehungsweise 166 kB. Dafür ist zusammenhängender interner RAM erforderlich; vorhandener PSRAM ersetzt diesen nicht. Bei langen oder mehreren Streifen den Standard beibehalten, sofern nicht genügend interner RAM frei ist. Unter ESP-IDF vor 5.3 werden die Symbole wie bisher immer vorab berechnet.

### Optionales Sparse-Dimming

Sparse-Dimming senkt die mittlere Helligkeit des Streifens, indem bei niedrigen Einstellungen einige LEDs ausgeschaltet bleiben. Die Funktion ist **standardmäßig deaktiviert** und greift nur, solange kein adressierbarer Effekt aktiv ist. So lässt sie sich beispielsweise im Licht-Eintrag einschalten:

```yaml
light:
  - platform: ws2805
    name: "WS2805 Nachtlicht"
    pin: GPIO4
    num_leds: 60
    sparse_dimming: true
    sparse_max_step: 8
```

`sparse_max_step` bestimmt den größten Abstand zwischen aktiven LEDs. Mit `8` leuchtet bei den niedrigsten Einstellungen ungefähr jede achte LED. Mit steigender Helligkeit kommen mehr LEDs hinzu, bevor der Ausgang in den gewöhnlichen PWM-Dimmbereich übergeht. Das Muster ist statisch und auf dem Streifen zentriert; sichtbare Lücken sind möglich. Die volle Helligkeit bleibt erhalten, und der Helligkeitsregler verwendet weiterhin 0–100 %.

Am unteren Ende des Bereichs verwenden die leuchtenden Pixel PWM `1`, den kleinsten von null verschiedenen Code.

Die Anzahl der LEDs begrenzt die verfügbaren Abstufungen. Sehr kurze Streifen bieten weniger unterscheidbare Stufen; es gibt eine kleinste Helligkeit oberhalb von „aus“ statt unbegrenztem Herunterdimmen. RGB und beide Weißkanäle folgen dem Muster der ausgewählten LEDs. Warm- und Kaltweiß bleiben dabei globale Einstellungen und werden nicht zu unabhängig konfigurierbaren Weißsegmenten. Sparse-Dimming reserviert keinen weiteren großen Frame-Puffer und ist unabhängig von `rmt_preencode`.

Die Weißübergänge und das optionale temporale Dithering folgen auf die Helligkeitsumrechnung. Beim Ausschalten bleibt das letzte Sparse-Muster erhalten, bis das Ausblenden der Weißkanäle beendet ist.

### Mehrere Streifen

Der Komponenten-Validator erlaubt **8 Instanzen auf ESP32**, **4 auf ESP32-S2/S3** und **2 auf ESP32-C3/C6**. Weitere RMT-Nutzer sowie Chip und Framework können die nutzbaren Ressourcen zusätzlich begrenzen. Jeder Streifen benötigt einen eigenen Daten-GPIO. Acht unabhängige Streifen auf einem S3 sind damit nicht zugesichert.

### Timing & Diagnose

Erweiterte Timing-Standardwerte sind `bit0_high_ns: 300`, `bit0_low_ns: 975`, `bit1_high_ns: 800`, `bit1_low_ns: 800` und `reset_pulse_us: 300`. Der RMT-Takt rundet die Zeiten auf Hardware-Ticks: Mit diesen Standardwerten dauert ein Null-Bit bei 80 oder 40 MHz insgesamt 1.275 ns, im älteren 10-MHz-Pfad 1.300 ns. Ein Eins-Bit dauert 1.600 ns.

Die [Timing-Vorgaben des Herstellers](https://dl.lightstream.pro/PDF/IC/WS2805_EN.pdf#page=3) nennen 220–380 ns für den High-Puls des Null-Bits und 580–1.000 ns für die anderen drei Pulse. Die gesamte Bitdauer muss mindestens 1.250 ns betragen. Timing-Parameter nur bei abweichenden Anforderungen des Streifens ändern und dabei Pulsbreiten sowie Gesamtdauer innerhalb der Vorgaben halten.

Bitzeiten, die sich beim gewählten RMT-Takt nicht darstellen lassen, werden mit einer Fehlermeldung abgelehnt. Sehr lange Reset-Pulse werden auf die Hardwaregrenze begrenzt; die Anpassung wird im Log gemeldet.

Template-Sensoren können über die separate `output_id` (oben beispielsweise `ws2805_output`) die Methoden `get_num_leds()`, `get_frame_bytes()`, `get_rmt_resolution_hz()`, `get_last_frame_ms()`, `get_max_refresh_hz()` und `get_tx_error_count()` aufrufen. Die Licht-`id` bezeichnet den Lichtzustand und nicht den Treiberausgang. Siehe das [Diagnosebeispiel](docu.md#diagnostics). Die Werte belegen weder die Signalqualität noch eine bestimmte Hardwarekompatibilität.

## Beispiele & Fehlersuche

- [example.yaml](example.yaml) enthält ein vollständiges Beispielgerät; [docu.md](docu.md) erklärt Weißübergänge und Dithering.
- **Falsche Farben:** `channel_order` mit dem Streifen abgleichen.
- **Unerwartete Weißkanäle:** Farbverriegelung und manuell eingestellte Weißwerte während Effekten prüfen.
- **Lücken bei sehr geringer Helligkeit:** Bei `sparse_dimming: true` vorgesehen. Für ein dichteres Muster `sparse_max_step` verringern oder mit `sparse_dimming: false` zum gewöhnlichen Dimmen des ganzen Streifens zurückkehren.
- **Ungleichmäßiges Licht bei den niedrigsten Sparse-Einstellungen:** Prüfen, ob die Bit-Zeiten innerhalb der Spezifikation liegen (siehe „Timing & Diagnose") — eine außerhalb der Spezifikation liegende „1"-Bit-Low-Zeit lässt PWM 1 fleckig erscheinen.
- **Blitzen oder RMT-Fehler:** Versorgung, gemeinsame Masse, Signalpegel, Streifenlänge, RMT-Belegung und Framework-Logs prüfen.
- **Flackern oder falsche erste LEDs:** Auf klassischen ESP32 bei ausreichend freiem internem RAM `rmt_preencode: true` ausprobieren.
- **Angaben für den Support:** Bei einer Fehlermeldung Komponenten- und ESPHome-Version, Board, Framework, relevante Logs und eine minimale YAML angeben.

## Lizenz

[CC BY-NC-SA 4.0](LICENSE). Es gilt die bestehende Projektlizenz. Drittanbieter-Komponenten behalten ihre jeweiligen Lizenzen.

## Mitmachen

Fragen, Rückmeldungen und Beiträge sind willkommen. Beschreibe deinen Aufbau und die Schritte zum Nachstellen eines Problems. Größere Änderungen besprechen wir am besten vor der Umsetzung.

Dieses Hobbyprojekt unterstützen: [PayPal](https://www.paypal.me/babeinlovexd).

[BxD / BabeinlovexD](https://github.com/babeinlovexd) · [Website](https://babeinlovexd.de/)
