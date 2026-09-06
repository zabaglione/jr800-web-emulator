# JR-800アプリ開発SDK

SDKの導入、アセンブル、リンク、Native実行、テスト、Web UIでのデバッグは、[JR-800アプリ開発ガイド](../docs/sdk/application-development.md)を参照してください。

最小のMakeベースプロジェクトは[examples/write-watch](examples/write-watch)にあります。
このサンプルはROMを使わず、合成64 KiB RAMの実行モデル上で動作します。

JR-800本体のLCDとキー入力を使う[機械語サンプルとゲーム](examples/lcd/README.md)もあります。
文字表示、市松模様、カウンター、跳ね返るスプライト、テンキーカーソルを、通常のWebエミュレーターで実行できます。
各サンプルにソース、Makefile、日本語の解説と自動検査を用意しています。
SPACEでジャンプする恐竜ゲームは、RAM上のソフトウェアPCGで恐竜やサボテンを描きます。
[RELIC DIVE](examples/lcd/07-relic-dive/README.md)は、下降専用の迷宮、12種類の敵、3段階の難易度とブラウザー中断保存を備えています。
