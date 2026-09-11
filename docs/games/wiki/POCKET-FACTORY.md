# POCKET FACTORY

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [経営・シミュレーション](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Simulation) · モダン

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=pocket-factory) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/pocket-factory)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/26a59d255d4174a3998caf4388d6cc98ce372c9e/docs/games/screenshots/pocket-factory/title.png)

資源A・Bをベルトと加工機で運び、製品2種類を出荷する全12課題の工場パズルです。

## 遊び方

左側の資源口から原料が出ます。PRESS Aは資源Aを製品Aへ、PRESS Bは資源Bを製品Bへ加工し、右向きに排出します。右側の対応する出荷口へ届け、左側のSHIP A/GOAL AとSHIP B/GOAL Bを両方達成するとクリア。行き先が埋まっている資源はその場で待つため、逆向きのベルトや合流の詰まりに注意してください。

方向キーで配置カーソルを動かし、SPACEで現在の部品を置きます。RETURN → **SELECT TOOL** → 方向キー → SPACEで、4方向のベルト・2種類の加工機・消去を選択します。選択中のRETURNは取消です。**RUN / PAUSE** で生産を実行・停止します。配置を変えるときは停止してください。

## ゲーム画面

![出荷口を確認して工場を設計](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/26a59d255d4174a3998caf4388d6cc98ce372c9e/docs/games/screenshots/pocket-factory/gameplay-1.png)

出荷口を確認して工場を設計。

![加工機を通って資源が流れる](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/26a59d255d4174a3998caf4388d6cc98ce372c9e/docs/games/screenshots/pocket-factory/gameplay-2.png)

加工機を通って資源が流れる。

![別の地形で2系統の製品を出荷](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/26a59d255d4174a3998caf4388d6cc98ce372c9e/docs/games/screenshots/pocket-factory/gameplay-3.png)

別の地形で2系統の製品を出荷。

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
