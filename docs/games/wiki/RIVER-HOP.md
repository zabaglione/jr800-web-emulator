# RIVER HOP

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [アクション](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Action) · 定番

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=river-hop) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/river-hop)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/783272507e2b55cc520eeb3ca127580a8bd4a108/docs/games/screenshots/river-hop/title.png)

車列を避け、流れる丸太を渡って5つの岸へ到着する12面のアクションゲームです。

## 遊び方

SPACEで開始し、方向キーで1マスずつ跳びます。下から2列の道路には車が流れ、中央は安全な草地、その上2列は丸太が流れる川です。車のあるマスへ入ったり、丸太のない川へ入ったりするとLIVESを1失います。

丸太の上では流れに運ばれます。そのまま画面の左右から流れ落ちても失敗です。一番上の三角印がある5つの岸へ、1回ずつ到着してください。到着済みの岸は四角い印に変わり、もう一度入ることはできません。岸以外の場所へ跳び上がってもLIVESを失います。

1回の渡河にはTIMEの制限があります。到着すると100点と残りTIMEが得点に入り、下の草地へ戻ります。SPACEで次の渡河を始めてください。HOMEが5になるとクリアです。各面はLIVES5から始まり、LIVESをすべて失うと失敗します。得点は面ごとに集計します。

RETURNでメニューを開くと車・丸太・TIMEが止まります。PAUSEはメニューを閉じた後も停止を保ち、SPACEで再開します。RESETやRETRYは到着済みの岸も含めて、その面を最初からやり直します。12面は車列・丸太の配置と移動間隔が異なり、開始時のSTAGE画面で選べます。

## ゲーム画面

![道路と川の流れを見て開始](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/783272507e2b55cc520eeb3ca127580a8bd4a108/docs/games/screenshots/river-hop/gameplay-1.png)

道路と川の流れを見て開始。

![丸太に運ばれながら岸を目指す](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/783272507e2b55cc520eeb3ca127580a8bd4a108/docs/games/screenshots/river-hop/gameplay-2.png)

丸太に運ばれながら岸を目指す。

![4つの岸へ到着し最後の1つを狙う](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/783272507e2b55cc520eeb3ca127580a8bd4a108/docs/games/screenshots/river-hop/gameplay-3.png)

4つの岸へ到着し最後の1つを狙う。

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
make -C games/river-hop
make -C games/river-hop test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
