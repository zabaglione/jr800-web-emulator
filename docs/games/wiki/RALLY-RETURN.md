# RALLY RETURN

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [スポーツ・タイミング](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Sports) · 定番

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=rally-return) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/rally-return)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/titles/rally-return.png)

打点とスピンで返球の角度を変える、CPUとのパドル対戦です。先に5点取ると勝利します。

## 遊び方

自分は左、CPUは右のパドルです。上／下（8／2、W／S）の長押しでパドルを動かします。READYと表示されている間にSPACEを押すとサーブします。得点のたびに球が中央で止まり、次のSPACEを待ちます。

パドルの上端近くで打つと上へ、下端近くで打つと下へ鋭く返します。中央寄りでは角度が緩くなります。左（4、A）でSPINを-1、右（6、D）で+1に選び、返球をそれぞれ上向き・下向きに補正します。スピンは選んだ状態を保ち、RETURNメニューのNEUTRALで0に戻します。

上下の壁では球が反射します。相手のパドルを抜いて右端へ出すとYOUに1点、自分のパドルを抜かれて左端へ出るとCPUに1点入ります。5点先取で勝敗が決まります。ラリー中のSPACEは追加の球を出しません。

DIFFICULTYは3段階です。難易度を上げるとCPUが球を追う頻度が上がり、ゲームの進行も速くなります。CPUが追いつきにくい角度を、打点とスピンで作ってください。

RETURNのメニューでは球とCPUが止まります。RESETまたはRETRYで0対0からやり直し、失敗画面のSPACEでも再挑戦できます。勝利後のSPACEで次の難易度へ進みます。

ゲーム開始時は、自分の位置が効果音とともに短く点滅します。

自分とCPUのどちらが得点しても、得点表示の後にジングルが鳴ります。自分の得点は上がる音、CPUの得点は下がる音で区別できます。

## ゲーム画面

![中央の球をSPACEでサーブし、先に5点を目指す](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/13991a5d547d33993eb28301277893b453a79965/docs/games/screenshots/rally-return/gameplay-1.png)

中央の球をSPACEでサーブし、先に5点を目指す。

![打点をずらして、CPUが追いにくい角度へ返す](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/13991a5d547d33993eb28301277893b453a79965/docs/games/screenshots/rally-return/gameplay-2.png)

打点をずらして、CPUが追いにくい角度へ返す。

![速いCPUとのラリーで、上下の壁も利用する](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/13991a5d547d33993eb28301277893b453a79965/docs/games/screenshots/rally-return/gameplay-3.png)

速いCPUとのラリーで、上下の壁も利用する。

## プレイ動画

[プレイ動画を見る（30秒・音声あり）](https://zabaglione.github.io/jr800-web-emulator/videos/#rally-return)

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
make -C games/rally-return
make -C games/rally-return test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
