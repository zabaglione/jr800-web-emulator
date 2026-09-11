# GRID CLAIM

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [アクション](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Action) · 定番

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=grid-claim) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/grid-claim)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/2f1dcb2f428aff44ed112b3276a2e78ec3b89fc7/docs/games/screenshots/grid-claim/title.png)

走った跡が壁になる競走でCPUを追い込む、2勝先取の対戦アクションです。3段階の難易度と6種類のアリーナがあります。

## 遊び方

左の黒い車体がYOU、右のひし形がCPUです。SPACEで走り始めると自動で進み、方向キーで次に曲がる向きを指定します。直前の進行方向と逆には曲がれません。走行跡はラウンドが終わるまで残り、自分の跡・CPUの跡・障害物・画面の端にぶつかると負けです。

両者は同時に1マス進みます。同じマスへ入った場合、互いの車体へ突っ込んだ場合、両者とも進めなくなった場合は引き分けです。YOUかCPUが先に2勝すると対戦が終わります。ラウンド終了後はSPACEで次のアリーナへ移り、もう一度SPACEで開始します。引き分けでは勝数は増えません。

初級のCPUは空いていれば直進を優先します。中級は次に曲がれる方向の多いマス、上級は到達できる空き領域の広いマスを選びます。上級はプレイヤーの現在の進行先も避けて計算します。難易度が上がると移動間隔も短くなります。6種類のアリーナはラウンドごとに順番に切り替わります。

RETURNでメニューを開くと停止します。PAUSEはメニューを閉じた後も停止を保ち、SPACEで再開します。RESETとRETRYは両者の勝数を0にして対戦をやり直します。開始時のDIFFICULTY画面で3段階を選べます。

## ゲーム画面

![向かい合った車体から競走開始](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/2f1dcb2f428aff44ed112b3276a2e78ec3b89fc7/docs/games/screenshots/grid-claim/gameplay-1.png)

向かい合った車体から競走開始。

![走行跡で相手の進路を狭める](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/2f1dcb2f428aff44ed112b3276a2e78ec3b89fc7/docs/games/screenshots/grid-claim/gameplay-2.png)

走行跡で相手の進路を狭める。

![残りの空き領域を争う上級戦](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/2f1dcb2f428aff44ed112b3276a2e78ec3b89fc7/docs/games/screenshots/grid-claim/gameplay-3.png)

残りの空き領域を争う上級戦。

## 共通操作

| 操作 | キー |
|---|---|
| 上・下・左・右 | テンキー8・2・4・6、またはW・S・A・D |
| 決定・開始・主要アクション | SPACE |
| 取消・戻る・操作メニュー | RETURN |
| BASICへ終了 | BREAK |

メニューを開いている間はゲームが停止します。決定・取消は押した瞬間だけ反応し、移動だけ長押しできます。

クリア後は解き終えた盤面をしばらく残し、ジングルと余韻の後に結果を表示します。続行案内が出てからSPACEを押してください。押しっぱなしでは次へ進みません。

## 確認済み環境

JR-HuBASIC 1.0を使ったWebエミュレーターで確認しています。ゲームは標準16KB RAM内に収まり、拡張RAMなしのNative/WASM再生でも検証しています。ブラウザーのBASIC起動には既存のBASIC実験プロファイルを使用します。2.0は対応未確認です。実機動作・カセット転送・実機のLCD応答と音は未検証です。

BREAKで戻る際には、以前のBASICプログラムと変数が消去されます。必要な内容はゲームを読み込む前に保存してください。途中経過はエミュレーターの汎用状態保存で保存できます。ROMとROM入り状態ファイルは配布しません。

## ビルド

```sh
make -C games/grid-claim
make -C games/grid-claim test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
