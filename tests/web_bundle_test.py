# SPDX-License-Identifier: MIT
"""Check that any asset update moves the entire browser dependency graph."""

from pathlib import Path
import hashlib
import json
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

    # Documentation media participates in deployment without changing games.
    media = root / 'docs/videos'
    media.mkdir(parents=True)
    film = media / 'demo.mp4'
    film.write_bytes(b'project-created video fixture')
    manifest = source / 'site-media.json'

    def media_entry(source_path='docs/videos/demo.mp4', target='videos/demo-1.mp4'):
        return {'source': source_path, 'target': target,
                'sha256': hashlib.sha256(film.read_bytes()).hexdigest()}

    def set_media(entries):
        manifest.write_text(json.dumps({'version': 1, 'files': entries}))

    set_media([media_entry()])
    build()
    current = published_assets()
    assert current != previous
    assert (output / 'videos/demo-1.mp4').read_bytes() == film.read_bytes()
    build()
    assert published_assets() == current
    original = film.read_bytes()
    film.write_bytes(b'new video')
    entry_before_failure = (output / 'index.html').read_bytes()
    assert build(check=False).returncode != 0  # Stale manifest must fail closed.
    assert (output / 'index.html').read_bytes() == entry_before_failure
    assert (output / 'videos/demo-1.mp4').read_bytes() == original
    set_media([media_entry(target='videos/demo-2.mp4')])
    build()
    assert published_assets() != current
    assert (output / 'videos/demo-1.mp4').read_bytes() == original
    assert (output / 'videos/demo-2.mp4').read_bytes() == film.read_bytes()
    entry_before_failure = (output / 'index.html').read_bytes()
    for entry in [media_entry(source_path='../source/private.rom'),
                  media_entry(source_path='source/private.rom'),
                  media_entry(source_path='docs/videos/../../source/private.rom'),
                  media_entry(target='../escaped.mp4'),
                  media_entry(target='/tmp/escaped.mp4')]:
        set_media([entry])
        assert build(check=False).returncode != 0
        assert (output / 'index.html').read_bytes() == entry_before_failure
    set_media([media_entry(), media_entry()])
    assert build(check=False).returncode != 0
    assert not list(output.rglob('*.rom'))

print("Web bundle update, atomic entry, retained revision, media hash and asset scope checks passed")
