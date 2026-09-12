# BALANCE DOCK

[ゲーム一覧](https://github.com/zabaglione/jr800-web-emulator/wiki) / [スポーツ・タイミング](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Sports) · モダン

[遊ぶ](https://zabaglione.github.io/jr800-web-emulator/?program=balance-dock) · [ビルド可能なソース](https://github.com/zabaglione/jr800-web-emulator/tree/main/games/balance-dock)

![タイトル画面](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/titles/balance-dock.png)

左右に動く荷物を落として高く積むタイミングゲームです。はみ出した部分は切り落とされ、残った幅で次の荷物を受け止めます。

## 遊び方

READYでは左／右（4／6、A／D）で最初の荷物を出す側を選びます。SPACEを1回押すと左右移動を始め、次のSPACEで表示中の位置から落下します。下の積み荷と重なる位置を狙ってください。

着地したときに支えられている部分だけが残ります。幅を保てればPERFECT、はみ出して幅が縮むとTRIMです。まったく重ならないとMISSで失敗します。幅が小さくなるほど次の荷物も狭くなり、位置合わせが難しくなります。

着地すると次の荷物が反対側から自動で動き始めます。SPACEを押しっぱなしにしても次の荷物は落ちません。一度離してから次の落下を決めてください。高さが増すと移動速度も上がります。

HEIGHTは積んだ数、GOALは目標、WIDTHは現在の幅です。難易度1は12段、2は16段、3は20段を目指します。上部のSCOは得点で、着地ごとに残った幅と同じ点数、PERFECTなら追加で20点を得ます。最初の幅は80です。

RETURNのメニューでは移動・落下が止まります。FLIPは左右移動中だけ向きを反転する補助操作で、1回の挑戦につき3回使えます。FLIPSは残り回数です。READYと落下中には使えず、回数も消費しません。RESETまたはRETRYで最初からやり直します。失敗画面のSPACEで再挑戦、クリア後のSPACEで次の難易度へ進みます。

## ゲーム画面

![はみ出した部分が切れ、次に支えられる幅が小さくなる](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/e7c00dd8a7ed836d7009e9ed66f971bd079e30fb/docs/games/screenshots/balance-dock/gameplay-1.png)

はみ出した部分が切れ、次に支えられる幅が小さくなる。

![同じ位置へ正確に重ねて幅と得点を保つ](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/e7c00dd8a7ed836d7009e9ed66f971bd079e30fb/docs/games/screenshots/balance-dock/gameplay-2.png)

同じ位置へ正確に重ねて幅と得点を保つ。

![20段を目指し、高くなった積み荷へ次の荷物を合わせる](https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/e7c00dd8a7ed836d7009e9ed66f971bd079e30fb/docs/games/screenshots/balance-dock/gameplay-3.png)

20段を目指し、高くなった積み荷へ次の荷物を合わせる。

## プレイ動画

[プレイ動画を見る（30秒・音声あり）](https://zabaglione.github.io/jr800-web-emulator/videos/#balance-dock)

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
make -C games/balance-dock
make -C games/balance-dock test
```

環境の準備・一括ビルドは[Games README](https://github.com/zabaglione/jr800-web-emulator/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。
