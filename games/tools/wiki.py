# SPDX-License-Identifier: MIT
"""Build genre-indexed Japanese manuals from the working game catalog."""
import argparse,json,re,shutil
from pathlib import Path
from puzzle_assets import PUZZLES
from wiki_index import title_image_path,title_image_url,video_section,write_index
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--live-check',type=Path)
p.add_argument('--image-revision',default='main',help='Published revision for gameplay captures; titles always use the shared current image')
p.add_argument('--game',action='append',help='Update only these game pages and captures')
p.add_argument('--index-only',action='store_true',help='Update player gallery, genre lists, and controls only')
a=p.parse_args()
site='https://zabaglione.github.io/jr800-web-emulator/'
repo='https://github.com/zabaglione/jr800-web-emulator'
if not re.fullmatch(r'[0-9a-f]{40}|main',a.image_revision):raise ValueError('Use a full published commit ID for images')
raw=f'https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/{a.image_revision}/docs/games/screenshots'
games=json.loads((root/'games/catalog.json').read_text())['programs']
genres=json.loads((root/'games/genres.json').read_text())
plan=json.loads((root/'games/roadmap.json').read_text())
by_genre={g['id']:g for g in genres}
assert len({g['id'] for g in games})==len(games)
assert all(g['genre'] in by_genre for g in games)
if a.game and not set(a.game)<=set(g['id'] for g in games):raise ValueError('Unknown game ID')
verified=set()
if a.live_check:
 check=json.loads(a.live_check.read_text())
 if check.get('passed') is not True or check.get('siteUrl')!=site:raise ValueError('A successful public-site check is required')
 verified=set(check['programs'])
if a.index_only:
 if a.game:raise ValueError('Choose index-only or selected game pages')
 write_index(root,games,genres,verified,site,repo)
 print(f'Generated gallery indexes for {len(games)} games')
 raise SystemExit(0)
common=(root/'games/tools/manual-common.md').read_text()
puzzle_common=(root/'games/tools/manual-puzzle.md').read_text()
wiki=root/'docs/games/wiki';wiki.mkdir(parents=True,exist_ok=True)
def genre_page(g):return 'Genre-'+g['id'].title()
def style(g):return '定番' if g['style']=='classic' else 'モダン'
def wiki_link(name):return repo+'/wiki/'+name
for game in games:
 ident=game['id'];title=game['title'];genre=by_genre[game['genre']]
 if a.game and ident not in a.game:continue
 manual=json.loads((root/'games'/ident/'manual.json').read_text())
 source=f'{repo}/tree/main/games/{ident}'
 launch=f'[遊ぶ]({site}?program={ident}) · ' if ident in verified else ''
 page=f'# {title}\n\n[ゲーム一覧]({repo}/wiki) / [{genre["title"]}]({wiki_link(genre_page(genre))}) · {style(game)}\n\n{launch}[ビルド可能なソース]({source})\n\n'
 page+=f'![タイトル画面]({title_image_url(ident)})\n\n{manual["summary"]}\n\n## 遊び方\n\n{manual["guide"]}\n\n## ゲーム画面\n\n'
 assert len(manual['captions'])==3
 for i,caption in enumerate(manual['captions'],1):page+=f'![{caption}]({raw}/{ident}/gameplay-{i}.png)\n\n{caption}。\n\n'
 if 'motion_caption' in manual:
  caption=manual['motion_caption'];page+=f'![{caption}]({raw}/{ident}/motion.png)\n\n{caption}。\n\n'
 if 'animation' in manual:
  animation=manual['animation'];file=animation['file'];caption=animation['caption']
  assert re.fullmatch(r'[a-z-]+\.gif',file)
  page+=f'![{caption}]({raw}/{ident}/{file})\n\n{caption}。\n\n'
 page+=video_section(root,ident,site)
 if ident in PUZZLES:
  page+=puzzle_common+f'\n![面選択とパスワード]({raw}/{ident}/selection.png)\n\n面選択では規定手数、追加目標、BEST、2種類のパスワードを確認できます。\n\n'
 page+=common+f'\n## ビルド\n\n```sh\nmake -C games/{ident}\nmake -C games/{ident} test\n```\n\n環境の準備・一括ビルドは[Games README]({repo}/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。\n'
 (wiki/(ident.upper()+'.md')).write_text(page)
 (root/'games'/ident/'README.md').write_text(page.replace(title_image_url(ident),'../../'+title_image_path(ident)).replace(raw+'/'+ident,'../../docs/games/screenshots/'+ident))
 title=root/title_image_path(ident);title.parent.mkdir(parents=True,exist_ok=True)
 src=root/'build/games'/ident/'title.png'
 if src.exists():shutil.copyfile(src,title)
 elif not title.exists():raise FileNotFoundError(f'Capture the actual title first: {ident}/title.png')
 dest=root/'docs/games/screenshots'/ident;dest.mkdir(parents=True,exist_ok=True)
 for name in ['gameplay-1','gameplay-2','gameplay-3']+(['selection'] if ident in PUZZLES else [])+(['motion'] if 'motion_caption' in manual else []):
  for suffix in ['', '-1x']:
   image=name+suffix+'.png';src=root/'build/games'/ident/image
   if src.exists():shutil.copyfile(src,dest/image)
   elif not (dest/image).exists():raise FileNotFoundError(f'Capture actual gameplay first: {ident}/{image}')
 if 'animation' in manual:
  file=manual['animation']['file'];src=root/'build/games'/ident/file
  if src.exists():shutil.copyfile(src,dest/file)
  elif not (dest/file).exists():raise FileNotFoundError(f'Capture actual gameplay first: {ident}/{file}')
if a.game:
 print(f'Generated {len(set(a.game))} selected game pages')
 raise SystemExit(0)
write_index(root,games,genres,verified,site,repo)
roadmap='# ゲーム開発一覧\n\n「収録済み」は実装・検証対象の作品です。「制作予定」は未収録で、遊べる作品数には数えません。各作品の公開サイト確認後に起動リンクを掲載します。\n'
known={g['id'] for g in games}
for genre in genres:
 planned=[g for g in plan['programs'] if g['genre']==genre['id']]
 roadmap+=f'\n## {genre["title"]}\n\n| タイトル | 系統 | 状態 | 実装範囲 |\n|---|---|---|---|\n'
 for game in planned:roadmap+=f'| {game["title"]} | {style(game)} | {"収録済み" if game["id"] in known else "制作予定"} | {game["summary"]} |\n'
(wiki/'Puzzle-Challenges.md').write_text('# パズルチャレンジ\n\n'+puzzle_common+'\n## 対象の15作品\n\n'+'\n'.join('- ['+g['title']+']('+wiki_link(g['id'].upper())+')' for g in games if g['id'] in PUZZLES)+'\n')
(root/'docs/games/roadmap.md').write_text(roadmap)
print(f'Generated {len(games)} game manuals and {len(genres)} genre pages; {len(verified & known)} verified launch links')
