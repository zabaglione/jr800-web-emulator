# SPDX-License-Identifier: MIT
"""Check that any asset update moves the entire browser dependency graph."""

from pathlib import Path
import re
import subprocess
import sys
import tempfile


builder = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    source = root / "source"
    source.mkdir()
    index = source / "index.html"
    index.write_text(
        '<link rel="stylesheet" href="./styles.css">'
        '<script type="module" src="./app.mjs"></script>',
        encoding="utf-8",
    )
    names = (
        "app.mjs", "styles.css", "worker.mjs", "adapter.mjs",
        "module.mjs", "module.wasm", "locale.json",
    )
    assets = [source / name for name in names]
    for asset in assets:
        asset.write_bytes(asset.name.encode())
    (source / "private.rom").write_bytes(b"private fixture; never an asset")
    output = root / "site"

    def build(paths=assets, check=True):
        return subprocess.run(
            [sys.executable, str(builder), "--index", str(index),
             "--output", str(output), *map(str, paths)],
            check=check, capture_output=True, text=True,
        )

    def published_assets():
        html = (output / "index.html").read_text(encoding="utf-8")
        script = re.search(r'src="([^"]+)"', html).group(1)
        style = re.search(r'href="([^"]+)"', html).group(1)
        assert Path(script).parent == Path(style).parent
        directory = output / Path(script).parent
        assert sorted(path.name for path in directory.iterdir()) == sorted(names)
        for asset in assets:
            assert (directory / asset.name).read_bytes() == asset.read_bytes()
        return directory

    build()
    previous = published_assets()
    entry_time = (output / "index.html").stat().st_mtime_ns
    build(list(reversed(assets)))
    assert published_assets() == previous
    assert (output / "index.html").stat().st_mtime_ns == entry_time

    # A WASM-only or transitive-module update must invalidate every asset URL.
    for asset in [*reversed(assets), index]:
        prior_content = {p.name: p.read_bytes() for p in previous.iterdir()}
        with asset.open("ab") as file:
            file.write(b"\nupdated")
        build()
        current = published_assets()
        assert current != previous
        # Pages that were already open retain a complete, unchanged revision.
        assert {p.name: p.read_bytes() for p in previous.iterdir()} == prior_content
        previous = current

    entry_before_failure = (output / "index.html").read_bytes()
    assert build([*assets, source / "missing.mjs"], check=False).returncode != 0
    assert (output / "index.html").read_bytes() == entry_before_failure
    assert not list(output.rglob("*.rom"))

print("Web bundle update, atomic entry, retained revision, and asset scope checks passed")
