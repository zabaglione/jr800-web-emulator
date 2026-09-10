# LAMP GRID

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [パズル・論理](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Logic) · 定番

[ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/lamp-grid)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/lamp-grid/title.png)

十字に反転する5×5のランプを、すべて消す全20面の論理パズルです。

## 遊び方

方向キーで反転するマスを選び、SPACEでそのマスと上下左右のランプを反転します。黒い内部が点灯、点だけの内部が消灯です。選択中のマスは白黒が反転します。LEFTは点灯数、MOVESは操作回数（最大255）です。

RETURNのメニューからUNDOで直前の反転を1回取り消せます。RESETまたはRETRYで同じ面を最初からやり直します。全20面はどれも解ける配置で、面選択から好きな問題を選べます。

## ゲーム画面

![最初のランプ配置と反転カーソル](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/lamp-grid/gameplay-1.png)

最初のランプ配置と反転カーソル。

![第8面で周囲の点灯を整理する場面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/lamp-grid/gameplay-2.png)

第8面で周囲の点灯を整理する場面。

![第17面のランプ配置](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/lamp-grid/gameplay-3.png)

第17面のランプ配置。

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
make -C games/lamp-grid
make -C games/lamp-grid test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
