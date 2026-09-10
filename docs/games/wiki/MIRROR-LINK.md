# MIRROR LINK

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [パズル・論理](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Logic) · モダン

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=mirror-link) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/mirror-link)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/mirror-link/title.png)

鏡を回転させて、すべての受光器へレーザーを導く全20面のパズルです。

## 遊び方

白い輪が未点灯の受光器、塗られた輪が点灯済みです。後半は2〜3個の光源が登場します。壁と光源で光は止まります。同じ位置・方向に戻る光路も有限の処理で停止します。

方向キーでカーソルを移動し、SPACEで鏡を90度回転します。RETURNの **UNDO ROTATION** で直前の回転を戻し、**RESET MIRRORS** でその面の鏡を初期配置へ戻せます。

## ゲーム画面

![1本の光路と鏡](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/mirror-link/gameplay-1.png)

1本の光路と鏡。

![2回の反射を使う面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/mirror-link/gameplay-2.png)

2回の反射を使う面。

![複数の光源を接続する後半の面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/mirror-link/gameplay-3.png)

複数の光源を接続する後半の面。

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
make -C games/mirror-link
make -C games/mirror-link test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
