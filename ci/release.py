"""Check repository version and package tracked source without local secrets."""
import ast
import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys
import zipfile

root = Path(__file__).resolve().parents[1]
os.chdir(root)
version = (root / "VERSION").read_text().strip()
if not re.fullmatch(r"\d+\.\d+\.\d+", version):
    raise SystemExit("VERSION must contain major.minor.patch")
if os.getenv("GITHUB_REF_TYPE") == "tag" and os.getenv("GITHUB_REF_NAME") != "v" + version:
    raise SystemExit("Release tag must equal v + VERSION")
files = subprocess.check_output(["git", "ls-files", "-z"]).decode().split("\0")
files = [f for f in files if f and Path(f).is_file()]
for file in files:
    if file.endswith(".py"):
        ast.parse(Path(file).read_text(encoding="utf-8-sig"), filename=file)
if "--package" in sys.argv:
    name = os.getenv("GITHUB_REPOSITORY", "local/project").split("/")[-1]
    folder = f"{name}-v{version}"
    out = root / "release-output"
    out.mkdir(exist_ok=True)
    target = out / f"{folder}-source.zip"
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as archive:
        for file in files:
            path = Path(file)
            if path.name.lower() in {"secrets.yaml", "secrets.yml", ".env"}:
                continue
            if any(part in {".git", "__pycache__", ".pio", "node_modules", "release-output"} for part in path.parts):
                continue
            archive.write(path, f"{folder}/{file}")
    with zipfile.ZipFile(target) as archive:
        if archive.testzip() is not None:
            raise SystemExit("Invalid source archive")
    digest = hashlib.sha256(target.read_bytes()).hexdigest()
    (out / "SHA256SUMS.txt").write_text(f"{digest}  {target.name}\n", encoding="ascii")
    print(f"Verified source package: {target.name}")
else:
    print(f"Version and Python syntax OK: {version}")
