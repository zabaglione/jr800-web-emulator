# SUIT RUN

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [カード・ダイス](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Cards) · 定番

[ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/suit-run)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/suit-run/title.png)

捨て札と前後の数字をつなぎ、連続得点を狙うゴルフ型ソリティアです。解ける20種類の配札を収録しています。

## 遊び方

7列に並ぶ35枚をすべて取り除くとクリアです。各列は一番下にあるカードから使います。方向キーでカーソルを動かし、右側のWASTEと数字が1つ違うカードをSPACEで取り除きます。Aは1、Tは10、Jは11、Qは12、Kは13です。AとKもつながります。取り除いたカードが次のWASTEになります。

カードを連続して取り除くとCHAINが増え、1枚目は1点、2枚目は2点、3枚目は3点と得点が伸びます。使えるカードがないときはRETURNメニューのDRAWで山札を引きます。DRAWはCHAINを0に戻します。山札は16枚で一巡だけです。STOCKが残り山札、LEFTが場に残ったカード数です。山札が尽きて使えるカードもなくなると失敗します。

RETURNのUNDOは直前の除去またはDRAWを1回戻し、得点とCHAINも復元します。RETRYで同じ配札をやり直せます。すべての数字を4枚ずつ使う標準52枚構成の独自配札を20面収録しています。

## ゲーム画面

![各列の下から前後の数字をつなぐ](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/suit-run/gameplay-1.png)

各列の下から前後の数字をつなぐ。

![4連続で得点を伸ばしている場面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/suit-run/gameplay-2.png)

4連続で得点を伸ばしている場面。

![14連続を保ちながら最終面を進める](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/suit-run/gameplay-3.png)

14連続を保ちながら最終面を進める。

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
make -C games/suit-run
make -C games/suit-run test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
