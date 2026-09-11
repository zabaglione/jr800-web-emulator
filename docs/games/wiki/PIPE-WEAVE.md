# PIPE WEAVE

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [パズル・論理](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Logic) · 定番

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=pipe-weave) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/pipe-weave)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/26a59d255d4174a3998caf4388d6cc98ce372c9e/docs/games/screenshots/pipe-weave/title.png)

管を回して給水口Sから全36マスと排水口Eへ水を通す20面です。漏れのない配管を組み上げます。

## 遊び方

方向キーで管を選び、SPACEで時計回りに90度回します。四方向に開いた十字管は回しても変わらないため、その場では手数を増やしません。

Sが給水口、Eが排水口です。給水口につながった管の中心は塗りつぶされ、つながっていない管は白い四角で表示されます。DRYは未通水のマス数、LEAKは相手と接続していない口の数です。画面の外へ向いた口も漏れとして数えます。全36マスに水が通り、漏れが0になればクリアです。

RETURNのUNDOで直前の回転を1手戻し、RESETで現在の面をやり直します。開始前の面選択で20面の好きな面を選べます。

## ゲーム画面

![給水口と回転前の管の配置](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/26a59d255d4174a3998caf4388d6cc98ce372c9e/docs/games/screenshots/pipe-weave/gameplay-1.png)

給水口と回転前の管の配置。

![上半分へ水を通した配管](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/26a59d255d4174a3998caf4388d6cc98ce372c9e/docs/games/screenshots/pipe-weave/gameplay-2.png)

上半分へ水を通した配管。

![残りの未通水部分と漏れを修正する終盤](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/26a59d255d4174a3998caf4388d6cc98ce372c9e/docs/games/screenshots/pipe-weave/gameplay-3.png)

残りの未通水部分と漏れを修正する終盤。

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
make -C games/pipe-weave
make -C games/pipe-weave test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
