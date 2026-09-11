# JR-800 Games

パズルを中心とした15作品は**各40面・合計600面**のチャレンジに対応します。規定手数と追加目標による3段階評価、5文字の面指定コード、25文字の全評価コードを搭載しています。[遊び方とパスワード](../docs/games/wiki/Puzzle-Challenges.md)・[盤面と実装の設計](../docs/games/puzzle-design.md)を参照してください。


SDKサンプルとは別に開発する、標準16KB RAM向けの独立した機械語ゲームです。
ゲームの規則・描画・操作はHD6301プログラム内で実行します。

## ビルド

```sh
cmake --preset native-release
cmake --build --preset native-release --target jr8as jr8ld
make -C games/box-shift
make -C games/box-shift test
make -C games/box-shift run
make -C games/box-shift debug
make -C games/box-shift clean
```

`test`・`run`・`debug`には、先にWASM版のビルドが必要です。`run`はヘッドレス実行結果と画像、`debug`は追加のレジスター・シンボルを出力します。対話プレイはWebエミュレーターから行います。

操作はテンキー8/2/4/6またはW/S/A/D、SPACEで決定、RETURNで取消・メニューです。BREAKでJR-HuBASIC 1.0へ戻ります。この終了処理はBASICのプログラムと変数を消去します。必要な内容はゲームを読み込む前に保存してください。

新規ソースとドット画像はMIT Licenseです。ROMは含みません。実機動作・カセット転送は未検証です。

## ジャンルから探す

既存6本を含めて計50本を収録しています。作品の概要とジャンルは、[ジャンル別ゲーム一覧](../docs/games/README.md)と[開発一覧](../docs/games/roadmap.md)で確認できます。

- [パズル](../docs/games/wiki/Genre-Logic.md)
- [盤上戦略](../docs/games/wiki/Genre-Board.md)
- [カード・ダイス](../docs/games/wiki/Genre-Cards.md)
- [アクション](../docs/games/wiki/Genre-Action.md)
- [シューティング](../docs/games/wiki/Genre-Shooting.md)
- [探索](../docs/games/wiki/Genre-Adventure.md)
- [経営・シミュレーション](../docs/games/wiki/Genre-Simulation.md)
- [スポーツ・タイミング](../docs/games/wiki/Genre-Sports.md)

一括実行は `make -C games`、`make -C games test`、`make -C games run`、`make -C games debug`、`make -C games clean` です。

DebugとReleaseのWebビルドは `build/games` を共用するため、同じ作業フォルダーでは順番に実行してください。

WASM環境の準備は次のとおりです。

```sh
emcmake cmake --preset wasm-release
cmake --build --preset wasm-release
ctest --preset wasm-release
```

`generate.py`と`tools/visual_art.py`から作品ごとのタイトル・面データを、`tools/hud_layouts.py`・`tools/hud_custom.py`からHUDを再生成できます。パズルの解答手順は各作品の`solutions.json`にあります。所有ROMでも検査する場合は、`JR800_GAME_ROM`にローカルROMの**絶対パス**を設定してください。ROMの配布やネットワーク送信は行いません。

タイトル画像とゲーム中3場面は[スクリーンショット](../docs/games/screenshots/)に、[検証範囲と測定値](../docs/games/validation.md)は別紙にまとめています。Wiki原稿は`docs/games/wiki/`で管理し、`python3 games/tools/wiki.py`で更新します。公開サイトでの検査記録を`--live-check`に渡した作品だけ「遊ぶ」リンクを生成します。公開済みソースのコミットIDを`--image-revision`に渡すと、Wikiの画像をその版に固定できます。

## 画面の制作

50作品のタイトルを個別の構図で制作し、HUDも左右の計器、荷札、カード卓、帳簿など作品に合わせて配置しています。[画面設計とメモリー配置](../docs/games/visual-design.md)に制作方針をまとめています。
