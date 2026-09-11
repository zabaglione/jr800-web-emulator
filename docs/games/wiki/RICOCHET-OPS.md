# RICOCHET OPS

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [シューティング](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Shooting) · モダン

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=ricochet-ops) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/ricochet-ops)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/ricochet-ops/title.png)

発射位置と8方向の角度を選び、反射する弾で3個の標的を狙う全20面の射撃パズルです。

## 遊び方

左右で砲台を下端の6か所へ移動します。上で照準を時計回り、下で反時計回りに回し、SPACEで発射します。AIMにはUP・UP-R・RIGHT・DN-R・DOWN・DN-L・LEFT・UP-Lの8方向を表示します。WASDとテンキーも同じ操作です。

弾は斜めの鏡で反射し、四角い標的へ当たると100点が入ります。標的を壊した後も弾は進むため、1発で複数の標的を壊せます。石壁、画面外、自分の砲台へ達すると弾は止まります。同じ場所と方向を繰り返す循環でも停止します。点線は直前の弾が通った跡です。

AMMOは3発で、FOESを0にすればクリアします。3発を使い切っても標的が残れば失敗です。1〜5面には1発、6〜15面には2発、16〜20面には3発の解法があります。弾が飛んでいる間は移動と再発射ができません。

RETURNでメニューを開くと弾が止まります。UNDO SHOTは直前の発射を1回だけ取り消し、標的・得点・残弾・発射位置・角度を戻します。飛行中にも使えます。軌跡は消去されます。RESETとRETRYはその面を最初からやり直します。失敗画面ではSPACEで再挑戦してください。20面は開始時のSTAGE画面で選べます。

## ゲーム画面

![鏡の向きと標的の配置を読む](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/ricochet-ops/gameplay-1.png)

鏡の向きと標的の配置を読む。

![鏡で曲がる弾と通過した軌跡](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/ricochet-ops/gameplay-2.png)

鏡で曲がる弾と通過した軌跡。

![残りの標的を狙う終盤の射線](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/ricochet-ops/gameplay-3.png)

残りの標的を狙う終盤の射線。

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
make -C games/ricochet-ops
make -C games/ricochet-ops test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
