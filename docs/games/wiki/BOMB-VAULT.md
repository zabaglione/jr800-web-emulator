# BOMB VAULT

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [アクション](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Action) · 定番

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=bomb-vault) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/bomb-vault)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/bomb-vault/title.png)

時限爆弾で壁を壊し、3個の鍵を集めて出口を目指す全12面のアクションです。

## 遊び方

方向キーで1マスずつ移動し、SPACEで足元に爆弾を1個置きます。移動またはSPACEで敵と導火線が動き始めます。爆弾を置いた後は、そのマスから離れて隠れてください。爆弾のマスへ戻ることはできません。

FUSEが0になると、上下左右へ最大3マスの爆風が広がります。石壁で止まり、木箱は最初の1個を壊して止まります。Kの箱からは鍵が現れます。爆風が消えてから鍵を拾い、KEYSを0にして右下の出口へ入りましょう。爆弾と爆風が残っている間は次の爆弾を置けません。

2体の敵は通路を左右に巡回し、壁・箱・爆弾で引き返します。敵は爆風で倒せますが、全滅させなくてもクリアできます。敵や爆風に触れるとLIFEを1失い、左上へ戻ります。壊した箱・集めた鍵・倒した敵はそのままです。LIFE3を使い切ると失敗します。

右下のSが得点で、箱30点、鍵50点、敵100点、脱出200点です。得点は面ごとに集計します。RETURNでメニューを開くと敵と導火線が止まります。PAUSEはメニューを閉じた後も停止を保ち、移動またはSPACEで再開します。RESETとRETRYはその面の箱・鍵・敵も最初に戻します。12面は開始時のSTAGE画面で選べます。

## ゲーム画面

![鍵入りの箱と巡回する敵を確認](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/bomb-vault/gameplay-1.png)

鍵入りの箱と巡回する敵を確認。

![壁に隠れて爆風を避ける](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/bomb-vault/gameplay-2.png)

壁に隠れて爆風を避ける。

![鍵を集めて出口へ向かう](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/bomb-vault/gameplay-3.png)

鍵を集めて出口へ向かう。

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
make -C games/bomb-vault
make -C games/bomb-vault test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
