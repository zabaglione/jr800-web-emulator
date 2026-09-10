# STEP STRIKE

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=step-strike) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/step-strike)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/step-strike/title.png)

移動・射撃・待機の1手でだけ世界が進む、全20面の戦術パズルです。

## 遊び方

全員の敵を倒すとクリア。隣の敵に移動すると近接攻撃で倒せます。射撃は最後に移動した向きへ最大4マス届きます。敵は3手ごとに、壁で遮られていない縦・横の直線上へ弾を撃ちます。敵弾に触れると失敗です。

方向キーで移動・向き変更、SPACEで射撃。RETURNの **WAIT ONE TURN** で1手待機し、**HELP** で案内を切り替えます。移動できない壁へ押した場合は手が進みません。

## ゲーム画面

![敵の配置を見て作戦を立てる](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/step-strike/gameplay-1.png)

敵の配置を見て作戦を立てる。

![遮蔽物を使って接近する](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/step-strike/gameplay-2.png)

遮蔽物を使って接近する。

![敵と弾道を見ながら次の1手を選ぶ](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/step-strike/gameplay-3.png)

敵と弾道を見ながら次の1手を選ぶ。

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
make -C games/step-strike
make -C games/step-strike test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
