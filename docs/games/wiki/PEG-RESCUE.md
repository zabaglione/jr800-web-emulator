# PEG RESCUE

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [盤上戦略](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Board) · 定番

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=peg-rescue) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/peg-rescue)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/peg-rescue/title.png)

独自の31穴盤で石を飛び越して取り、最後の1個を残す20面のパズルです。

## 遊び方

方向キーで石を選び、SPACEで選択します。縦か横に隣の石を1個飛び越した空き穴が、＋の印で表示されます。そこへカーソルを動かしてSPACEを押すと、飛び越した石を取り除きます。斜めには跳べません。

最後の1個を残せばクリアです。LEFTは残っている石の数、JUMPSは跳んだ回数です。5個から24個まで石の数が増える20面を収録しています。RETURNで石の選択を取り消します。選択していないときのRETURNメニューからUNDOで1手戻すか、RESETでその面をやり直せます。

## ゲーム画面

![5個の石から始める最初の課題](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/peg-rescue/gameplay-1.png)

5個の石から始める最初の課題。

![選んだ石と跳べる空き穴の案内](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/peg-rescue/gameplay-2.png)

選んだ石と跳べる空き穴の案内。

![石が残り6個になった最終課題](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/peg-rescue/gameplay-3.png)

石が残り6個になった最終課題。

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
make -C games/peg-rescue
make -C games/peg-rescue test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
