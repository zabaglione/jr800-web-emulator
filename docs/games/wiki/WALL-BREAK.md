# WALL BREAK

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [アクション](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Action) · 定番

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=wall-break) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/wall-break)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/2f1dcb2f428aff44ed112b3276a2e78ec3b89fc7/docs/games/screenshots/wall-break/title.png)

パドルでボールを打ち返し、12種類のブロック配置を壊すアクションゲームです。

## 遊び方

左右キーでパドルを動かし、SPACEでボールを発射します。ボールを下へ落とさず、すべてのブロックを壊すとクリアです。パドルの端に近い位置へ当てると横へ大きく、中央寄りでは縦に近い角度で反射します。左右それぞれ2種類、計4種類の角度を打ち分けられます。

横線が入ったブロックは2回当てると壊れます。接触ごとに10点が入り、SCOREに表示されます。BRICKSは残りのブロック数です。LIVESは残りのボール数で、各面3個から始まります。落とした後は再びSPACEで発射してください。3個とも失うと失敗します。

RETURNでメニューを開くとボールとパドルが止まります。RELAUNCHはボールを1個消費して発射位置へ戻します。RESETやRETRYでは、選んだ面を3個のボールで最初から遊び直します。得点は面ごとに集計します。12面は開始時のSTAGE画面で自由に選べます。

## ゲーム画面

![最初の配置と発射位置](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/2f1dcb2f428aff44ed112b3276a2e78ec3b89fc7/docs/games/screenshots/wall-break/gameplay-1.png)

最初の配置と発射位置。

![複数回当てるブロックを含む盤面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/2f1dcb2f428aff44ed112b3276a2e78ec3b89fc7/docs/games/screenshots/wall-break/gameplay-2.png)

複数回当てるブロックを含む盤面。

![最後に残ったブロックを狙う](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/2f1dcb2f428aff44ed112b3276a2e78ec3b89fc7/docs/games/screenshots/wall-break/gameplay-3.png)

最後に残ったブロックを狙う。

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
make -C games/wall-break
make -C games/wall-break test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
