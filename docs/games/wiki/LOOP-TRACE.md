# LOOP TRACE

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [パズル・論理](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Logic) · モダン

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=loop-trace) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/loop-trace)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/loop-trace/title.png)

A→B→Cの順に通り、全地点を一度ずつ巡って輪を閉じる20面のパズルです。

## 遊び方

Sが開始地点です。方向キーを押すと隣の地点へ進み、その間に線を引きます。点模様の壁には入れません。A・B・Cは必ずこの順で通り、NEXTが次に通るチェックポイントです。すでに通った地点には入り直せません。

ただし、直前の地点へ向かう方向キーは1手戻しになります。RETURNのUNDOでも1手ずつ繰り返し戻せます。チェックポイントから戻ると、通過順の状態も元に戻ります。

LEFTが0になったら、Sの隣からSPACEを押して最後の線を結びます。全地点を巡るだけでは完了せず、輪を閉じてクリアです。MOVESは現在引いている経路の歩数です。RETURNのRESETでその面を最初からやり直せます。12〜30地点の20面を収録しています。

## ゲーム画面

![開始地点と3つのチェックポイントを確認](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/loop-trace/gameplay-1.png)

開始地点と3つのチェックポイントを確認。

![順番を守りながら経路を伸ばす](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/loop-trace/gameplay-2.png)

順番を守りながら経路を伸ばす。

![全地点を巡り最後の線を閉じた完成図](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/loop-trace/gameplay-3.png)

全地点を巡り最後の線を閉じた完成図。

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
make -C games/loop-trace
make -C games/loop-trace test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
