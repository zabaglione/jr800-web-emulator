# ORBIT GUARD

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [シューティング](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Shooting) · モダン

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=orbit-guard) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/orbit-guard)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/titles/orbit-guard.png)

中央の砲台を8方向へ回し、軌道から近づく敵を迎撃する全12ウェーブの防衛シューティングです。

## 遊び方

左右で照準を1方向ずつ回し、上で真上、下で真下を直接向きます。WASDとテンキーでも同じ操作ができます。SPACEで開始し、その後のSPACEで照準方向へ射撃します。現在の方向は中央の砲身で確認できます。

射撃は同じ方向にいる最も近い敵へ当たります。黒い敵は1発、枠のある敵は2発で撃破でき、1回の命中につき10点です。敵のいない方向へ撃っても得点は入りません。SPACEを押し続けても連射にはならず、発射には短い待ち時間があります。

敵が現れる位置は外側の点滅と予告音で先に示します。出現後は軌道の間を細かく移動しながら中央へ近づきます。内側の軌道を越えるとCOREが1減り、CORE5をすべて失うと失敗です。FOESは未到着の敵を含む残数で、撃破か侵入で減ります。すべての敵を処理した時点でCOREが残っていればクリアします。9ウェーブ目以降は、時計回りへ角度を変えながら近づきます。

RETURNのメニューからPULSEを選ぶと、画面にいる敵を一度に撃破します。ウェーブごとに1回だけ使え、未到着の敵には当たりません。装甲の残りにも応じて得点が入ります。右側のSCOREが得点です。

RETURNでメニューを開いている間は停止します。RESETとRETRYはCORE・PULSE・得点を含めて、そのウェーブの最初からやり直します。12ウェーブは開始時のSTAGE画面で選べます。

ゲーム開始時は、自分の位置が効果音とともに短く点滅します。

出現予告、発射、撃破、中央への侵入、PULSEで、それぞれ異なる効果音が鳴ります。

## ゲーム画面

![中央の砲台と3重の接近軌道](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/13991a5d547d33993eb28301277893b453a79965/docs/games/screenshots/orbit-guard/gameplay-1.png)

中央の砲台と3重の接近軌道。

![複数方向から迫る装甲付きの敵](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/13991a5d547d33993eb28301277893b453a79965/docs/games/screenshots/orbit-guard/gameplay-2.png)

複数方向から迫る装甲付きの敵。

![角度を変えながら接近する後半ウェーブ](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/13991a5d547d33993eb28301277893b453a79965/docs/games/screenshots/orbit-guard/gameplay-3.png)

角度を変えながら接近する後半ウェーブ。

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
make -C games/orbit-guard
make -C games/orbit-guard test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
