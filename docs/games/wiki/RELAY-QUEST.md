# RELAY QUEST

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [探索・冒険](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Adventure) · 定番

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=relay-quest) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/relay-quest)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/titles/relay-quest.png)

鍵と回復薬を集め、2基のリレーを回収して遺跡の出口を目指す全12面の探索ゲームです。

## 遊び方

テンキー2468またはWASDで移動します。方向キーで向きを変え、SPACEで正面の敵を攻撃します。主人公の突起が向きです。敵のいるマスには進めないので、敵へ向いてから攻撃してください。1回の攻撃で敵の体力を2減らします。敵の下の点は残り体力です。

移動、命中する攻撃、薬の使用を行うと1手が進み、その後に上下左右で隣接する敵が各1ダメージを与えます。倒した敵は反撃しません。壁へぶつかる、向きだけを変える、何もない方向へ攻撃する操作では敵は動きません。考えている間も時間は止まります。

HPは9から始まり、0になると失敗します。鍵を拾うとKEYSが増え、鍵穴のある扉を通ると1本消費します。瓶を拾うとTONICが増えます。RETURNのメニューでUSE TONICを選ぶと1本を使いHPを3回復します（上限9）。満タンまたは薬がない場合は消費しません。敵に隣接して薬を使うと回復後に反撃を受けます。

三角印のあるリレーを2基回収してRELAYを0にし、横線3本の階段へ進むとクリアです。リレーは100点、敵を倒すと25点、クリア時は残りHP×10点です。得点は上部のSCO、面番号は左上に表示します。

RETURNで停止メニューを開き、RESETまたはRETRYでその面をやり直せます。失敗画面ではSPACEで再挑戦します。全12面は開始時のSTAGE画面で選択できます。

ゲーム開始時は、自分の位置が効果音とともに短く点滅します。

## ゲーム画面

![扉と資源の位置を読み、遺跡の順路を考える](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/13991a5d547d33993eb28301277893b453a79965/docs/games/screenshots/relay-quest/gameplay-1.png)

扉と資源の位置を読み、遺跡の順路を考える。

![守衛へ向かって攻撃し、通路を確保する](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/13991a5d547d33993eb28301277893b453a79965/docs/games/screenshots/relay-quest/gameplay-2.png)

守衛へ向かって攻撃し、通路を確保する。

![2基を回収し、残り体力を保って出口へ向かう](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/13991a5d547d33993eb28301277893b453a79965/docs/games/screenshots/relay-quest/gameplay-3.png)

2基を回収し、残り体力を保って出口へ向かう。

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
make -C games/relay-quest
make -C games/relay-quest test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
