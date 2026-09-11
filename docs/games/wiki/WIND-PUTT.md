# WIND PUTT

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [スポーツ・タイミング](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Sports) · 定番

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=wind-putt) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/wind-putt)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/wind-putt/title.png)

風と壁の反射を読み、12コースのカップを狙うミニゴルフです。角度と強さを選び、各コース12打以内のカップインを目指します。

## 遊び方

左／右（4／6、A／D）でANGLEを45度ずつ回し、上／下（8／2、W／S）でPOWERを1〜8に調整します。0度は上、90度は右、180度は下、270度は左です。SPACEで1打を打ちます。小さな塗りつぶしの四角が球、その先の四隅だけの印が狙う方向です。旗の根元がカップです。

WINDの<は左向き、>は右向き、-は無風です。同じ角度でも風によって軌道が少し変わります。壁に当たると反射し、最後に減速して止まります。飛んでいる間は角度や強さを変えられません。止まってから次の打球を調整してください。

SHOTは使用した打数／12です。12打目でもカップに届けばクリアし、届かずに止まると失敗します。各コースは独立しており、クリア後のSPACEで次へ進みます。全12コースはSTAGEで選べます。

RETURNでメニューを開くと球の進行が止まります。RETEEは1打を追加してティーへ戻す救済操作です。飛んでいる球も中断して戻せます。RESETまたはRETRYで選んだコースを最初からやり直します。失敗画面ではSPACEで再挑戦します。

## ゲーム画面

![球と狙いの印を見て、風に合わせて打つ方向を調整する](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/wind-putt/gameplay-1.png)

球と狙いの印を見て、風に合わせて打つ方向を調整する。

![壁の切れ目へ向けて斜めに打ち、次の位置を作る](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/wind-putt/gameplay-2.png)

壁の切れ目へ向けて斜めに打ち、次の位置を作る。

![反射を利用しながら旗のカップを目指す](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots/wind-putt/gameplay-3.png)

反射を利用しながら旗のカップを目指す。

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
make -C games/wind-putt
make -C games/wind-putt test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
