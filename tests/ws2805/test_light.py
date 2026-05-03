import sys
from unittest.mock import MagicMock, patch
import pytest

# Define the exception class
class MockInvalid(Exception):
    pass

@pytest.fixture(autouse=True)
def setup_mocks():
    mock_cg = MagicMock()
    mock_cv = MagicMock()
    mock_light = MagicMock()
    mock_esp32 = MagicMock()
    mock_esp32_const = MagicMock()
    mock_pins = MagicMock()
    mock_core = MagicMock()
    mock_const = MagicMock()

    mock_cv.Invalid = MockInvalid
    mock_esp32_const.VARIANT_ESP32 = "ESP32"
    mock_esp32_const.VARIANT_ESP32S2 = "ESP32-S2"
    mock_esp32_const.VARIANT_ESP32S3 = "ESP32-S3"
    mock_esp32_const.VARIANT_ESP32C3 = "ESP32-C3"
    mock_esp32_const.VARIANT_ESP32C6 = "ESP32-C6"

    # Use a dictionary for sys.modules to simulate the modules
    modules = {
        "esphome": MagicMock(),
        "esphome.codegen": mock_cg,
        "esphome.config_validation": mock_cv,
        "esphome.components": MagicMock(),
        "esphome.components.light": mock_light,
        "esphome.components.esp32": mock_esp32,
        "esphome.components.esp32.const": mock_esp32_const,
        "esphome.pins": mock_pins,
        "esphome.core": mock_core,
        "esphome.const": mock_const,
    }

    # Pre-populate parent modules to avoid AttributeError: __spec__
    for name, m in modules.items():
        m.__name__ = name
        m.__file__ = f"{name.replace('.', '/')}.py"
        m.__package__ = name.rsplit('.', 1)[0] if '.' in name else ''

    with patch.dict(sys.modules, modules):
        yield {
            "esp32": mock_esp32,
            "const": mock_esp32_const,
            "cv": mock_cv,
            "core": mock_core,
        }

def test_validate_rmt_usage_esp32(setup_mocks):
    mock_esp32 = setup_mocks["esp32"]
    mock_const = setup_mocks["const"]
    mock_core = setup_mocks["core"]

    mock_esp32.get_esp32_variant.return_value = mock_const.VARIANT_ESP32
    mock_core.CORE.data = {}

    # Import inside the test
    sys.path.append("components")
    import ws2805.light as ws2805_light

    # Inject mocks into the module
    ws2805_light.esp32 = mock_esp32
    ws2805_light.const = mock_const
    ws2805_light.cv = setup_mocks["cv"]
    ws2805_light.CORE = mock_core.CORE

    config = {}
    for i in range(1, 9):
        assert ws2805_light.validate_rmt_usage(config) == config
        assert mock_core.CORE.data["ws2805"] == i

    with pytest.raises(MockInvalid) as excinfo:
        ws2805_light.validate_rmt_usage(config)
    assert "Too many WS2805 instances (9) for ESP32" in str(excinfo.value)

def test_validate_rmt_usage_esp32s3(setup_mocks):
    mock_esp32 = setup_mocks["esp32"]
    mock_const = setup_mocks["const"]
    mock_core = setup_mocks["core"]

    mock_esp32.get_esp32_variant.return_value = mock_const.VARIANT_ESP32S3
    mock_core.CORE.data = {}

    sys.path.append("components")
    import ws2805.light as ws2805_light
    ws2805_light.esp32 = mock_esp32
    ws2805_light.const = mock_const
    ws2805_light.cv = setup_mocks["cv"]
    ws2805_light.CORE = mock_core.CORE

    config = {}
    for i in range(1, 5):
        assert ws2805_light.validate_rmt_usage(config) == config
        assert mock_core.CORE.data["ws2805"] == i

    with pytest.raises(MockInvalid) as excinfo:
        ws2805_light.validate_rmt_usage(config)
    assert "Too many WS2805 instances (5) for ESP32-S3" in str(excinfo.value)

def test_validate_rmt_usage_esp32c3(setup_mocks):
    mock_esp32 = setup_mocks["esp32"]
    mock_const = setup_mocks["const"]
    mock_core = setup_mocks["core"]

    mock_esp32.get_esp32_variant.return_value = mock_const.VARIANT_ESP32C3
    mock_core.CORE.data = {}

    sys.path.append("components")
    import ws2805.light as ws2805_light
    ws2805_light.esp32 = mock_esp32
    ws2805_light.const = mock_const
    ws2805_light.cv = setup_mocks["cv"]
    ws2805_light.CORE = mock_core.CORE

    config = {}
    for i in range(1, 3):
        assert ws2805_light.validate_rmt_usage(config) == config
        assert mock_core.CORE.data["ws2805"] == i

    with pytest.raises(MockInvalid) as excinfo:
        ws2805_light.validate_rmt_usage(config)
    assert "Too many WS2805 instances (3) for ESP32-C3" in str(excinfo.value)

def test_validate_rmt_usage_unknown_variant(setup_mocks):
    mock_esp32 = setup_mocks["esp32"]
    mock_const = setup_mocks["const"]
    mock_core = setup_mocks["core"]

    mock_esp32.get_esp32_variant.return_value = "UNKNOWN"
    mock_core.CORE.data = {}

    sys.path.append("components")
    import ws2805.light as ws2805_light
    ws2805_light.esp32 = mock_esp32
    ws2805_light.const = mock_const
    ws2805_light.cv = setup_mocks["cv"]
    ws2805_light.CORE = mock_core.CORE

    config = {}
    # Unknown variant defaults to 8
    for i in range(1, 9):
        assert ws2805_light.validate_rmt_usage(config) == config

    with pytest.raises(MockInvalid) as excinfo:
        ws2805_light.validate_rmt_usage(config)
    assert "Too many WS2805 instances (9) for UNKNOWN" in str(excinfo.value)
