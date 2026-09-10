# SPDX-License-Identifier: MIT
"""Build author-owned RAM programs and a matching, digest-checked catalog."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
root=Path(__file__).resolve().parents[1]
a.output.mkdir(parents=True,exist_ok=True)
programs=json.loads((root/'games/catalog.json').read_text())['programs']
tools=root/'build/native-release/tools'
if not (tools/'jr8as').exists() or not (tools/'jr8ld').exists() or not (root/'build/native-release/tests/game_replay_test').exists():
    subprocess.run(['cmake','--preset','native-release'],cwd=root,check=True)
    subprocess.run(['cmake','--build','--preset','native-release','--target','jr8as','jr8ld','game_replay_test'],cwd=root,check=True)
seen=set()
for program in programs:
    ident=program['id']
    if not re.fullmatch(r'[a-z][a-z0-9-]{0,39}',ident) or ident in seen:raise ValueError('Invalid or duplicate program ID')
    seen.add(ident)
    subprocess.run(['make','--no-print-directory','-s','-C',str(root/'games'/ident)],check=True)
    file=ident+'.j8a';data=(root/'build/games'/ident/file).read_bytes()
    target=a.output/file
    if not target.exists() or target.read_bytes()!=data:target.write_bytes(data)
    program.update(file=file,sha256=hashlib.sha256(data).hexdigest(),source=f'https://github.com/zabaglione/jr800-web-emulator/tree/main/games/{ident}')
encoded=json.dumps({'version':1,'programs':programs},indent=2)+'\n'
catalog=a.output/'program-catalog.json'
if not catalog.exists() or catalog.read_text()!=encoded:catalog.write_text(encoded)
