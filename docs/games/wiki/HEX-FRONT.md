# HEX FRONT

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [盤上戦略](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Board) · モダン

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=hex-front) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/hex-front)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/783272507e2b55cc520eeb3ca127580a8bd4a108/docs/games/screenshots/hex-front/title.png)

六方向につながる6×6の盤面で辺どうしを結ぶCPU対戦です。1局1回だけ相手の拠点を変換するリレーを使えます。

## 遊び方

自分は白い輪で左辺から右辺へ、CPUは黒い拠点で上辺から下辺への接続を目指します。上下左右に加えて右上・左下の2方向が隣接します。盤面の線と外側の矢印がつながる方向と目標です。方向キーで空き拠点を選び、SPACEで配置します。

RETURNのRELAYを選ぶと変換モードになります。自分の拠点が隣に2個以上あるCPUの拠点を選び、SPACEで自分のものに変換できます。1局1回で、RLYが残り回数です。変換の後もCPUが1手進めます。RETURNで変換選択を取り消せます。

NEEDは通常の配置で接続するために必要な最小の空き拠点数、MOVESは自分の行動回数です。RETURNからUNDO TURNで直前の自分とCPUの行動を戻せます。リレーを使った手を戻すと残り回数も戻ります。RETRYで新しい対局にします。

## ゲーム画面

![左右と上下の接続を目指す序盤](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/783272507e2b55cc520eeb3ca127580a8bd4a108/docs/games/screenshots/hex-front/gameplay-1.png)

左右と上下の接続を目指す序盤。

![リレーで相手拠点を変換する選択画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/783272507e2b55cc520eeb3ca127580a8bd4a108/docs/games/screenshots/hex-front/gameplay-2.png)

リレーで相手拠点を変換する選択画面。

![リレーを使った後の接続経路](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/783272507e2b55cc520eeb3ca127580a8bd4a108/docs/games/screenshots/hex-front/gameplay-3.png)

リレーを使った後の接続経路。

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
make -C games/hex-front
make -C games/hex-front test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
