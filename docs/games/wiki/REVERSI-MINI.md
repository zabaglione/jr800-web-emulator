# REVERSI MINI

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [盤上戦略](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Board) · 定番

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=reversi-mini) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/reversi-mini)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/2f1dcb2f428aff44ed112b3276a2e78ec3b89fc7/docs/games/screenshots/reversi-mini/title.png)

6×6の盤面で相手のディスクを挟んで返す、CPU対戦のリバーシです。置ける場所の表示と自動パスがあります。

## 遊び方

自分は先手の白い輪、CPUは黒い円です。方向キーでカーソルを動かし、＋印の合法手にSPACEで置きます。縦・横・斜めに相手のディスクを挟むと、その列が自分の色に返ります。

YOUとCPUは現在の枚数、MOVESは自分の手数です。置ける場所がない側は自動的にパスし、YOU PASSまたはCPU PASSと表示します。両者とも置けなくなった時点で、枚数の多い側が勝ちです。引き分けもあります。CPUは角・辺・取られやすい角の周囲・返せる枚数を評価します。

RETURNからUNDO TURNで自分の前の手番まで戻れます。自動パスに続くCPUの複数手もまとめて戻します。RESETまたはRETRYで最初から再対戦できます。

## ゲーム画面

![中央の4枚と合法手を示す＋印](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/2f1dcb2f428aff44ed112b3276a2e78ec3b89fc7/docs/games/screenshots/reversi-mini/gameplay-1.png)

中央の4枚と合法手を示す＋印。

![辺を確保して返す範囲が広がった中盤](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/2f1dcb2f428aff44ed112b3276a2e78ec3b89fc7/docs/games/screenshots/reversi-mini/gameplay-2.png)

辺を確保して返す範囲が広がった中盤。

![残りの空きマスと枚数を読み合う終盤](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/2f1dcb2f428aff44ed112b3276a2e78ec3b89fc7/docs/games/screenshots/reversi-mini/gameplay-3.png)

残りの空きマスと枚数を読み合う終盤。

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
make -C games/reversi-mini
make -C games/reversi-mini test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
