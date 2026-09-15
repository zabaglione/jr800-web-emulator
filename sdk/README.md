# JR-800アプリ開発SDK

SDKの導入、アセンブル、リンク、Native実行、テスト、Web UIでのデバッグは、[JR-800アプリ開発ガイド](../docs/sdk/application-development.md)を参照してください。

最小のMakeベースプロジェクトは[examples/write-watch](examples/write-watch)にあります。
このサンプルはROMを使わず、合成64 KiB RAMの実行モデル上で動作します。

JR-800本体のLCDとキー入力を使う[機械語サンプルとゲーム](examples/lcd/README.md)もあります。
文字表示、市松模様、カウンター、跳ね返るスプライト、テンキーカーソルを、通常のWebエミュレーターで実行できます。
各サンプルにソース、Makefile、日本語の解説と自動検査を用意しています。
恐竜ゲームはSPACEを押す長さでジャンプの高さを変え、サボテン・大岩・木箱・穴を越えます。
恐竜や障害物はRAM上のソフトウェアPCGで描きます。
[RELIC DIVE](examples/lcd/07-relic-dive/README.md)は、下降専用の迷宮、12種類の敵、3段階の難易度とブラウザー中断保存を備えています。

[省RAM機械語ライブラリ](lib/README.md)とサンプル08〜13を追加しました。各機能を小さく試す例と、3レーンの回収・回避ゲーム[STAR COURIER](examples/lcd/13-star-courier/README.md)があります。

[POLYGON FIGHTER](examples/lcd/14-polygon-fighter/README.md)は、陰影付きの戦闘機ポリゴンを機械語で描画・回転するテックデモです。

[poly3d](lib/poly3d/README.md)は、三角錐などの小さな凸多面体を再利用できる描画ライブラリです。[実証サンプル15](examples/lcd/15-poly3d-lab/README.md)と、[GATE FLIGHT](examples/lcd/16-gate-flight/README.md)・[TURN MATCH](examples/lcd/18-turn-match/README.md)の独立したサンプルを用意しています。射撃ゲームの完成版は[POLY DEFENDER](../games/poly-defender/README.md)として公開しています。
