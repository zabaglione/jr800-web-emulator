# SPDX-License-Identifier: MIT
"""Publish one content-addressed set of browser assets and its entry page."""

import argparse
import hashlib
import json
from pathlib import Path
import tempfile


def site_media(index_path: Path) -> dict[str, bytes]:
    """Read an explicit, hash-checked publication list; never discover files."""
    manifest = index_path.parent / "site-media.json"
    if not manifest.exists():
        return {}
    root = index_path.parent.parent.resolve()
    media = {}
    for item in json.loads(manifest.read_text(encoding="utf-8"))["files"]:
        source, target = Path(item["source"]), Path(item["target"])
        if (source.is_absolute() or target.is_absolute() or
                ".." in source.parts or ".." in target.parts or
                len(source.parts) < 2 or source.parts[0] != "docs" or
                len(target.parts) < 2 or target.parts[0] != "videos" or
                target.as_posix() in media or
                source.suffix not in {".html", ".json", ".mp4"} or
                source.suffix != target.suffix):
            raise ValueError("Invalid site media path")
        resolved = (root / source).resolve()
        if not resolved.is_relative_to(root / "docs"):
            raise ValueError("Site media escapes the documentation directory")
        content = resolved.read_bytes()
        if hashlib.sha256(content).hexdigest() != item["sha256"]:
            raise ValueError(f"Site media hash mismatch: {source}")
        media[target.as_posix()] = content
    return media


def publish_file(path: Path, content: bytes) -> None:
    if path.exists() and path.read_bytes() == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as staged:
        staged.write(content)
        temporary = Path(staged.name)
    temporary.chmod(0o644)
    temporary.replace(path)


def build_bundle(index_path: Path, output: Path, asset_paths: list[Path]) -> None:
    assets = {path.name: path.read_bytes() for path in asset_paths}
    media = site_media(index_path)
    if len(assets) != len(asset_paths):
        raise ValueError("Web assets must have unique filenames")
    index = index_path.read_text(encoding="utf-8")
    for name in ("app.mjs", "styles.css"):
        if name not in assets or index.count(f'"./{name}"') != 1:
            raise ValueError(f"Missing or ambiguous Web entry: {name}")

    digest = hashlib.sha256()
    for name, content in sorted({"index.html": index.encode(), **assets, **media}.items()):
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

    # Publish video bytes before catalogs and entry pages can point to them.
    for name in sorted(media, key=lambda name: (Path(name).suffix != ".mp4", name)):
        publish_file(output / name, media[name])
    for name in ("app.mjs", "styles.css"):
        index = index.replace(f'"./{name}"', f'"./assets/{revision}/{name}"')
    publish_file(output / "index.html", index.encode())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--index", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("assets", type=Path, nargs="+")
    args = parser.parse_args()
    build_bundle(args.index, args.output, args.assets)
