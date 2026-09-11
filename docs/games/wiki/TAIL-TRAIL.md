# TAIL TRAIL

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [アクション](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Action) · 定番

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=tail-trail) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/tail-trail)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/783272507e2b55cc520eeb3ca127580a8bd4a108/docs/games/screenshots/tail-trail/title.png)

食べ物を集めるたびに長くなる尾を避けて走る、3難易度のヘビゲームです。

## 遊び方

SPACEで移動を開始します。方向キーで進行方向を変え、ひし形の食べ物を集めてください。FOODがGOALに達するとクリアです。難易度1・2・3の目標は10・20・30個で、難易度が上がるほど移動も速くなります。SIZEは頭を含む全長です。

画面外へ出たり、自分の胴体へ入ったりすると失敗します。進行方向と正反対には曲がれません。ただし、食べ物を取らない移動では一番後ろの尾も同時に進むので、尾が直前にあったマスへは入れます。

短く押した方向も次の移動まで保持されます。1回の移動までに複数の方向を入力した場合は、現在の方向へ逆走しない最後の入力を使います。テンキーとWASDのどちらでも操作できます。

RETURNでメニューを開くと時間が止まります。PAUSEはメニューを閉じた後も停止し、SPACEで再開します。RESETやRETRYで最初から遊び直します。食べ物は空いているマスにJR-800側の乱数で配置します。

## ゲーム画面

![食べ物と進む方向を見て開始](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/783272507e2b55cc520eeb3ca127580a8bd4a108/docs/games/screenshots/tail-trail/gameplay-1.png)

食べ物と進む方向を見て開始。

![食べ物を10個集めて長くなった尾](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/783272507e2b55cc520eeb3ca127580a8bd4a108/docs/games/screenshots/tail-trail/gameplay-2.png)

食べ物を10個集めて長くなった尾。

![高速な難易度で自分の尾を避ける](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/783272507e2b55cc520eeb3ca127580a8bd4a108/docs/games/screenshots/tail-trail/gameplay-3.png)

高速な難易度で自分の尾を避ける。

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
make -C games/tail-trail
make -C games/tail-trail test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
