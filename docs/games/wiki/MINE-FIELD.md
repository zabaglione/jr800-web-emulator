# MINE FIELD

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [パズル・論理](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Logic) · 定番

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=mine-field) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/mine-field)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/mine-field/title.png)

周囲の数字から地雷を探す14×7の盤面です。地雷10・15・20個の3難易度を選べます。

## 遊び方

方向キーでマスを選び、SPACEで開きます。最初に開くマスとその周囲には地雷を置きません。数字は周囲8マスにある地雷の数です。数字が0の領域は続けて開きます。地雷以外の全マスを開けばクリアです。

RETURNのFLAG MODEに入ると、SPACEで旗を置く・取り除く操作になります。旗は地雷の数まで置けます。RETURNで旗モードを終了し、OPENに戻ります。旗を置いたマスは開きません。FLAGSが旗の数、MINESが地雷の数、SAFEが残りの安全な未開放マス数です。

すでに開いた数字でSPACEを押すと、周囲の旗の数が数字と同じ場合に、残りの周囲マスを一括で開きます。旗の位置が間違っていると地雷を踏むため、数だけでなく位置も確認してください。失敗時は地雷と間違った旗を表示します。SPACEで新しい盤面、RETURNのRESETでもやり直せます。毎回配置は変わり、推理だけで決められない局面もあります。

## ゲーム画面

![最初の安全な場所から空白領域を開放](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/mine-field/gameplay-1.png)

最初の安全な場所から空白領域を開放。

![旗を置いて地雷の位置を整理](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/mine-field/gameplay-2.png)

旗を置いて地雷の位置を整理。

![旗の推理違いで地雷が開いた失敗画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/mine-field/gameplay-3.png)

旗の推理違いで地雷が開いた失敗画面。

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
make -C games/mine-field
make -C games/mine-field test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
