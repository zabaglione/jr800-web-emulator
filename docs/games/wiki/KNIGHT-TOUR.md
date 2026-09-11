# KNIGHT TOUR

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [盤上戦略](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Board) · 定番

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=knight-tour) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/knight-tour)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/knight-tour/title.png)

ナイトのL字移動で6×6の全マスを一度ずつ巡る、6種類の開始位置のパズルです。

## 遊び方

方向キーでカーソルを動かし、＋の付いたマスをSPACEで選ぶと、ナイトが縦2・横1、または縦1・横2のL字に跳びます。すでに通ったマスには入れません。全36マスを巡ればクリアです。

マスの数字は訪問順、LEFTは残りマス数です。STUCKになったときは次の跳び先がありません。RETURNのUNDOで1手ずつ繰り返し戻せるので、経路を修正できます。RESETで最初からやり直します。開始前の面選択では6種類の開始位置を選べます。

## ゲーム画面

![最初の位置と跳べるマスの案内](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/knight-tour/gameplay-1.png)

最初の位置と跳べるマスの案内。

![通った順番を残しながら盤面を巡回](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/knight-tour/gameplay-2.png)

通った順番を残しながら盤面を巡回。

![残り7マスの終盤で経路を考える](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/knight-tour/gameplay-3.png)

残り7マスの終盤で経路を考える。

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
make -C games/knight-tour
make -C games/knight-tour test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
