# PUSH LUCK

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [カード・ダイス](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Cards) · モダン

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=push-luck) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/push-luck)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/titles/push-luck.png)

得点を確定するか、もう一度振るか。1が出ると手番の得点を失う、CPU対戦のリスク判断ゲームです。

## 遊び方

先に確定得点を100点以上にした側が勝ちます。自分の手番では画面下の **ROLL / BANK** を方向キーで選び、SPACEで実行します。ROLLを選ぶと、目が次々に切り替わってから出目が止まります。2〜6はPOTに加算され、何度でも続けられます。1が出るとPOTを失って相手の手番になります。すでに確定したYOU・CPUの得点は減りません。

画面下の **BANK** でPOTを確定得点に加え、相手に手番を渡します。POTが0のときのBANKは何もしません。POTだけが100点を超えても勝ちにはならず、BANKによる確定が必要です。

1による失点とBANKのどちらでも、画面中央に **CPU TURN** を表示してからCPUの手番を始めます。表示が消えるまではCPUは振りません。CPUの出目もアニメーションとSEを伴って間隔を空けて表示され、下段のCPU ROLLSにその手番の履歴が残ります。CPU THINKINGの間は相手の処理を待ちます。RETURNでメニューを開くとCPUの進行も止まります。CPUは先の出目を調べず、難易度に応じたPOTの基準と残りの必要点でBANKを判断します。難易度1・2・3の基準は12・18・24点です。難易度3は自分が80点以上のときに32点までリスクを取ります。CPUは最大6回で確定します。

自分の手番ではRETURNのRESETで対戦を最初からやり直せます。メニューのRETRYでも再開できます。方向キーはROLL / BANKと、メニューや難易度の選択に使います。

## ゲーム画面

![1が出てPOTを失いCPUの手番へ](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/beb386220901f9861b50f8c8c72e1e3a7ecc22ed/docs/games/screenshots/push-luck/gameplay-1.png)

1が出てPOTを失いCPUの手番へ。

![CPUが出目を重ねながら確定を判断](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/beb386220901f9861b50f8c8c72e1e3a7ecc22ed/docs/games/screenshots/push-luck/gameplay-2.png)

CPUが出目を重ねながら確定を判断。

![21点を確定するか続けるかを選ぶ場面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/beb386220901f9861b50f8c8c72e1e3a7ecc22ed/docs/games/screenshots/push-luck/gameplay-3.png)

21点を確定するか続けるかを選ぶ場面。

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
make -C games/push-luck
make -C games/push-luck test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
