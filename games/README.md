# JR-800 Games

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

既存6本を含めて計50本を目標に開発しています。動作確認済みの収録作品と制作予定は、[ジャンル別ゲーム一覧](../docs/games/README.md)と[開発一覧](../docs/games/roadmap.md)で区別しています。

- [パズル](../docs/games/wiki/Genre-Logic.md)
- [盤上戦略](../docs/games/wiki/Genre-Board.md)
- [カード・ダイス](../docs/games/wiki/Genre-Cards.md)
- [アクション](../docs/games/wiki/Genre-Action.md)
- [シューティング](../docs/games/wiki/Genre-Shooting.md)
- [探索](../docs/games/wiki/Genre-Adventure.md)
- [経営・シミュレーション](../docs/games/wiki/Genre-Simulation.md)
- [スポーツ・タイミング](../docs/games/wiki/Genre-Sports.md)

一括実行は `make -C games`、`make -C games test`、`make -C games run`、`make -C games debug`、`make -C games clean` です。

WASM環境の準備は次のとおりです。

```sh
emcmake cmake --preset wasm-release
cmake --build --preset wasm-release
ctest --preset wasm-release
```

`generate.py`と共通の`tools/art.py`から独自画像・面データを再生成できます。パズルの解答手順は各作品の`solutions.json`にあります。所有ROMでも検査する場合は、`JR800_GAME_ROM`にローカルROMの**絶対パス**を設定してください。ROMの配布やネットワーク送信は行いません。

タイトル画像とゲーム中3場面は[スクリーンショット](../docs/games/screenshots/)に、[検証範囲と測定値](../docs/games/validation.md)は別紙にまとめています。Wiki原稿は`docs/games/wiki/`で管理し、`python3 games/tools/wiki.py`で更新します。公開サイトでの検査記録を`--live-check`に渡した作品だけ「遊ぶ」リンクを生成します。
