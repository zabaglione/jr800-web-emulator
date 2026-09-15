# POLY DEFENDER

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [シューティング](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Shooting) · モダン

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=poly-defender) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/poly-defender)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/titles/poly-defender.png)

右から迫る立体の敵を撃つ横スクロール型シューティングです。左側の自機を上下へ動かし、敵弾を避けながら強化アイテムを取るか、攻撃を続けるかを選びます。攻撃・操作・得点・演出・音はすべてJR-800の機械語で動きます。

## 遊び方

### 操作

| キー | 操作 |
| --- | --- |
| SPACE／RETURN | タイトル・終了画面から開始／再挑戦 |
| テンキー8／2、W／S | 上下へ移動 |
| SPACE | 自動連射をオン／オフ。長押しで連続切り替えはしません |
| BREAK | 空のBASICへ戻る |

開始時は自動連射がオンです。移動しながら撃つための同時押しは不要です。文字キー側の数字と矢印キーは移動に使いません。上部の`SC`は得点、`H`は耐久力、`W`は武器レベル、`S`はシールドです。右側には場面、ボス戦ではHPと`AIM / FIRE / DIVE / OPEN`を表示します。

### 攻撃とアイテム

- 敵は右端の小さい姿から約1.2秒かけて拡大・前進します。登場中は攻撃も被弾もしません。
- 射撃型は横線で狙いを予告します。敵弾は小さい黒い四角で、狙った自機の高さへ約3.2秒で進みます。発射後は追尾しません。
- 突進型は予告後に狙った高さへ突っ込みます。敵を避けて見逃しても耐久力は減りません。
- `W`は武器を1段階強化します。レベル1は弾1発、レベル2は最大2発の連射、レベル3は貫通弾です。貫通弾は敵1体につき1回だけ当たり、2ダメージを与えます。
- 大きい`+`は1回分のシールドです。被弾すると先にシールドを消費し、耐久力と武器を守ります。
- シールドなしの被弾では耐久力と武器が1段階下がります。武器は最低1です。再開後の12更新は無敵になります。
- アイテムは右から左へ流れます。自機の高さを合わせて回収し、危険な場合は画面外へ見送れます。回収では敵弾を消しません。

通常敵のドロップは、空き枠へ実際に出る順にWとシールドが交互になります。アイテムが残っている間は上書きしません。強化が最大でも回収すると2点を得られます。

### 4つの場面

1. **PATROL**：射撃型と突進型が計4体。狙いを見て移動し、強化を集めます。
2. **SPLIT**：耐久力3の八面体を倒すと小さい三角錐2体へ分裂します。上下へ広がりながら接近し、両方を撃破するとWが出ます。片方を見逃すと報酬はありません。
3. **RUSH**：小型の敵が計10体。最大2体を保ち、空いた枠へ次の敵が接近します。
4. **BOSS**：警告音と`BOSS APPROACHING`の表示の後、HP30の八面体が右端から拡大・接近します。狙い撃ち、予告付き突進、後退と隙を繰り返します。`OPEN`中の命中は1ダメージ増加。HP20以下でW、HP10以下でシールドを各1個出します。HP9以下では連続射撃が増え、攻撃間隔も短くなります。

武器とシールドは次の場面へ持ち越します。ボス撃破でクリア、耐久力3を失うとゲームオーバーです。通常敵と分裂後の敵は各1点、親は2点、ボスは20点、アイテムは2点。得点表示は99で止まります。

### 音と被弾演出

開始・クリア・ゲームオーバーに別々のジングルを再生します。自機と敵の発射、装甲への命中、小爆発、分裂、アイテム、シールド、被弾、撃墜にも音があります。ボス撃破には大きい爆発の演出を入れています。

被弾時は敵・弾・自機の位置を止め、画面全体を反転します。衝突位置をXで示し、下部に`HIT BY BOLT`または`HIT BY ENEMY`を表示します。通常の被弾は短く停止して再開し、最後の撃墜は数秒間その場面を残してからゲームオーバーへ移ります。

Webエミュレーターでは画面をクリックして音声を有効にしてください。


## ゲーム画面

![ボス接近の警告。文字と動く枠、警告音で次の戦いを予告します](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/poly-defender/gameplay-1.png)

ボス接近の警告。文字と動く枠、警告音で次の戦いを予告します。

![狙いをかわしてボスに反撃。強化を拾うか、射撃を続けるかを選びます](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/poly-defender/gameplay-2.png)

狙いをかわしてボスに反撃。強化を拾うか、射撃を続けるかを選びます。

![最後の被弾後は場面を止め、原因と衝突位置を確認できます](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/poly-defender/gameplay-3.png)

最後の被弾後は場面を止め、原因と衝突位置を確認できます。

## 確認済み環境

JR-HuBASIC 1.0を使ったWebエミュレーターで確認しています。ゲームは標準16KB RAM内に収まり、拡張RAMなしのNative/WASM再生でも検証しています。ブラウザーのBASIC起動には既存のBASIC実験プロファイルを使用します。2.0は対応未確認です。実機動作・カセット転送・実機のLCD応答と音は未検証です。

BREAKで戻る際には、以前のBASICプログラムと変数が消去されます。必要な内容はゲームを読み込む前に保存してください。途中経過はエミュレーターの汎用状態保存で保存できます。ROMとROM入り状態ファイルは配布しません。

## ビルド

```sh
make -C games/poly-defender
make -C games/poly-defender test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
