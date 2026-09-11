# CIRCUIT DECK

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [カード・ダイス](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Cards) · モダン

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=circuit-deck) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/circuit-deck)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/circuit-deck/title.png)

手札3枚とエナジー3を使う、9戦制のカードバトルです。最終戦にはボスが待っています。

## 遊び方

カードのCOSTが残りエナジー以下なら使用できます。使ったカードは捨て札へ移り、ターン終了時に残りの手札も捨てて3枚引き直します。山札が尽きると捨て札をシャッフルします。ENが残りエナジー、HPが自分の体力、ARMが自分の防御です。ENEMYは敵のHP、ARMORは敵の防御、NEXTとINTENTは次の敵行動とその量です。8回の勝利報酬では3枚から1枚を選んでデッキへ追加し、HPを12回復します（上限40）。難易度1〜3を選べます。

左右または上下で手札を選び、SPACEで使用します。RETURNの **END TURN** でターンを終え、**TOGGLE HELP** で案内を切り替えます。報酬画面でも方向キーとSPACEでカードを選びます。

| カード | COST | 効果 |
|---|---:|---|
| STRIKE | 1 | 6ダメージ |
| GUARD | 1 | 防御6 |
| SPARK | 0 | 2ダメージ |
| PIERCE | 2 | 12ダメージ |
| WALL | 2 | 防御12 |
| HEAL | 1 | HPを5回復 |
| VENOM | 1 | 毒3を付与。敵ターンごとに継続ダメージ |
| CHARGE | 0 | エナジーを1増やす |
| DRAIN | 2 | 7ダメージ、HPを4回復 |
| NOVA | 3 | 20ダメージ |
| FOCUS | 1 | この戦闘中の攻撃力を2増やす |
| ECHO | 1 | 4ダメージ、防御4 |

防御は次の敵行動まで有効です。毒と攻撃力上昇は各99が上限で、次の戦闘ではリセットされます。

## ゲーム画面

![手札・エナジー・敵の予告](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/circuit-deck/gameplay-1.png)

手札・エナジー・敵の予告。

![勝利後の3択報酬](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/circuit-deck/gameplay-2.png)

勝利後の3択報酬。

![最終戦のボス](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/circuit-deck/gameplay-3.png)

最終戦のボス。

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
make -C games/circuit-deck
make -C games/circuit-deck test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
