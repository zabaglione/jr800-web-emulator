# SIGNAL GHOST

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [探索・冒険](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Adventure) · モダン

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=signal-ghost) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/signal-ghost)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/titles/signal-ghost.png)

90度ずつ回転する監視カメラを避け、2系統の端末を停止して脱出する全20面の潜入パズルです。

## 遊び方

テンキー2468またはWASDで移動します。A・Bは壁ではなく、歩いて入れる端末のマスです。方向キーで自分の位置をAまたはBと重ねてからSPACEを押すと、その系統のカメラが止まります。端末に入るだけでは停止しません。端末以外、または停止済みの端末ではSPACEで1手待機します。壁やカメラへは進めず、ぶつかっても手は進みません。

1手ごとにカメラが時計回りに90度回り、その後に視線を判定します。各カメラは正面へ90度に広がる三角形の範囲を、4マス先まで監視します。三角形の内側全体が監視対象です。壁や別のカメラに当たると、その先は見えません。カメラ内の白い切れ込みが正面の向き、床の網模様で塗られた三角形や四角い部分が、現在の監視範囲です。重なった光も同じ監視範囲として扱います。動いた直後の向きで見つかるかどうかが決まります。現在は光がない場所でも、次の回転で監視範囲に入る場合があります。危ないときは安全なマスでSPACEを1回押して待ち、カメラの向きを変えてから進んでください。端末や出口の上も監視対象です。

端末Aは2台、Bは1台のカメラを停止します。SPACEで停止を実行した手では、その系統の視線が消えた後で判定します。停止した機器は斜線入りの四角に変わります。停止済みカメラも障害物として残ります。LINKSを0にすると出口の×印が横線に変わり、そこへ進めばクリアです。

見つかると失敗します。SPACEでその面を再挑戦できます。端末停止は各100点で、クリア時に100からSTEPSを引いた値（最低0）が加算されます。左側のSCOREは得点、右上の数字は面番号です。

RETURNで停止メニューを開きます。WAITは端末を操作せずに1手待機します。RESETとRETRYはその面をやり直します。何も押していない間やメニュー中はカメラが回りません。全20面は開始時のSTAGE画面で選べます。

1面は練習用です。開始位置から右・右・下でAの上へ入り、SPACEで停止します。この道と開始位置はカメラがどの方向を向いても安全です。次に下側の通路からBへ進み、SPACEで止めて出口へ向かいます。端末操作以外に待機する必要はありません。キーは1回ずつ押してください。

ゲーム開始時は、自分の位置が効果音とともに短く点滅します。

## ゲーム画面

![安全な入口と、近くにある最初の端末A](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/bd26eb4aa8a691102e1c5c1eab5c7cc548450bbb/docs/games/screenshots/signal-ghost/gameplay-1.png)

安全な入口と、近くにある最初の端末A。

![回転のタイミングを見て端末へ潜入する](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/bd26eb4aa8a691102e1c5c1eab5c7cc548450bbb/docs/games/screenshots/signal-ghost/gameplay-2.png)

回転のタイミングを見て端末へ潜入する。

![片方の系統を止め、残る端末と出口へ進む](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/bd26eb4aa8a691102e1c5c1eab5c7cc548450bbb/docs/games/screenshots/signal-ghost/gameplay-3.png)

片方の系統を止め、残る端末と出口へ進む。

![端末Aと自分の位置を重ねた状態](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/bd26eb4aa8a691102e1c5c1eab5c7cc548450bbb/docs/games/screenshots/signal-ghost/motion.png)

端末Aと自分の位置を重ねた状態。

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
make -C games/signal-ghost
make -C games/signal-ghost test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
