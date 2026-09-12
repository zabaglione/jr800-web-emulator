# TOWER LEAP

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [アクション](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Action) · 定番

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=tower-leap) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/tower-leap)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/titles/tower-leap.png)

足場を跳び移って12階の頂上を目指す、全12コースの縦スクロールアクションです。

## 遊び方

左右で移動し、足場に立っているときにSPACEを押すとジャンプします。SPACEを離してから左右を押せば、空中で着地点を調整できます。空中でも左右へ動けますが、空中での再ジャンプはできません。SPACEを押し続けても着地後に自動では跳びません。次のジャンプには一度キーを離してください。

上から足場へ着地すると止まります。足場は下から通り抜けられます。高く登ると画面が上へ移り、画面下へ落ちるとLIVESを1失います。左右端には足場がないので、地上から歩き落ちても失敗です。

FLOORは到達した最高階、SAVEは落下時に戻る階です。太い足場の5階と9階が中間地点で、一度着地すればそこから再開できます。12階へ着地するとクリアです。LIVES3で始まり、すべて失うと失敗します。

RETURNのメニューでは動きが止まります。CHECKPOINTはLIVESを1使って中間地点へ戻ります。RESETとRETRYはそのコースの1階からLIVES3で再挑戦します。開始時のSTAGE画面で12コースを選べます。

ゲーム開始時は、自分の位置が効果音とともに短く点滅します。

横向きの矢印がある足場は左右へ動き、乗っている自分も運ばれます。ひび割れた足場は着地後しばらくすると崩れるので、立ち止まらず次へ跳んでください。空中を巡回する敵に触れてもLIVESを失います。後半のコースほど動く足場や崩れる足場が増え、崩れるまでの猶予も短くなります。

## ゲーム画面

![足場を見上げて最初のジャンプを準備](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/13991a5d547d33993eb28301277893b453a79965/docs/games/screenshots/tower-leap/gameplay-1.png)

足場を見上げて最初のジャンプを準備。

![中間地点から次の足場へ跳ぶ](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/13991a5d547d33993eb28301277893b453a79965/docs/games/screenshots/tower-leap/gameplay-2.png)

中間地点から次の足場へ跳ぶ。

![頂上へ向けて高い階を登る](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/13991a5d547d33993eb28301277893b453a79965/docs/games/screenshots/tower-leap/gameplay-3.png)

頂上へ向けて高い階を登る。

## プレイ動画

[プレイ動画を見る（30秒・音声あり）](https://zabaglione.github.io/jr800-web-emulator/videos/#tower-leap)

通常速度で操作と演出を確認できます。再生・一時停止・シークは動画ページで操作できます。

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
make -C games/tower-leap
make -C games/tower-leap test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
