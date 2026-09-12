# ACE STACK

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [カード・ダイス](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Cards) · 定番

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=ace-stack) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/ace-stack)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/titles/ace-stack.png)

合計13の組を取り除き、28枚のピラミッドを崩すカードパズルです。解ける20種類の配札を収録しています。

## 遊び方

Aは1、Tは10、Jは11、Qは12、Kは13です。下に重なる2枚がなくなったカードを選べます。左右の縦線があるカードが選択可能です。方向キーでカーソルを動かし、SPACEで1枚目、もう一度SPACEで合計13になる2枚目を選びます。Kは1枚だけで取り除けます。選択中のRETURNは取消です。

左側のWASTEは捨て札の一番上です。カーソルを右に進めるとWASTEも選べます。RETURNメニューのDRAWで山札から1枚引きます。捨て札を使うと、その下のカードが再び現れます。山札は24枚で一巡だけです。STOCKは未使用の山札、LEFTはピラミッドの残り枚数です。

ピラミッド28枚をすべて取り除くとクリアです。山札が尽き、取り除けるKも合計13の組もないと失敗します。山札の使い切りには注意してください。RETURNのUNDOは直前の組の除去またはDRAWを1回戻します。RETRYで同じ配札を最初から遊び直せます。20面すべては標準52枚の各数字4枚から作った独自配札です。MOVESは除去とDRAWを数えた手数です。

操作対象のマスや項目はゆっくり点滅します。方向キーを押すと選択位置がすぐにはっきり表示されます。

## ゲーム画面

![下段から取り除きピラミッドを崩す](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/13991a5d547d33993eb28301277893b453a79965/docs/games/screenshots/ace-stack/gameplay-1.png)

下段から取り除きピラミッドを崩す。

![捨て札との組を選んで上段を開ける](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/13991a5d547d33993eb28301277893b453a79965/docs/games/screenshots/ace-stack/gameplay-2.png)

捨て札との組を選んで上段を開ける。

![残り7枚になった最終面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/13991a5d547d33993eb28301277893b453a79965/docs/games/screenshots/ace-stack/gameplay-3.png)

残り7枚になった最終面。

## プレイ動画

[プレイ動画を見る（30秒・音声あり）](https://zabaglione.github.io/jr800-web-emulator/videos/#ace-stack)

通常速度で操作と演出を確認できます。再生・一時停止・シークは動画ページで操作できます。

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
make -C games/ace-stack
make -C games/ace-stack test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
