# SPDX-License-Identifier: MIT
"""Import reviewed recordings and refresh the explicit Web publication list."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'docs/games/videos'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, data):
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n')


def publish(recordings, selected):
    games = json.loads((ROOT / 'games/catalog.json').read_text())['programs']
    known = {g['id'] for g in games}
    if selected and not set(selected) <= known:
        raise ValueError('Unknown game ID')
    prior = DEST / 'catalog.json'
    entries = {g['id']: g for g in json.loads(prior.read_text())['games']} if prior.exists() else {}
    validated = []
    for game in games:
        ident = game['id']
        if selected and ident not in selected:
            continue
        source = recordings / ident / (ident + '.mp4')
        report = json.loads((recordings / ident / 'recording.json').read_text())
        probe = json.loads(subprocess.check_output([
            'ffprobe', '-v', 'error', '-show_streams', '-show_format', '-of', 'json', str(source)]))
        video, = [s for s in probe['streams'] if s['codec_type'] == 'video']
        audio, = [s for s in probe['streams'] if s['codec_type'] == 'audio']
        assert report['id'] == ident and report['bootstrap'] == 'project-authored'
        assert report['playbackSpeed'] == 1 and report['clockHz'] == 1228800
        assert report['frames'] == report['duration'] * 30 and report['distinctFrames'] >= 15
        assert report['speakerEdges'] > 0
        assert report['applicationSha256'] == sha(ROOT / 'build/games' / ident / (ident + '.j8a'))
        assert abs(float(probe['format']['duration']) - report['duration']) < 0.1
        assert video['codec_name'] == 'h264' and video['pix_fmt'] == 'yuv420p'
        assert (video['width'], video['height'], video['r_frame_rate']) == (800, 288, '30/1')
        assert audio['codec_name'] == 'aac' and audio['channels'] == 1 and audio['sample_rate'] == '48000'
        digest = sha(source)
        entries[ident] = {**game, 'duration': report['duration'],
                          'file': f'{ident}-{digest[:16]}.mp4', 'sha256': digest,
                          'applicationSha256': report['applicationSha256']}
        validated.append((source, DEST / (ident + '.mp4')))
    if set(entries) != known:
        raise ValueError('Record every catalog game before publishing the gallery')
    # No files are replaced until every selected recording passes validation.
    DEST.mkdir(parents=True, exist_ok=True)
    for source, target in validated:
        shutil.copyfile(source, target)
    write_json(prior, {'version': 1, 'games': [entries[g['id']] for g in games]})
    files = [{'source': f'docs/games/videos/{g["id"]}.mp4',
              'target': 'videos/' + entries[g['id']]['file'], 'sha256': entries[g['id']]['sha256']}
             for g in games]
    for name in ('index.html', 'catalog.json'):
        files.append({'source': f'docs/games/videos/{name}', 'target': f'videos/{name}',
                      'sha256': sha(DEST / name)})
    write_json(ROOT / 'web/site-media.json', {'version': 1, 'files': files})
    print(f'Published {len(validated)} recordings; gallery contains {len(entries)} games')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--recordings', type=Path, default=ROOT / 'build/game-videos-review')
    parser.add_argument('--game', action='append', help='Import only these games and retain the others')
    args = parser.parse_args()
    publish(args.recordings.resolve(), args.game)
