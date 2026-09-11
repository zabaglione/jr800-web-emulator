# POCKET FACTORY

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [経営・シミュレーション](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Simulation) · モダン

[ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/pocket-factory)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/pocket-factory/title.png)

2種類の資源を加工して出荷する、全40課題の工場パズルです。後半は折り返しと迂回を含む長い搬送経路を組みます。

## 遊び方

方向キーでマスを選び、SPACEで選択中の設備を置きます。ベルトは矢印方向へ送り、PRESS A/Bは対応する原料を製品へ加工して右へ送ります。両製品を指定数出荷すればクリアです。追加目標は、加工済みの製品を2か所の印へ通してからクリアすることです。

RETURNメニューの SELECT TOOL でベルト4方向・加工機2種類・ERASEを選び、RUN / PAUSE で生産と配置を切り替えます。設備の新設・変更・消去がそれぞれ1手です。同じ設備の再指定、カーソル移動、工具選択、実行切替は手数に含めません。規定手数は、両方の印を経由する確認済み設備配置が基準です。

## ゲーム画面

![第1面の初期配置と規定手数](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/pocket-factory/gameplay-1.png)

第1面の初期配置と規定手数。

![第31面で追加目標に挑戦している場面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/pocket-factory/gameplay-2.png)

第31面で追加目標に挑戦している場面。

![第40面を規定手数と追加目標の両方を満たしてクリア](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/pocket-factory/gameplay-3.png)

第40面を規定手数と追加目標の両方を満たしてクリア。

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

![面選択とパスワード](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/pocket-factory/selection.png)

面選択では規定手数、追加目標、BEST、2種類のパスワードを確認できます。

## 共通操作

| 操作 | キー |
|---|---|
| 上・下・左・右 | テンキー8・2・4・6、またはW・S・A・D |
| 決定・開始・主要アクション | SPACE |
| 取消・戻る・操作メニュー | RETURN |
| BASICへ終了 | BREAK |

メニューを開いている間はゲームが停止します。決定・取消は押した瞬間だけ反応し、移動だけ長押しできます。

## 確認済み環境

JR-HuBASIC 1.0を使ったWebエミュレーターで確認しています。ゲームは標準16KB RAM内に収まり、拡張RAMなしのNative/WASM再生でも検証しています。ブラウザーのBASIC起動には既存のBASIC実験プロファイルを使用します。2.0は対応未確認です。実機動作・カセット転送・実機のLCD応答と音は未検証です。

BREAKで戻る際には、以前のBASICプログラムと変数が消去されます。必要な内容はゲームを読み込む前に保存してください。途中経過はエミュレーターの汎用状態保存で保存できます。ROMとROM入り状態ファイルは配布しません。

## ビルド

```sh
make -C games/pocket-factory
make -C games/pocket-factory test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
