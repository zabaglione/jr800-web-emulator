# RAIL DISPATCH

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [経営・シミュレーション](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Simulation) · 定番

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=rail-dispatch) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/rail-dispatch)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/titles/rail-dispatch.png)

上下から来る列車を2つの信号で合流させ、分岐を切り替えて正しい駅へ届ける全12課題の運行ゲームです。

## 遊び方

テンキー2468またはWASDで、上側信号・下側信号・中央の分岐を選びます。選択中の装置が反転し、SPACEで切り替わります。信号の×は停止、縦の印は通過です。分岐は上側A駅・下側B駅のどちらへ送るかを選び、左側のEXITにも行き先を表示します。

最初はSET SWITCHES（設定中）です。RETURNのメニューでRUN / PAUSEを選ぶと運行を開始します。同じ項目で再び設定中に戻すと、列車を止めたまま設定を変更できます。運行中でも信号と分岐を操作できます。RETURNのメニューを開いている間は時間が止まります。

列車はA・Bの文字で目的地を表します。NEXTは次に入ってくる列車で、Uは上側入口、Dは下側入口、矢印の右が目的地です。列車は時刻表の順に入ります。入口や4本分の管理枠が埋まっていると、空くまで出発を待ちます。表示中の列車を見て、中央の合流に同時進入させないよう信号を操作してください。

停止列車の後ろでは自動的に間隔を保って待ちます。ただし、上下の列車が合流点へ同時に入ると衝突します。分岐の進行方向は、列車が分岐から出る瞬間の設定で決まります。違う駅へ届けても失敗です。失敗理由はCRASH（衝突）、ROUTE（行き先間違い）、TIME（時間切れ）と表示します。

1〜4面は6本、5〜8面は8本、9〜12面は10本を届けます。DONEは到着済み、LEFTは残りの本数です。左側のTIMEは運行時刻、左上の数字は課題番号です。上部のNEXが次の列車です。時刻180までに全列車を正しい駅へ届ければクリアです。時計の1刻みはCPUサイクルで約0.163秒で、設定中とメニュー中は進みません。

まずは中央の区間へ1本ずつ通す運行を試してください。RETURNのRESETまたはRETRYで課題を最初からやり直せます。失敗画面ではSPACEで再挑戦します。全12課題は開始時のSTAGE画面で選べます。

操作対象のマスや項目はゆっくり点滅します。方向キーを押すと選択位置がすぐにはっきり表示されます。

## ゲーム画面

![上下の入口信号とA・B駅への分岐を設定する](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/13991a5d547d33993eb28301277893b453a79965/docs/games/screenshots/rail-dispatch/gameplay-1.png)

上下の入口信号とA・B駅への分岐を設定する。

![待機列車を残し、中央へ1本ずつ通す](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/13991a5d547d33993eb28301277893b453a79965/docs/games/screenshots/rail-dispatch/gameplay-2.png)

待機列車を残し、中央へ1本ずつ通す。

![終盤の列車を正しい行き先へ送り出す](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/13991a5d547d33993eb28301277893b453a79965/docs/games/screenshots/rail-dispatch/gameplay-3.png)

終盤の列車を正しい行き先へ送り出す。

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
make -C games/rail-dispatch
make -C games/rail-dispatch test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
