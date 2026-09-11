# LINE FOUR

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [盤上戦略](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Board) · 定番

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=line-four) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/line-four)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/26a59d255d4174a3998caf4388d6cc98ce372c9e/docs/games/screenshots/line-four/title.png)

7列×6段の盤面にディスクを落とし、先に4個を並べるCPU対戦です。3段階の難易度があります。

## 遊び方

左右で列を選び、SPACEでディスクを落とします。白い輪が自分、黒い円がCPUです。横・縦・斜めのどれかに4個並ぶと勝ちです。満杯の列には置けません。盤面が埋まると引き分けです。

難易度1はランダム性のある思考、2は即勝利と相手のリーチの防御、3は並び方と中央列、相手の次の勝ち場所も評価します。完全読みではありません。MOVESは自分の手数、FREEは空きマス数です。終了時にはYOU WIN・CPU WIN・DRAWを表示します。

RETURNからUNDO TURNで自分とCPUの直前の1ターンを戻し、RESETまたはRETRYで再対戦します。勝利または引き分けの後にSPACEを押すと次の難易度へ進みます。敗北後のSPACEは同じ難易度で再挑戦します。

## ゲーム画面

![最初のディスク配置と選択中の列](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/26a59d255d4174a3998caf4388d6cc98ce372c9e/docs/games/screenshots/line-four/gameplay-1.png)

最初のディスク配置と選択中の列。

![両端のリーチを狙う中盤](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/26a59d255d4174a3998caf4388d6cc98ce372c9e/docs/games/screenshots/line-four/gameplay-2.png)

両端のリーチを狙う中盤。

![最高難易度で縦と斜めを競う場面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/26a59d255d4174a3998caf4388d6cc98ce372c9e/docs/games/screenshots/line-four/gameplay-3.png)

最高難易度で縦と斜めを競う場面。

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
make -C games/line-four
make -C games/line-four test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
