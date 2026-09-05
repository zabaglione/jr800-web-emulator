# SPDX-License-Identifier: MIT
"""Publish one content-addressed set of browser assets and its entry page."""

import argparse
import hashlib
from pathlib import Path
import tempfile


def build_bundle(index_path: Path, output: Path, asset_paths: list[Path]) -> None:
    assets = {path.name: path.read_bytes() for path in asset_paths}
    if len(assets) != len(asset_paths):
        raise ValueError("Web assets must have unique filenames")
    index = index_path.read_text(encoding="utf-8")
    for name in ("app.mjs", "styles.css"):
        if name not in assets or index.count(f'"./{name}"') != 1:
            raise ValueError(f"Missing or ambiguous Web entry: {name}")

    digest = hashlib.sha256()
    for name, content in sorted({"index.html": index.encode(), **assets}.items()):
        digest.update(name.encode() + b"\0")
        digest.update(len(content).to_bytes(8, "big"))
        digest.update(content)
    revision = digest.hexdigest()
    asset_root = output / "assets"
    destination = asset_root / revision
    asset_root.mkdir(parents=True, exist_ok=True)
    if not destination.exists():
        # Publish the complete directory before the entry page can refer to it.
        with tempfile.TemporaryDirectory(dir=asset_root, prefix=".building-") as staging:
            staged = Path(staging) / revision
            staged.mkdir()
            for name, content in assets.items():
                (staged / name).write_bytes(content)
            staged.rename(destination)

    for name in ("app.mjs", "styles.css"):
        index = index.replace(f'"./{name}"', f'"./assets/{revision}/{name}"')
    entry = output / "index.html"
    if not entry.exists() or entry.read_text(encoding="utf-8") != index:
        with tempfile.NamedTemporaryFile(dir=output, mode="w", encoding="utf-8", delete=False) as staged:
            staged.write(index)
            temporary_entry = Path(staged.name)
        temporary_entry.chmod(0o644)
        temporary_entry.replace(entry)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--index", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("assets", type=Path, nargs="+")
    args = parser.parse_args()
    build_bundle(args.index, args.output, args.assets)
