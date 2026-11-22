"""Compile native sparse regressions, including verbatim production driver methods."""
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
    match = re.search(r"^(?:void|uint8_t) WS2805LightOutput::" + name + r"\(", source, re.M)
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
fields = header[header.index("  bool color_interlock_"):header.index("  // Configurable RMT bit timing")]
template = (Path(__file__).with_name("sparse_state_harness.cpp.in")).read_text(encoding="utf-8")
with tempfile.TemporaryDirectory(prefix="ws2805-sparse-") as folder:
    temporary = Path(folder)
    state_source = temporary / "state.cpp"
    state_source.write_text(template.replace("@@FIELDS@@", fields).replace("@@METHODS@@", methods), encoding="utf-8")
    for name, test_source in (("curve", Path(__file__).with_name("test_sparse_dimming.cpp")), ("state", state_source)):
        if os.name == "nt":
            executable = temporary / f"{name}.exe"
            command = ["cl", "/nologo", "/EHsc", "/std:c++17", "/W4", f"/I{component}",
                       str(test_source), f"/Fe:{executable}", f"/Fo:{temporary / (name + '.obj')}"]
        else:
            executable = temporary / name
            command = [os.getenv("CXX", "g++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                       "-fsanitize=address,undefined", "-I" + str(component), str(test_source), "-o", str(executable)]
        subprocess.run(command, check=True, cwd=temporary)
        subprocess.run([str(executable)], check=True)
