<p align="center">
  <a href="https://github.com/babeinlovexd"><img src="https://raw.githubusercontent.com/babeinlovexd/babeinlovexd/main/bxd-github-header.svg" width="520" alt="BxD / BabeinlovexD"></a>
</p>

# ESPHome WS2805

[English](README.md) · [Konfigurationsreferenz (Englisch)](docu.md) · [Downloads](https://github.com/babeinlovexd/ESPHOME-WS2805/releases)

Externe ESPHome-Komponente für **WS2805-LED-Streifen mit fünf RGBCCT-Kanälen**. RGB lässt sich für jedes Pixel einzeln steuern. Warmweiß und Kaltweiß werden für den ganzen Streifen eingestellt.

## Funktionen

- ESPHome-RGB-Effekte sowie Steuerung von Helligkeit, Farbe und Farbtemperatur in Home Assistant.
- Einstellbare Kanalreihenfolge, Weißmischung und temporales Dithering.
- ESP32-RMT-Ausgang mit optionalen Einstellungen für Signalübertragung und Sparse-Dimming.

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
    default_transition_length: 1s
    dithering: false
    effects:
      - addressable_rainbow:
      - addressable_scan:
```

## ESPHome-Kompatibilität

Die Komponente unterstützt **ESPHome 2026.10.2** und **2026.11.0**. Die Beispiele verwenden ESP-IDF.

## Home Assistant

Aktiviere `api:` in der Gerätekonfiguration und füge das Gerät über die [ESPHome-Integration](https://www.home-assistant.io/integrations/esphome/) hinzu. Der Streifen steht als Licht für Dashboards, Szenen und Automationen zur Verfügung.

- Mit `default_transition_length` stellst du die Standarddauer für Übergänge ein. Für eine einzelne Aktion kannst du in Home Assistant mit `transition` eine Dauer in Sekunden vorgeben. Mit `0` erfolgt die Änderung sofort.
- Mit `color_interlock: false` bietet das Licht den Modus RGBWW. Kaltweiß und Warmweiß lassen sich mit festen Farben oder RGB-Effekten mischen.
- Mit `color_interlock: true` stehen getrennte Modi für RGB und Farbtemperatur zur Verfügung. Passe die beiden Farbtemperaturgrenzen an deinen Streifen an.
- Mit `light.addressable_set` kannst du in ESPHome-Automationen einzelne RGB-Pixel steuern. Über [Lichtpartitionen](https://esphome.io/components/light/partition/) lassen sich RGB-Segmente als eigene Lichter in Home Assistant steuern. Gib dem ursprünglichen Streifen dazu eine ID und setze `internal: true` und `sparse_dimming: false`. Home Assistant kann geerbte Weißregler für die Segmente anzeigen. Steuere Warmweiß und Kaltweiß über einen Befehl an den gesamten Streifen.

Über die optionale ESPHome-Einstellung `power_supply` lässt sich die Versorgung des Streifens schalten. Die ESPHome-Option `power_estimation` steht für WS2805 nicht zur Verfügung.

## Konfiguration

| Option | Standard / Voraussetzung |
| :--- | :--- |
| `pin` | Daten-GPIO. Gib `pin` oder `din_pin` an. |
| `num_leds` | Erforderliche Anzahl adressierbarer Pixel. Bei mehreren LEDs pro Pixel zählen die adressierbaren Gruppen. |
| `channel_order` | `GRBWWCW`; außerdem `RGBWWCW`, `RGBCWWW`, `GRBCWWW`, `BRGWWCW`, `BRGCWWW` |
| `color_interlock` | `false` |
| `constant_brightness` | `false` |
| `cold_white_color_temperature` | `153 mireds` (etwa 6.536 K) |
| `warm_white_color_temperature` | `500 mireds` (2.000 K). Wähle einen niedrigeren Kelvin-Wert als für Kaltweiß. |
| `default_transition_length` | ESPHome-Standard `1s` für Übergänge von Farbe, Helligkeit und Weißkanälen. |
| `cct_transition_speed` | Bisherige Einstellung. Ein expliziter Wert gibt die Standarddauer vor, wenn `default_transition_length` fehlt. |
| `dithering` | `false` |
| `isr_priority` | `3`. Mit `0` wählt der RMT-Treiber die Priorität. Alternativ kannst du `1`–`3` festlegen. |
| `din_pin` | Primärer Daten-GPIO bei Angabe. Hat Vorrang vor `pin`. |
| `fdin_pin` | Optionaler zweiter Datenausgang für Streifen mit redundantem Eingang |
| `rmt_preencode` | `false`. Bereitet die RMT-Symbole ab ESP-IDF 5.3 vor dem Senden vor. |
| `rmt_no_dma_blocks` | `2`. Wählt ab ESP-IDF 5 je Ausgang ohne DMA `2` oder `4` Speicherblöcke. |
| `sparse_dimming` | `false`. Aktiviert räumliches Dimmen bei sehr niedriger Helligkeit. |

DIN und FDIN beanspruchen jeweils einen RMT-Ausgang. Verwende GPIOs, die auf deinem Board als nicht invertierte Push-Pull-Ausgänge nutzbar sind. Die verfügbaren Ausgänge hängen von der ESP32-Variante, dem reservierten Speicher je Ausgang und weiteren RMT-Komponenten auf dem Gerät ab.

Auf ESP32-Varianten mit RMT-DMA nutzt der erste WS2805-DIN-Ausgang DMA. FDIN und weitere WS2805-DIN-Ausgänge verwenden den RMT-Hardwarespeicher. Schlägt die Einrichtung des optionalen FDIN-Ausgangs fehl, protokolliert die Komponente den Fehler und verwendet weiterhin DIN.

Ab ESP-IDF 5 hat ein ESP32-C3 oder ESP32-C6 mit den Standardeinstellungen und ohne weitere RMT-Geräte Platz für einen WS2805-DIN-Ausgang. Ein ESP32-S3 kann einen DIN-Ausgang mit DMA und einen weiteren Ausgang ohne DMA nutzen. Der weitere Ausgang kann einen zweiten Streifen oder den Reserveeingang des ersten Streifens ansteuern.

### Einstellungen für die Signalübertragung

Beide Optionen gehören in den jeweiligen `ws2805`-Lichteintrag und lassen sich unabhängig testen.

- **`rmt_preencode: true`** bereitet den vollständigen Frame vor dem Senden vor. Dadurch ist weniger Arbeit nötig, wenn der RMT-Sender neue Symbole braucht. Ab ESP-IDF 5.3 belegen die drei Datenpuffer bei 1.000 Pixeln rund 11 kB mit der Standardeinstellung oder 166 kB mit Vorcodierung. Der Sendepuffer benötigt zusammenhängenden internen RAM. Ältere ESP-IDF-Pfade bereiten die Symbole bereits vor dem Senden vor.
- **`rmt_no_dma_blocks: 4`** gibt jedem Ausgang ohne DMA mehr Symbole zwischen zwei Nachfüllvorgängen. Jeder dieser DIN- oder FDIN-Ausgänge reserviert eigene Blöcke. Größere Reservierungen lassen weniger Platz für weitere RMT-Ausgänge.

Teste bei kurzem Aufblitzen oder falschen Farben unter Last jeweils eine Einstellung. Wenn das Gerät weitere RMT-Komponenten nutzt, kann der Treiber mit `isr_priority: 0` die gemeinsame Interrupt-Priorität wählen oder übernehmen. Die konfigurierten Bitzeiten bestimmen die Signalgeschwindigkeit.

### Sparse-Dimming

Aktiviere `sparse_dimming: true` für ein statisches Nachtlicht. Die Einstellung verwendet rohe PWM-Werte für feste Farben. Dadurch kann sich die Helligkeit von der üblichen gammakorrigierten Ausgabe unterscheiden. Im untersten Helligkeitsbereich leuchtet nur ein Teil der Pixel. Für einen voll angeforderten Kanal gilt dort die PWM-Referenz 1. Schwächere Kanalanteile in Mischfarben können auf null gerundet werden.

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
