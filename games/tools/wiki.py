# SPDX-License-Identifier: MIT
"""Build genre-indexed Japanese manuals from the working game catalog."""
import argparse,json,re,shutil
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--live-check',type=Path)
a=p.parse_args()
site='https://zabaglione.github.io/jr800-web-emulator/'
repo='https://github.com/zabaglione/jr800-web-emulator'
raw='https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots'
games=json.loads((root/'games/catalog.json').read_text())['programs']
genres=json.loads((root/'games/genres.json').read_text())
plan=json.loads((root/'games/roadmap.json').read_text())
by_genre={g['id']:g for g in genres}
assert len({g['id'] for g in games})==len(games)
assert all(g['genre'] in by_genre for g in games)
verified=set()
if a.live_check:
 check=json.loads(a.live_check.read_text())
 if check.get('passed') is not True or check.get('siteUrl')!=site:raise ValueError('A successful public-site check is required')
 verified=set(check['programs'])
common=(root/'games/tools/manual-common.md').read_text()
wiki=root/'docs/games/wiki';wiki.mkdir(parents=True,exist_ok=True)
def genre_page(g):return 'Genre-'+g['id'].title()
def style(g):return '定番' if g['style']=='classic' else 'モダン'
def wiki_link(name):return repo+'/wiki/'+name
for game in games:
 ident=game['id'];title=game['title'];genre=by_genre[game['genre']]
 manual=json.loads((root/'games'/ident/'manual.json').read_text())
 source=f'{repo}/tree/main/games/{ident}'
 launch=f'[遊ぶ]({site}?program={ident}) · ' if ident in verified else ''
 page=f'# {title}\n\n[ゲーム一覧]({repo}/wiki) / [{genre["title"]}]({wiki_link(genre_page(genre))}) · {style(game)}\n\n{launch}[ビルド可能なソース]({source})\n\n'
 page+=f'![タイトル画面]({raw}/{ident}/title.png)\n\n{manual["summary"]}\n\n## 遊び方\n\n{manual["guide"]}\n\n## ゲーム画面\n\n'
 assert len(manual['captions'])==3
 for i,caption in enumerate(manual['captions'],1):page+=f'![{caption}]({raw}/{ident}/gameplay-{i}.png)\n\n{caption}。\n\n'
 page+=common+f'\n## ビルド\n\n```sh\nmake -C games/{ident}\nmake -C games/{ident} test\n```\n\n環境の準備・一括ビルドは[Games README]({repo}/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。\n'
 (wiki/(ident.upper()+'.md')).write_text(page)
 (root/'games'/ident/'README.md').write_text(page.replace(raw+'/'+ident,'../../docs/games/screenshots/'+ident))
 dest=root/'docs/games/screenshots'/ident;dest.mkdir(parents=True,exist_ok=True)
 for name in ['title','gameplay-1','gameplay-2','gameplay-3']:
  for suffix in ['', '-1x']:
   image=name+suffix+'.png';src=root/'build/games'/ident/image
   if src.exists():shutil.copyfile(src,dest/image)
   elif not (dest/image).exists():raise FileNotFoundError(f'Capture actual gameplay first: {ident}/{image}')
home=f'# JR-800 ゲームライブラリー\n\n定番とモダンな遊びを組み合わせた**{len(games)}本**のゲームを収録しています。SDKサンプルとは別の独立したゲームで、ゲーム内表示は英語です。\n\n## ジャンルから探す\n\n| ジャンル | 作品数 | 内容 |\n|---|---:|---|\n'
roadmap='# 50本の開発一覧\n\n「収録済み」は実装・検証対象の作品です。「制作予定」は未収録で、遊べる作品数には数えません。各作品の公開サイト確認後に起動リンクを掲載します。\n'
sidebar=f'[ゲーム一覧]({repo}/wiki)\n\n'
known={g['id'] for g in games}
for genre in genres:
 working=[g for g in games if g['genre']==genre['id']]
 planned=[g for g in plan['programs'] if g['genre']==genre['id']]
 home+=f'| [{genre["title"]}]({wiki_link(genre_page(genre))}) | {len(working)} | {genre["description"]} |\n'
 sidebar+=f'- [{genre["title"]} ({len(working)})]({wiki_link(genre_page(genre))})\n'
 page=f'# {genre["title"]}\n\n[ゲーム一覧へ]({repo}/wiki)\n\n{genre["description"]}\n\n## 収録ゲーム\n\n'
 if working:
  page+='| タイトル | 系統 | 内容 |\n|---|---|---|\n'
  for game in working:
   manual=json.loads((root/'games'/game['id']/'manual.json').read_text())
   page+=f'| [{game["title"]}]({wiki_link(game["id"].upper())}) | {style(game)} | {manual["summary"]} |\n'
 else:page+='このジャンルの作品は制作予定です。\n'
 queued=[g for g in planned if g['id'] not in known]
 if queued:
  page+='\n## 制作予定\n\n'
  for game in queued:page+=f'- **{game["title"]}** — {game["summary"]}\n'
 (wiki/(genre_page(genre)+'.md')).write_text(page)
 roadmap+=f'\n## {genre["title"]}\n\n| タイトル | 系統 | 状態 | 実装範囲 |\n|---|---|---|---|\n'
 for game in planned:roadmap+=f'| {game["title"]} | {style(game)} | {"収録済み" if game["id"] in known else "制作予定"} | {game["summary"]} |\n'
home+='\n各作品のページに概要・操作・タイトル画面とゲーム中3場面を掲載します。**公開環境で確認済みの作品だけ「遊ぶ」リンクを付けます。** 同じサイト・パスでROMを保存済みなら、リンクからタイトル画面へ直接進めます。初回は手元のBASIC ROMを選び、Start BASICを押してください。\n\n'+common
(wiki/'Home.md').write_text(home)
(wiki/'_Sidebar.md').write_text(sidebar)
source_home=re.sub(re.escape(repo)+r'/wiki/(Genre-[^)]+)',r'wiki/\1.md',home)
(root/'docs/games/README.md').write_text(source_home)
(root/'docs/games/roadmap.md').write_text(roadmap)
print(f'Generated {len(games)} game manuals and {len(genres)} genre pages; {len(verified & known)} verified launch links')
