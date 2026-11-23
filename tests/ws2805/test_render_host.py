"""Compile the WS2805 driver with ESPHome light code and an in-process RMT fixture.

Run directly with a pinned ESPHome wheel and a native C++20 compiler.
This tests the renderer and encoder without an ESP32 or Home Assistant server.
"""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TESTS = Path(__file__).resolve().parent
MOCKS = ROOT / "tests" / "host_mocks"
COMPONENT = ROOT / "components" / "ws2805"
LIGHT_SOURCES = (
    "addressable_light.cpp",
    "esp_color_correction.cpp",
    "esp_color_view.cpp",
    "esp_range_view.cpp",
    "esp_hsv_color.cpp",
    "light_output.cpp",
    "light_color_values.cpp",
)
CORE_HEADERS = ("color.h", "finite_set_mask.h")
CORE_STUBS = ("component.h", "defines.h", "hal.h", "helpers.h", "log.h", "version.h")
IDF_STUBS = (
    "driver/gpio.h",
    "driver/rmt_tx.h",
    "esp_clk_tree.h",
    "esp_err.h",
    "esp_heap_caps.h",
    "esp_idf_version.h",
    "esp_private/rmt.h",
    "esp_timer.h",
    "soc/soc_caps.h",
)


def write_header(path, fixture, extra=""):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(f'#pragma once\n#include "{fixture}"\n{extra}', encoding="utf-8")


def copy_upstream(build, package, version):
    # Copy the light sources to a temporary overlay so their quoted includes can
    # use the fixture's clock, LightState and scheduler stubs.
    upstream = build / "esphome"
    for component in ("light", "partition"):
        shutil.copytree(
            package / "components" / component,
            upstream / "components" / component,
            ignore=shutil.ignore_patterns("*.py", "__pycache__"),
        )

    core = upstream / "core"
    core.mkdir()
    for name in (*CORE_HEADERS, "color.cpp"):
        shutil.copyfile(package / "core" / name, core / name)

    version = version.replace(".", ", ")
    for name in CORE_STUBS:
        version_define = (
            f"#define ESPHOME_VERSION_CODE VERSION_CODE({version})\n"
            if name == "version.h"
            else ""
        )
        write_header(core / name, "esphome_runtime.h", version_define)
    write_header(upstream / "components/power_supply/power_supply.h", "esphome_runtime.h")
    for name in IDF_STUBS:
        write_header(build / name, "idf_runtime.h")

    light = upstream / "components/light"
    partition = upstream / "components/partition"
    shutil.copyfile(MOCKS / "light_state.h", light / "light_state.h")

    # Use the exact ESPHome CWWW conversion without linking the entity and
    # preference subsystems that the renderer tests do not exercise.
    state = (light / "light_state.cpp").read_text(encoding="utf-8")
    start = state.index("void LightState::current_values_as_cwww(")
    end = state.index("\n}", start) + 2
    white_conversion = light / "host_white_conversion.cpp"
    white_conversion.write_text(
        '#include "light_state.h"\nnamespace esphome::light {\n'
        + state[start:end]
        + "\n}\n",
        encoding="utf-8",
    )
    sources = [light / name for name in LIGHT_SOURCES if (light / name).exists()]
    return [core / "color.cpp", partition / "light_partition.cpp", white_conversion, *sources]


def main():
    import esphome
    from esphome.const import __version__

    print(f"Testing WS2805 with ESPHome {__version__}", flush=True)
    package = Path(esphome.__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix="ws2805-host-") as directory:
        build = Path(directory)
        sources = copy_upstream(build, package, __version__) + [
            COMPONENT / "ws2805_light.cpp",
            TESTS / "test_ws2805.cpp",
            TESTS / "test_rmt_recovery.cpp",
            TESTS / "test_rmt_timing.cpp",
        ]
        if os.name == "nt":
            executable = build / "ws2805-tests.exe"
            compiler = os.environ.get("CXX", "cl")
            includes = [f"/I{path}" for path in (build, MOCKS, COMPONENT)]
            command = [compiler, "/nologo", "/EHsc", "/std:c++20", *includes, *sources, f"/Fe:{executable}"]
        else:
            executable = build / "ws2805-tests"
            compiler = os.environ.get("CXX", "g++")
            includes = [f"-I{path}" for path in (build, MOCKS, COMPONENT)]
            command = [compiler, "-std=c++20", *includes, *sources, "-o", executable]
        subprocess.run(command, cwd=build, check=True)
        subprocess.run([executable], cwd=build, check=True)


if __name__ == "__main__":
    main()
