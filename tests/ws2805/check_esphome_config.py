"""Exercise the external component through ESPHome's real schema and code generator."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import yaml


COMPONENTS = Path(__file__).resolve().parents[2] / "components"


class WS2805ConfigTests(unittest.TestCase):
    def check_config(self, light, *, board="esp32dev", error=None, generated_line=None):
        config = {
            "esphome": {"name": "ws2805-schema-test"},
            "esp32": {"board": board, "framework": {"type": "esp-idf"}},
            "external_components": [
                {"source": {"type": "local", "path": COMPONENTS.as_posix()}, "components": ["ws2805"]}
            ],
            "light": [{"platform": "ws2805", "name": "Test strip", "pin": "GPIO18", "num_leds": 12, **light}],
        }
        with tempfile.TemporaryDirectory(prefix="ws2805-schema-") as temporary:
            path = Path(temporary) / "test.yaml"
            path.write_text(yaml.safe_dump(config, sort_keys=False), encoding="utf-8")
            command = [sys.executable, "-m", "esphome"]
            command += ["config", str(path)] if generated_line is None else ["compile", str(path), "--only-generate"]
            result = subprocess.run(command, capture_output=True, text=True, timeout=45)
            output = result.stdout + result.stderr
            if error is not None:
                self.assertNotEqual(result.returncode, 0, output)
                self.assertIn(error, output)
            else:
                self.assertEqual(result.returncode, 0, output)
                if generated_line is not None:
                    source = next(Path(temporary).rglob("main.cpp")).read_text(encoding="utf-8")
                    self.assertIn(generated_line, source)

    def test_rmt_capacity_with_backup_output(self):
        for board in ("esp32-c3-devkitm-1", "esp32-c6-devkitc-1"):
            with self.subTest(board=board):
                self.check_config({"pin": "GPIO4", "fdin_pin": "GPIO5"}, board=board,
                                  error="cannot allocate 2 contiguous RMT block(s)")
        self.check_config({"pin": "GPIO4", "fdin_pin": "GPIO5"}, board="esp32-s3-devkitc-1",
                          generated_line="->set_fdin_pin(5)")

    def test_rejects_unsupported_options(self):
        self.check_config({"power_estimation": {"max_current_ma": 100}}, error="does not support power_estimation")
        self.check_config({"pin": {"number": "GPIO18", "inverted": True}}, error="requires a non-inverted data pin")
        self.check_config({"bit0_high_ns": 0x100000000}, error="4294967295")

    def test_legacy_transition_alias(self):
        self.check_config({"cct_transition_speed": "3s"}, generated_line="->set_default_transition_length(3000)")
        self.check_config({"cct_transition_speed": "3s", "default_transition_length": "0s"},
                          generated_line="->set_default_transition_length(0)")


if __name__ == "__main__":
    unittest.main()
