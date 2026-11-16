"""Compile a CI copy with dummy credentials; never flash or use private secrets."""
import base64
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import yaml

root = Path(__file__).resolve().parents[1]
config = sys.argv[1]
with tempfile.TemporaryDirectory(prefix="esphome-ci-") as temp:
    checkout = Path(temp) / "source"
    shutil.copytree(root, checkout, ignore=shutil.ignore_patterns(
        ".git", ".esphome", "secrets.yaml", "secrets.yml", ".pio", "build", "node_modules", "release-output"))
    for path in checkout.rglob("*.yaml"):
        if ".github" in path.parts:
            continue
        names = set(re.findall(r"!secret\s+([\w-]+)", path.read_text(encoding="utf-8-sig")))
        if not names:
            continue
        secret_file = path.parent / "secrets.yaml"
        secrets = yaml.safe_load(secret_file.read_text()) if secret_file.exists() else {}
        for name in names:
            secrets[name] = (base64.b64encode(bytes(32)).decode() if name == "api_encryption_key"
                             else '"0000000000"' if name == "master_tag_id"
                             else "ci-placeholder-only")
        secret_file.write_text(yaml.safe_dump(secrets), encoding="utf-8")
    # Keep user-owned external components at the exact checkout under test.
    path = checkout / config
    text = path.read_text(encoding="utf-8-sig")
    repo = os.getenv("GITHUB_REPOSITORY", "").split("/")[-1]
    components = checkout / "components"
    if repo and components.is_dir():
        pattern = (r"(?m)^(\s*)type: git\n\s*url: https://github.com/babeinlovexd/"
                   + re.escape(repo) + r"(?:\.git)?\s*\n(?:\s*ref: [^\n]+\n)?")
        text = re.sub(pattern, lambda m: f"{m[1]}type: local\n{m[1]}path: {components.as_posix()}\n", text)
        path.write_text(text, encoding="utf-8")
    subprocess.run([sys.executable, "-m", "esphome", "compile", str(path)], cwd=checkout, check=True)
