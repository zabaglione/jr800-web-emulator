# SLIDE NINE

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [パズル・論理](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Logic) · 定番

[ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/slide-nine)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/slide-nine/title.png)

3×3の盤面で空きマスを動かし、数字を1から8の順に並べる全20面のスライドパズルです。

## 遊び方

方向キーは空きマスが移動する方向です。隣の数字と入れ替え、上段を1・2・3、中段を4・5・6、下段を7・8・空きにそろえます。SPACEはメニューの決定に使います。

LEFTは正しい場所にない数字の数です。MOVESは移動回数（最大255）です。RETURNからUNDOで1手戻し、RESETまたはRETRYで再挑戦できます。20面は最短4〜23手で解ける異なる配置です。

## ゲーム画面

![大きな数字タイルと空きマス](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/slide-nine/gameplay-1.png)

大きな数字タイルと空きマス。

![第9面の並べ替え途中](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/slide-nine/gameplay-2.png)

第9面の並べ替え途中。

![第19面の終盤パズル](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/slide-nine/gameplay-3.png)

第19面の終盤パズル。

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
make -C games/slide-nine
make -C games/slide-nine test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
