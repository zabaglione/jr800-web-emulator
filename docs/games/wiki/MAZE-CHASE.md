# MAZE CHASE

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [アクション](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Action) · 定番

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=maze-chase) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/maze-chase)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/26a59d255d4174a3998caf4388d6cc98ce372c9e/docs/games/screenshots/maze-chase/title.png)

2体の追跡者をかわし、迷路の粒を集める12面のアクションゲームです。

## 遊び方

SPACEで開始し、方向キーで迷路を進みます。小さな粒をすべて集めるとクリアです。小さな粒は10点、大きな粒は20点です。SCOREが得点、FOODが残りの粒数です。方向入力は保持され、曲がれる場所で入力した方向へ曲がります。曲がれない場合は現在の方向へ進み、壁の前では止まります。

追跡者はプレイヤーへの距離を計算して近づきます。大きな粒を取るとPOWERが32になり、その間は追跡者が逃げるようになります。接触すると50点を獲得し、その追跡者はしばらく消えてから開始位置へ戻ります。POWERは時間経過で減り、メニューを開いた間は減りません。開始時と残機を失った後にもPOWERが32あります。

POWERがない状態で追跡者に触れるとLIVESを1失います。集めた粒と得点は保たれ、SPACEで再開できます。LIVESを3つとも失うと失敗します。面ごとに得点を集計します。

RETURNでメニューを開くと全体が止まります。PAUSEではメニューを閉じても停止を保ち、SPACEで再開します。RESETやRETRYは粒も含めて選んだ面を最初からやり直します。独自制作の12迷路を、開始時のSTAGE画面から選べます。

## ゲーム画面

![粒とパワー粒の位置を確認して開始](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/26a59d255d4174a3998caf4388d6cc98ce372c9e/docs/games/screenshots/maze-chase/gameplay-1.png)

粒とパワー粒の位置を確認して開始。

![追跡者をかわしながら迷路を巡る](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/26a59d255d4174a3998caf4388d6cc98ce372c9e/docs/games/screenshots/maze-chase/gameplay-2.png)

追跡者をかわしながら迷路を巡る。

![残り9個の粒を狙う](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/26a59d255d4174a3998caf4388d6cc98ce372c9e/docs/games/screenshots/maze-chase/gameplay-3.png)

残り9個の粒を狙う。

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
make -C games/maze-chase
make -C games/maze-chase test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
