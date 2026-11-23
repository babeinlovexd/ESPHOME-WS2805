<p align="center">
  <a href="https://github.com/babeinlovexd"><img src="https://raw.githubusercontent.com/babeinlovexd/babeinlovexd/main/bxd-github-header.svg" width="520" alt="BxD / BabeinlovexD"></a>
</p>

# ESPHome WS2805

[English](README.md) · [Konfigurationsreferenz (Englisch)](docu.md) · [Downloads](https://github.com/babeinlovexd/ESPHOME-WS2805/releases)

Externe ESPHome-Komponente für **WS2805-LED-Streifen mit fünf RGBCCT-Kanälen**. RGB-Effekte sind adressierbar; Warmweiß und Kaltweiß werden global für den ganzen Streifen gesteuert.

## Funktionen

- Adressierbare RGB-Effekte von ESPHome und globale Weißkanal-/CCT-Steuerung.
- Manuelles Beimischen der Weißkanäle während Effekten mit Synchronisierung der Home-Assistant-Anzeige.
- Einstellbare Kanalreihenfolge, Weißübergänge, temporales Dithering und Farbverriegelung.
- Nativer ESP32-RMT-Ausgang mit optionalen Einstellungen für die Signalübertragung.
- Optionales Sparse-Dimming für sehr schwaches statisches Licht.

## Installation

Ergänze deine ESPHome-Gerätekonfiguration um diesen Block. Wähle das passende Board und den Daten-GPIO. Gerätename, Netzwerk und API werden passend zum Gerät konfiguriert.

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
    effects:
      - addressable_rainbow:
      - addressable_scan:
```

## ESPHome-Kompatibilität

Komponentenversion **0.6.8** hat die Color-Accessor-API für **ESPHome 2026.11.0** eingeführt und unterstützt weiterhin die bisherige Schnittstelle aus **2026.10.2**. Die Beispiele verwenden ESP-IDF. Wähle einen Komponentenstand, der deine Einstellungen unterstützt.

## Konfiguration

| Option | Standard / Voraussetzung |
| :--- | :--- |
| `pin` | Erforderlicher Daten-GPIO |
| `num_leds` | Erforderliche Anzahl adressierbarer Pixel. Bei mehreren LEDs pro Pixel zählen die adressierbaren Gruppen. |
| `channel_order` | `GRBWWCW`; außerdem `RGBWWCW`, `RGBCWWW`, `GRBCWWW`, `BRGWWCW`, `BRGCWWW` |
| `color_interlock` | `false` |
| `constant_brightness` | `false` |
| `cold_white_color_temperature` | `153 mireds` |
| `warm_white_color_temperature` | `500 mireds` |
| `cct_transition_speed` | `3s` |
| `dithering` | `false` |
| `isr_priority` | `3`, zulässiger Bereich 1–3 |
| `din_pin` | Überschreibt bei Angabe den mit `pin` gewählten Daten-GPIO |
| `fdin_pin` | Optionaler zweiter Datenausgang für Streifen mit redundantem Eingang |
| `rmt_preencode` | `false`. Bereitet die RMT-Symbole ab ESP-IDF 5.3 vor dem Senden vor. |
| `rmt_no_dma_blocks` | `2`. Wählt `2` oder `4` Speicherblöcke je Ausgang ohne RMT-DMA ab ESP-IDF 5. |
| `sparse_dimming` | `false`. Aktiviert räumliches Dimmen bei sehr niedriger Helligkeit. |

`pin` ist auch bei Angabe von `din_pin` erforderlich. Die zweite Datenleitung beansprucht einen eigenen RMT-Kanal. Die verfügbaren Kanäle hängen von der ESP32-Variante, dem reservierten Speicher je Ausgang und weiteren RMT-Komponenten auf dem Gerät ab.

### Einstellungen für die Signalübertragung

Beide Optionen gehören in den jeweiligen `ws2805`-Lichteintrag und lassen sich unabhängig testen.

- **`rmt_preencode: true`** bereitet den vollständigen Frame vor dem Senden vor. Dadurch ist weniger Arbeit nötig, wenn der RMT-Sender neue Symbole braucht. Ab ESP-IDF 5.3 belegen die drei Datenpuffer bei 1.000 Pixeln rund 11 kB mit der Standardeinstellung oder 166 kB mit Vorcodierung. Der Sendepuffer benötigt zusammenhängenden internen RAM. Ältere ESP-IDF-Pfade bereiten die Symbole bereits vor dem Senden vor.
- **`rmt_no_dma_blocks: 4`** gibt dem Sender ohne RMT-DMA mehr Zeit zwischen zwei Nachfüllvorgängen. Durch die größere Reservierung stehen weniger Ausgänge und weitere RMT-Kanäle zur Verfügung. DIN und FDIN benötigen jeweils eigene Blöcke. Varianten mit DMA und der alte Treiber unter ESP-IDF 4 verwenden ihre bisherige Speicherzuteilung.

Teste bei kurzem Aufblitzen oder falschen Farben unter Last jeweils eine Einstellung. Beide können das Risiko von Übertragungsstörungen senken. Die konfigurierten Bitzeiten bestimmen die Signalgeschwindigkeit.

### Sparse-Dimming

Aktiviere `sparse_dimming: true` für ein statisches Nachtlicht. Im untersten Helligkeitsbereich leuchtet nur ein Teil der Pixel. Für einen voll angeforderten Kanal gilt dort die PWM-Referenz 1. Schwächere Kanalanteile in Mischfarben können auf null gerundet werden.

Der maximale Abstand im Muster beträgt acht Pixel. Beim Dimmen können sich Anzahl und Position der leuchtenden Pixel ändern. Ein Diffusor kann das Muster weniger sichtbar machen. Der Controller sendet weiterhin Daten für alle Pixel. Während eines Lichteffekts greift Sparse-Dimming nicht.

### Timing & Diagnose

Die Standards sind 300 ns High und 975 ns Low für ein Null-Bit, je 800 ns für High und Low beim Eins-Bit und 300 µs für den Reset. Das Null-Bit dauert nominell 1.275 ns. Die tatsächlichen Pulszeiten werden auf die Auflösung des RMT-Takts gerundet. Bei abweichenden Anforderungen des Streifens lassen sich die [Timing-Werte](docu.md#advanced-timing) in der YAML einstellen.

Template-Sensoren können über `output_id` Framegröße, RMT-Takt, Framezeit und Übertragungsfehler abfragen. Die Referenz enthält ein [Diagnosebeispiel](docu.md#diagnostics).

## Beispiele & Fehlersuche

- [example.yaml](example.yaml) enthält ein vollständiges Beispielgerät.
- **Falsche Farben:** `channel_order` mit dem Streifen abgleichen.
- **Unerwartete Weißkanäle:** Farbverriegelung und manuell eingestellte Weißwerte während Effekten prüfen.
- **Aufblitzen oder RMT-Fehler:** Versorgung, gemeinsame Masse, Signalpegel, Streifenlänge und die RMT-Meldungen des Geräts prüfen.
- **Unerwartetes Verhalten nach einem OTA-Update:** Bei anhaltenden Auffälligkeiten Controller und Streifen vollständig stromlos machen. Auch eine getrennte Versorgung des Streifens abschalten. Danach die Versorgung wieder einschalten und erneut testen.

## Lizenz

[CC BY-NC-SA 4.0](LICENSE). Es gilt die bestehende Projektlizenz. Drittanbieter-Komponenten behalten ihre jeweiligen Lizenzen.

Dieses Hobbyprojekt unterstützen: [PayPal](https://www.paypal.me/babeinlovexd).

[BxD / BabeinlovexD](https://github.com/babeinlovexd) · [Website](https://babeinlovexd.de/)
