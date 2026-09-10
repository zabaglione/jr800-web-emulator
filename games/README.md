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

## 6本のゲーム

| ゲーム | 内容 |
|---|---|
| [BOX SHIFT](box-shift/README.md) | 20面の箱押し |
| [MIRROR LINK](mirror-link/README.md) | 20面の鏡反射 |
| [STEP STRIKE](step-strike/README.md) | 20面の時間停止戦術パズル |
| [CIRCUIT DECK](circuit-deck/README.md) | 12種類のカード・9戦のデッキ構築 |
| [POCKET FACTORY](pocket-factory/README.md) | 2資源・2加工機・12課題 |
| [ARC DUEL](arc-duel/README.md) | 6地形・3難易度・2勝先取の弾道対戦 |

一括実行は `make -C games`、`make -C games test`、`make -C games run`、`make -C games debug`、`make -C games clean` です。

WASM環境の準備は次のとおりです。

```sh
emcmake cmake --preset wasm-release
cmake --build --preset wasm-release
ctest --preset wasm-release
```

`generate.py`と共通の`tools/art.py`から独自画像・面データを再生成できます。パズルの解答手順は各作品の`solutions.json`にあります。所有ROMでも検査する場合は、`JR800_GAME_ROM`にローカルROMの**絶対パス**を設定してください。ROMの配布やネットワーク送信は行いません。

タイトル画像とゲーム中3場面は[スクリーンショット](../docs/games/screenshots/)に、[検証範囲と測定値](../docs/games/validation.md)は別紙にまとめています。Wiki原稿は`docs/games/wiki/`で管理し、`python3 games/tools/wiki.py`で更新します。公開サイトでの検査記録を`--live-check`に渡した作品だけ「遊ぶ」リンクを生成します。
