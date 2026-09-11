# SPDX-License-Identifier: MIT
"""Write the player-facing gallery, genre lists, and getting-started pages."""
import json
from html import escape


def write_index(root, games, genres, verified, raw, site, repo):
    copy = json.loads((root / 'games/tools/gallery.json').read_text())
    if set(copy['games']) != {g['id'] for g in games}:
        raise ValueError('Every published game needs a gallery introduction')
    if set(copy['genres']) != {g['id'] for g in genres}:
        raise ValueError('Every genre needs a gallery introduction')
    wiki = root / 'docs/games/wiki'
    wiki.mkdir(parents=True, exist_ok=True)

    def link(page):
        return f'{repo}/wiki/{page}'

    def genre_page(genre):
        return 'Genre-' + genre['id'].title()

    def actions(game):
        ident = game['id']
        start = (f'[プレイ]({site}?program={ident})' if ident in verified
                 else f'[ビルド可能なソース]({repo}/tree/main/games/{ident})')
        return f'{start} · [遊び方を見る]({link(ident.upper())})'

    def gallery(items):
        rows = ['| タイトル画面 | ゲーム・楽しみ方 |', '| --- | --- |']
        for game in sorted(items, key=lambda g: g['title']):
            ident, title = game['id'], game['title']
            page = link(ident.upper())
            picture = (f'[<img src="{raw}/{ident}/title.png" width="384" '
                       f'alt="{escape(title, quote=True)} のタイトル画面">]({page})')
            rows.append(f'| {picture} | **[{title}]({page})**<br>'
                        f'{copy["games"][ident]}<br>{actions(game)} |')
        return '\n'.join(rows) + '\n'

    home = (f'# JR-800 ゲームライブラリー\n\n'
            f'ひとりでじっくり考える。CPUと読み合う。タイミングを合わせて駆け抜ける。\n'
            f'**{len(games)}本**のゲームから、気になる画面と遊び方を見つけてください。\n\n'
            '## ジャンルから探す\n\n'
            '| ジャンル | 作品数 | こんな遊びが好きなら |\n| --- | ---: | --- |\n')
    for genre in genres:
        items = [g for g in games if g['genre'] == genre['id']]
        home += (f'| [{genre["title"]}]({link(genre_page(genre))}) | {len(items)} | '
                 f'{copy["genres"][genre["id"]]} |\n')
    home += (f'\n[タイトル順の全作品]({link("All-Games")}) · '
             f'[タイトル画面ギャラリー](#user-content-タイトル画面ギャラリー) · '
             f'[はじめて遊ぶ方へ・共通操作]({link("Controls")})\n\n'
             '**ROMを設定済みなら「プレイ」からすぐに起動できます。** '
             '開いた画面をマウスでクリックし、SPACEで始めてください。'
             f'初回の準備は[こちら]({link("Controls")})。\n\n'
             f'じっくり挑戦したい方には、[40面のパズルチャレンジ]({link("Puzzle-Challenges")})も。'
             '規定手数と追加目標を両立して、最高評価を狙えます。パスワードで続きから遊べます。\n\n'
             '## タイトル画面ギャラリー\n\n')
    for genre in genres:
        items = [g for g in games if g['genre'] == genre['id']]
        heading = f'{genre["title"]}（{len(items)}作品）'
        intro = copy['genres'][genre['id']]
        table = gallery(items)
        home += f'### [{heading}]({link(genre_page(genre))})\n\n{intro}\n\n{table}\n'
        (wiki / f'{genre_page(genre)}.md').write_text(
            f'# {heading}\n\n[ゲーム一覧へ]({repo}/wiki)\n\n{intro}\n\n{table}')
    home += (f'[ページ先頭へ](#user-content-jr-800-ゲームライブラリー) · '
             f'[共通操作と起動方法]({link("Controls")})\n\n'
             'ゲーム内の表示は英語です。JR-HuBASIC 1.0を使ったWebエミュレーターで確認しています。'
             '実機での動作・音声は未確認です。\n')
    (wiki / 'Home.md').write_text(home)
    (root / 'docs/games/README.md').write_text(home)

    all_games = (f'# タイトル順の全作品\n\n[画像付きのゲーム一覧へ]({repo}/wiki)\n\n'
                 f'{len(games)}本をタイトル順に並べています。\n\n'
                 '| ゲーム | ジャンル | 楽しみ方・起動 |\n| --- | --- | --- |\n')
    by_genre = {genre['id']: genre for genre in genres}
    for game in sorted(games, key=lambda g: g['title']):
        genre = by_genre[game['genre']]
        all_games += (f'| [{game["title"]}]({link(game["id"].upper())}) | '
                      f'[{genre["title"]}]({link(genre_page(genre))}) | '
                      f'{copy["games"][game["id"]]}<br>{actions(game)} |\n')
    (wiki / 'All-Games.md').write_text(all_games)

    controls = (f'# はじめて遊ぶ方へ・共通操作\n\n[ゲーム一覧へ]({repo}/wiki)\n\n'
                '## プレイを始める\n\n'
                '1. 気になるゲームの「プレイ」を押します。\n'
                '2. 初回は自分で用意したBASIC ROMを選び、BASICを起動します。'
                '同じサイト・ブラウザーでROMを保存済みなら、自動でタイトル画面へ進みます。\n'
                '3. **画面をマウスでクリックしてください。** 音声とキーボード操作が有効になります。\n'
                '4. SPACEで開始します。難易度やステージの選択は方向キーとSPACEで操作します。\n\n'
                '画面下の仮想キーも使えます。作品ごとの操作やルールは「遊び方を見る」に掲載しています。\n\n')
    controls += (root / 'games/tools/manual-common.md').read_text()
    controls += (f'\n## ソースからビルドする\n\n'
                 f'[ビルド可能なソースと開発手順]({repo}/tree/main/games)を参照してください。\n')
    (wiki / 'Controls.md').write_text(controls)

    sidebar = (f'[画像付きのゲーム一覧]({repo}/wiki)\n\n'
               f'[タイトル順の全作品]({link("All-Games")})\n\n'
               f'[はじめて遊ぶ方へ]({link("Controls")})\n\n')
    for genre in genres:
        count = sum(g['genre'] == genre['id'] for g in games)
        sidebar += f'- [{genre["title"]} ({count})]({link(genre_page(genre))})\n'
    sidebar += f'\n[パズルチャレンジ・パスワード]({link("Puzzle-Challenges")})\n'
    (wiki / '_Sidebar.md').write_text(sidebar)
