"""Check real ESPHome YAML acceptance and generated output for existing settings."""
from pathlib import Path
import subprocess
import sys

import pytest
import yaml


REPO = Path(__file__).resolve().parents[2]


def generate(tmp_path, options):
    build_path = tmp_path / "build"
    config = {
        "esphome": {"name": "ws2805-config-test", "build_path": build_path.as_posix()},
        "esp32": {"board": "esp32dev", "framework": {"type": "esp-idf"}},
        "logger": {},
        "external_components": [{
            "source": {"type": "local", "path": (REPO / "components").as_posix()},
            "components": ["ws2805"],
        }],
        "light": [{
            "platform": "ws2805", "name": "Configuration compatibility",
            "pin": "GPIO18", "num_leds": 50, **options,
        }],
    }
    config_path = tmp_path / "check.yaml"
    config_path.write_text(yaml.safe_dump(config, sort_keys=False), encoding="utf-8")
    result = subprocess.run(
        [sys.executable, "-m", "esphome", "compile", str(config_path), "--only-generate"],
        capture_output=True, text=True, encoding="utf-8", timeout=90,
    )
    return result, build_path / "src/main.cpp"


@pytest.mark.parametrize("options, floor", [
    ({}, 2),
    ({"sparse_dimming": True}, 2),
    ({"sparse_dimming": True, "sparse_floor_pwm": 1}, 1),
    ({"sparse_dimming": True, "sparse_floor_pwm": 2}, 2),
    ({"sparse_dimming": True, "sparse_floor_pwm": 32}, 32),
    ({"sparse_dimming": True, "sparse_floor_pwm": 255}, 255),
    ({"sparse_dimming": False, "sparse_floor_pwm": 2}, 2),
    ({"sparse_dimming": True, "sparse_floor_pwm": 2,
      "rmt_preencode": True, "sparse_max_step": 32}, 2),
])
def test_existing_options_reach_the_driver(tmp_path, options, floor):
    result, generated_path = generate(tmp_path, options)
    assert result.returncode == 0, result.stdout + result.stderr
    generated = generated_path.read_text(encoding="utf-8")
    assert f"set_sparse_floor_pwm({floor})" in generated
    sparse = str(options.get("sparse_dimming", False)).lower()
    simple_encoder = str(not options.get("rmt_preencode", False)).lower()
    assert f"set_sparse_dimming({sparse})" in generated
    assert f"set_use_simple_encoder({simple_encoder})" in generated
    assert f"set_sparse_max_step({options.get('sparse_max_step', 8)})" in generated


@pytest.mark.parametrize("floor", [0, 256])
def test_out_of_range_floor_is_rejected(tmp_path, floor):
    result, _ = generate(tmp_path, {"sparse_floor_pwm": floor})
    assert result.returncode != 0
    assert "sparse_floor_pwm" in result.stdout + result.stderr
