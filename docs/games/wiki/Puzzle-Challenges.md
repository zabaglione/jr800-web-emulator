# パズルチャレンジ

## 40面のチャレンジと評価

面選択の左右で1〜40面を選べます。1〜10面は導入、11〜20面は基本の応用、21〜30面は難しい配置、31〜40面は上級向けです。未クリアの面も選べます。

HUDの **USED** は使った手数、**PAR** は規定手数です。規定手数を超えてもプレイを続け、通常のクリアを目指せます。

| 評価 | 条件 |
|---|---|
| 星1個 | 規定手数を超えてクリア |
| 星2個 | 規定手数以下でクリア。追加目標は未達成 |
| 星3個 | 規定手数以下でクリアし、追加目標もすべて達成 |

ゲーム内では星の小さな図形と `---` で評価を表示します。各面の **BEST** は最高評価を保持します。追加目標の内容は作品ごとの説明と面選択のヒントで確認してください。通常のクリア条件を満たすと、その時点で評価が確定します。

移動や回転など、盤面が変化する有効な操作を数えます。カーソルで選ぶだけの操作、決定前の選択、壁にぶつかる無効な移動は数えません。**UNDOも1手を消費します。** 手を戻すと、その操作で回収した印も元の状態に戻ります。RETRYはその面の手数を0からやり直し、BESTは維持します。

## パスワード

面選択画面に2種類のコードが表示されます。タイトルからSPACEで面選択へ進めます。

| 種類 | 長さ | 復元する内容 |
|---|---:|---|
| LOAD | 5文字 | 選択している面。記録済みのBESTは維持 |
| LOAD RECORD | 25文字 | 選択している面と、全40面のBEST |

上下で **LOAD** または **LOAD RECORD** を選び、SPACEで入力画面へ進みます。入力画面では上下で文字を変更、左右で入力位置を移動します。SPACEも次の文字へ進み、最後の文字でSPACEを押すと確定します。RETURNは入力取消です。25文字のコードは5文字ずつ区切って表示されます。空白は入力しません。

使用文字は `2346789ACDEFHJKM` の16種類です。`0/O`、`1/I/L`、`5/S/Z`、`8/B`、`6/G` のような取り違えを避けるため、紛らわしい組合せを同時に使いません。別のゲームのコードや検査値の合わないコードは `INVALID CODE` となり、現在の面や評価は変更しません。

コードは**面と評価**を記録します。盤面の途中状態は含みません。BASICへ戻る前にLOAD RECORDを控えると、次回の起動後に記録を復元できます。入力したLOAD RECORDの内容で全40面のBESTが置き換わるため、新しい記録を控えてから復元してください。

## 対象の15作品

- [BOX SHIFT](https://github.com/zabaglione/jr800-web-emulator/wiki/BOX-SHIFT)
- [MIRROR LINK](https://github.com/zabaglione/jr800-web-emulator/wiki/MIRROR-LINK)
- [STEP STRIKE](https://github.com/zabaglione/jr800-web-emulator/wiki/STEP-STRIKE)
- [POCKET FACTORY](https://github.com/zabaglione/jr800-web-emulator/wiki/POCKET-FACTORY)
- [LAMP GRID](https://github.com/zabaglione/jr800-web-emulator/wiki/LAMP-GRID)
- [SLIDE NINE](https://github.com/zabaglione/jr800-web-emulator/wiki/SLIDE-NINE)
- [ICE ROUTE](https://github.com/zabaglione/jr800-web-emulator/wiki/ICE-ROUTE)
- [SWITCH MAZE](https://github.com/zabaglione/jr800-web-emulator/wiki/SWITCH-MAZE)
- [KNIGHT TOUR](https://github.com/zabaglione/jr800-web-emulator/wiki/KNIGHT-TOUR)
- [PEG RESCUE](https://github.com/zabaglione/jr800-web-emulator/wiki/PEG-RESCUE)
- [PIPE WEAVE](https://github.com/zabaglione/jr800-web-emulator/wiki/PIPE-WEAVE)
- [NUMBER RAIL](https://github.com/zabaglione/jr800-web-emulator/wiki/NUMBER-RAIL)
- [MINE FIELD](https://github.com/zabaglione/jr800-web-emulator/wiki/MINE-FIELD)
- [LOOP TRACE](https://github.com/zabaglione/jr800-web-emulator/wiki/LOOP-TRACE)
- [RICOCHET OPS](https://github.com/zabaglione/jr800-web-emulator/wiki/RICOCHET-OPS)
