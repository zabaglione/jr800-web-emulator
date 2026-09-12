# PAWN RACE

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [盤上戦略](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Board) · モダン

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=pawn-race) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/pawn-race)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/titles/pawn-race.png)

前進と斜め取りで敵陣を突破する6×6のCPU対戦です。3段階のCPUを選べます。

## 遊び方

自分は白い駒で上端へ、CPUは黒い駒で下端への到達を目指します。方向キーで自分の駒を選び、SPACEで選択します。＋の付いた行き先へカーソルを動かし、SPACEで1マス進みます。

前方の空きマス、または斜め前の空きマスへ進めます。相手の駒を取れるのは斜め前だけです。後退や横移動はできません。どれか1個が敵側の端へ着くか、相手に合法手がなくなれば勝利です。

CPUの難易度1は前進を優先し、2は取り合いと取り返される危険を評価し、3は次の手で突破される配置も避けます。FOESはCPUの残り駒数です。RETURNは選択取消、選択していないときはメニューです。UNDO TURNで直前の自分とCPUの手をまとめて戻し、RESETで対局をやり直せます。

操作対象のマスや項目はゆっくり点滅します。方向キーを押すと選択位置がすぐにはっきり表示されます。

## ゲーム画面

![白と黒の12個ずつの駒で対局開始](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/13991a5d547d33993eb28301277893b453a79965/docs/games/screenshots/pawn-race/gameplay-1.png)

白と黒の12個ずつの駒で対局開始。

![選択した駒の移動先を確認](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/13991a5d547d33993eb28301277893b453a79965/docs/games/screenshots/pawn-race/gameplay-2.png)

選択した駒の移動先を確認。

![突破を狙う駒と後方の守り](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/13991a5d547d33993eb28301277893b453a79965/docs/games/screenshots/pawn-race/gameplay-3.png)

突破を狙う駒と後方の守り。

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
make -C games/pawn-race
make -C games/pawn-race test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
