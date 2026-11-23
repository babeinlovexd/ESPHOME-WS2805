from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path
import sys
from types import ModuleType, SimpleNamespace
from unittest.mock import MagicMock

import pytest

CODEOWNERS = ["@BabeinlovexD"]


class MockInvalid(Exception):
    def __init__(self, message, path=None):
        super().__init__(message)
        self.path = path


@pytest.fixture
def ws2805(monkeypatch):
    cv = MagicMock()
    cv.Invalid = MockInvalid
    cv.Version = lambda *parts: parts
    cv.color_temperature = float
    components = ModuleType("esphome.components")
    components.esp32 = MagicMock()
    components.esp32.get_esp32_variant.return_value = "ESP32"
    components.esp32.idf_version.return_value = (5, 5, 6)
    components.esp32_rmt = ModuleType("esphome.components.esp32_rmt")
    components.light = MagicMock()

    keys = (
        "COLD_WHITE_COLOR_TEMPERATURE", "COLOR_INTERLOCK", "CONSTANT_BRIGHTNESS",
        "DEFAULT_TRANSITION_LENGTH", "ID", "INVERTED", "MODE", "NAME", "NUMBER",
        "NUM_LEDS", "OUTPUT", "OUTPUT_ID", "PIN", "RMT_SYMBOLS",
        "SETUP_PRIORITY", "USE_DMA", "WARM_WHITE_COLOR_TEMPERATURE",
    )
    modules = {
        "esphome.codegen": MagicMock(),
        "esphome.components": components,
        "esphome.config_validation": cv,
        "esphome.const": SimpleNamespace(**{f"CONF_{key}": key.lower() for key in keys}),
        "esphome.core": SimpleNamespace(CORE=SimpleNamespace(data={})),
        "esphome.pins": MagicMock(),
    }
    esphome = ModuleType("esphome")
    for name, module in modules.items():
        setattr(esphome, name.rsplit(".", 1)[-1], module)
    monkeypatch.setitem(sys.modules, "esphome", esphome)
    for name, module in modules.items():
        monkeypatch.setitem(sys.modules, name, module)

    schema = Path(__file__).resolve().parents[2] / "components/ws2805/light.py"
    spec = spec_from_file_location("ws2805_light_schema", schema)
    ws2805 = module_from_spec(spec)
    spec.loader.exec_module(ws2805)
    return ws2805


def strip(index, blocks=2, backup=False, **options):
    configuration = {
        "id": f"ws_{index}",
        "name": f"Strip {index}",
        "pin": {"number": index + 10},
        "isr_priority": 3,
        "rmt_no_dma_blocks": blocks,
        **options,
    }
    if backup:
        configuration["fdin_pin"] = {"number": 0}
    return configuration


@pytest.mark.parametrize(
    "variant, strips, expected_error",
    [
        ("ESP32", [(2, False)] * 4, None),
        ("ESP32", [(2, False)] * 5, "2 contiguous"),
        ("ESP32", [(2, True), (4, False)], None),
        ("ESP32", [(4, True), (2, False)], "2 contiguous"),
        ("ESP32S2", [(2, True)], None),
        ("ESP32S2", [(4, True)], "4 contiguous"),
        ("ESP32C3", [(4, False)], None),
        ("ESP32C3", [(2, True)], "2 contiguous"),
        ("ESP32C3", [(4, False), (2, False)], "2 contiguous"),
        ("ESP32C6", [(2, False), (2, False)], "2 contiguous"),
        ("ESP32S3", [(2, True)], None),
        ("ESP32S3", [(4, True)], "4 contiguous"),
        ("ESP32S3", [(2, False)] * 2, None),
        ("ESP32S3", [(2, False)] * 3, "2 contiguous"),
    ],
)
def test_rmt_capacity(ws2805, variant, strips, expected_error):
    ws2805.esp32.get_esp32_variant.return_value = variant
    configurations = [strip(i, blocks, backup) for i, (blocks, backup) in enumerate(strips)]
    for configuration in configurations:
        ws2805.register_rmt_usage(configuration)

    for configuration in configurations[:-1]:
        ws2805.validate_rmt_usage(configuration)
    if expected_error:
        with pytest.raises(MockInvalid, match=expected_error):
            ws2805.validate_rmt_usage(configurations[-1])
    else:
        ws2805.validate_rmt_usage(configurations[-1])


def test_priorities_follow_setup_order(ws2805):
    first = strip(1)
    second = strip(2, isr_priority=0, setup_priority=900)
    for configuration in (first, second):
        ws2805.register_rmt_usage(configuration)

    with pytest.raises(MockInvalid, match="The first strip uses 0"):
        ws2805.validate_rmt_usage(first)
    first["isr_priority"] = 0
    ws2805.validate_rmt_usage(first)
    ws2805.validate_rmt_usage(second)


def test_legacy_allocates_one_block_per_output(ws2805):
    ws2805.esp32.get_esp32_variant.return_value = "ESP32C3"
    ws2805.esp32.idf_version.return_value = (4, 4, 8)
    configuration = strip(1, blocks=4, backup=True)
    ws2805.register_rmt_usage(configuration)
    ws2805.validate_rmt_usage(configuration)


@pytest.mark.parametrize("variant", ["ESP32C2", "ESP32C61"])
def test_variants_without_rmt(ws2805, variant):
    ws2805.esp32.get_esp32_variant.return_value = variant
    with pytest.raises(MockInvalid, match="has no RMT peripheral"):
        ws2805.register_rmt_usage(strip(1))


def test_shared_allocator_receives_din_and_fdin(ws2805):
    ws2805.esp32.get_esp32_variant.return_value = "ESP32S3"
    channels = []
    ws2805.esp32_rmt.request_rmt_channel = MagicMock(return_value=channels.append)
    ws2805.esp32_rmt.final_validate_rmt_channels = MagicMock()
    configuration = strip(1, backup=True)

    ws2805.register_rmt_usage(configuration)
    ws2805.validate_rmt_usage(configuration)

    assert [(entry["id"], entry["use_dma"], entry["rmt_symbols"]) for entry in channels] == [
        ("ws_1", True, 192),
        ("ws_1", False, 96),
    ]
    assert channels[1]["name"] == "Strip 1 FDIN"
    ws2805.esp32_rmt.final_validate_rmt_channels.assert_called_once_with(configuration)


def test_legacy_transition_alias_and_primary_pin(ws2805):
    configuration = ws2805.normalize_config(
        {"cct_transition_speed": "3s", "pin": "GPIO4", "din_pin": "GPIO5", "fdin_pin": "GPIO4"}
    )
    assert configuration == {
        "default_transition_length": "3s", "din_pin": "GPIO5", "fdin_pin": "GPIO4"
    }
    assert ws2805.normalize_config(
        {"cct_transition_speed": "3s", "default_transition_length": "0s"}
    ) == {"default_transition_length": "0s"}


@pytest.mark.parametrize("value", ["0", "inf", "nan", "-1"])
def test_white_temperature_must_be_positive_and_finite(ws2805, value):
    with pytest.raises(MockInvalid, match="greater than zero"):
        ws2805.validate_white_temperature(value)


def test_uninverted_push_pull_pin_required(ws2805):
    pin = {"number": 0, "mode": {"output": True}, "inverted": False}
    assert ws2805.validate_output_pin(pin) == pin
    with pytest.raises(MockInvalid, match="non-inverted"):
        ws2805.validate_output_pin({**pin, "inverted": True})
    with pytest.raises(MockInvalid, match="push-pull"):
        ws2805.validate_output_pin({**pin, "mode": {"output": True, "open_drain": True}})
