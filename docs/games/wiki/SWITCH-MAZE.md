# SWITCH MAZE

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [パズル・論理](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Logic) · モダン

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=switch-maze) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/switch-maze)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/26a59d255d4174a3998caf4388d6cc98ce372c9e/docs/games/screenshots/switch-maze/title.png)

2系統のスイッチと扉を使い、2本の鍵を集めて出口へ向かう全20面の迷路パズルです。

## 遊び方

方向キーで1マス移動します。Aに乗ると2本の縦棒がある扉、Bに乗ると3本の縦棒がある扉が開閉します。同じスイッチに再び乗ると元に戻ります。開いた扉は上下の枠だけの表示です。

鍵は自動で回収します。KEYSが0の状態でEの出口へ入るとクリアです。SPACEはメニューの決定に使います。RETURNからUNDOで移動・鍵回収・扉の状態を1手戻せます。RESETまたはRETRYで再挑戦します。MOVESは移動回数（最大255）です。

## ゲーム画面

![2系統のスイッチと鍵がある最初の迷路](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/26a59d255d4174a3998caf4388d6cc98ce372c9e/docs/games/screenshots/switch-maze/gameplay-1.png)

2系統のスイッチと鍵がある最初の迷路。

![第8面で扉の開閉順序を考える場面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/26a59d255d4174a3998caf4388d6cc98ce372c9e/docs/games/screenshots/switch-maze/gameplay-2.png)

第8面で扉の開閉順序を考える場面。

![第17面の鍵回収と帰り道](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/26a59d255d4174a3998caf4388d6cc98ce372c9e/docs/games/screenshots/switch-maze/gameplay-3.png)

第17面の鍵回収と帰り道。

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
make -C games/switch-maze
make -C games/switch-maze test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
