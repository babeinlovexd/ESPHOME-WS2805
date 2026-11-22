"""Compile native sparse regressions, including verbatim production driver methods."""
import ast
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
component = root / "components/ws2805"
source = (component / "ws2805_light.cpp").read_text(encoding="utf-8")
header = (component / "ws2805_light.h").read_text(encoding="utf-8")


def extract(name):
    match = re.search(r"^(?:void|uint8_t|bool) WS2805LightOutput::" + name + r"\(", source, re.M)
    if not match:
        raise RuntimeError(f"Production method missing: {name}")
    end = source.index("{", match.start())
    depth = 1
    while depth:
        end += 1
        depth += (source[end] == "{") - (source[end] == "}")
    return source[match.start():end + 1]


methods = "\n\n".join(extract(name) for name in (
    "write_state", "read_target_values_", "update_white_transition_", "step_channel_",
    "compute_white_bytes_", "calculate_dither_", "clear_white_state_", "fill_led_buffer_", "sparse_fill_",
))
fields = header[header.index("  bool color_interlock_"):header.index("  // Configurable RMT")]
template = (Path(__file__).with_name("sparse_state_harness.cpp.in")).read_text(encoding="utf-8")
with tempfile.TemporaryDirectory(prefix="ws2805-sparse-") as folder:
    temporary = Path(folder)
    state_source = temporary / "state.cpp"
    state_source.write_text(template.replace("@@FIELDS@@", fields).replace("@@METHODS@@", methods), encoding="utf-8")
    timing_template = Path(__file__).with_name("timing_harness.cpp.in").read_text(encoding="utf-8")
    config_fields = header[header.index("  uint32_t bit0_high_ns_"):header.index("  uint8_t isr_priority_")]
    start = header.index("  int tx_timeout_ms_() const {")
    end = header.index("\n  }", start) + len("\n  }")
    defaults = {}
    for node in ast.walk(ast.parse((component / "light.py").read_text(encoding="utf-8"))):
        if (isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
                and node.func.attr == "Optional" and node.args
                and isinstance(node.args[0], ast.Constant) and isinstance(node.args[0].value, str)):
            for keyword in node.keywords:
                if keyword.arg == "default":
                    defaults[node.args[0].value] = ast.literal_eval(keyword.value)
    timing_source = temporary / "timing.cpp"
    timing_source.write_text(timing_template.replace("@@CONFIG_FIELDS@@", config_fields)
                             .replace("@@TIMEOUT@@", header[start:end])
                             .replace("@@TIMING@@", extract("calc_timing_"))
                             .replace("@@SCHEMA_TIMINGS@@", ",".join(str(defaults[key]) for key in
                                      ("bit0_high_ns", "bit0_low_ns", "bit1_high_ns", "bit1_low_ns", "reset_pulse_us"))),
                             encoding="utf-8")
    cases = [("curve", Path(__file__).with_name("test_sparse_dimming.cpp"), []),
             ("state", state_source, []), ("timing", timing_source, [])]
    esphome_include = os.getenv("ESPHOME_INCLUDE_DIR")
    if not esphome_include:
        spec = importlib.util.find_spec("esphome")
        if spec and spec.origin:
            esphome_include = str(Path(spec.origin).resolve().parents[1])
    if esphome_include:
        cases.append(("real_state", state_source, ["USE_REAL_LIGHT_VALUES"]))
    elif os.getenv("REQUIRE_REAL_LIGHT_VALUES"):
        raise RuntimeError("ESPHome headers are required for the real-value state test")
    for name, test_source, defines in cases:
        includes = [str(component)]
        if defines:
            includes.extend([str(root / "tests/light_values_mocks"), esphome_include])
        if os.name == "nt":
            executable = temporary / f"{name}.exe"
            command = ["cl", "/nologo", "/EHsc", "/std:c++20" if defines else "/std:c++17", "/W4", *["/I"+p for p in includes],
                       *["/D"+d for d in defines],
                       str(test_source), f"/Fe:{executable}", f"/Fo:{temporary / (name + '.obj')}"]
        else:
            executable = temporary / name
            command = [os.getenv("CXX", "g++"), "-std=c++20" if defines else "-std=c++17", "-Wall", "-Wextra", "-Werror",
                       "-fsanitize=address,undefined", *["-I"+p for p in includes], *["-D"+d for d in defines],
                       str(test_source), "-o", str(executable)]
        subprocess.run(command, check=True, cwd=temporary)
        subprocess.run([str(executable)], check=True)
