# STAR PATROL

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [シューティング](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Shooting) · 定番

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=star-patrol) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/star-patrol)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/star-patrol/title.png)

移動する敵編隊と敵弾を避けながら撃つ、全12ステージの固定画面シューティングです。

## 遊び方

左右で自機を動かし、SPACEで開始・発射します。自分の弾は画面内に1発だけです。SPACEを押し続けても連射にはならないので、弾が消えてから押し直してください。敵弾に自分の弾を当てて打ち消すこともできます。

敵は左右へ移動し、左右端で2回折り返すごとに1段下がります。敵をすべて倒せばクリアです。外枠のある敵は2発必要で、命中1回につき10点が入ります。後半のステージほど編隊の移動間隔が短くなります。

自機の上にある3か所の防壁は、それぞれ左右2マスでできています。各マスは自分や敵の弾を2発受けると壊れます。防壁の隙間から撃つか、自分で穴を開けて射線を作ってください。防壁と弾はステージが変わると元に戻ります。

敵弾に触れるとLIFEを1失い、中央からSPACEで再開します。倒した敵・得点・防壁の損傷は引き継ぎます。LIFE3を使い切るか、敵編隊が最下段まで侵入すると失敗です。敵は残っている編隊から射手を選んで撃ちます。

RETURNでメニューを開くと停止します。PAUSEはメニューを閉じた後も停止を保ち、SPACEで再開します。RESETとRETRYは敵と防壁を含めて、そのステージの最初からやり直します。12ステージは開始時のSTAGE画面で選べます。

## ゲーム画面

![防壁の隙間から編隊を狙う](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/star-patrol/gameplay-1.png)

防壁の隙間から編隊を狙う。

![装甲を持つ敵と敵弾をかわす](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/star-patrol/gameplay-2.png)

装甲を持つ敵と敵弾をかわす。

![残った敵を追い込む終盤](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/star-patrol/gameplay-3.png)

残った敵を追い込む終盤。

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
make -C games/star-patrol
make -C games/star-patrol test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
