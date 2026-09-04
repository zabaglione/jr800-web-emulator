# JR-800 Webエミュレーター文書

エミュレーターを使う場合は、まずプロジェクトルートの[README](../README.md)を参照してください。
実機からROMを録音して`.j8r`へ変換し、Web UIでBASICを起動するところまでを説明しています。

## 機械語プログラム

- [WAVで配布された機械語プログラムを実機とエミュレーターで動かす](user/machine-language-wav.md)

## アプリ開発

- [JR-800アプリ開発ガイド](sdk/application-development.md)
- [アセンブリー構文](sdk/assembly-syntax-v1.md)
- [リンクスクリプト](sdk/link-script-v1.md)
- [コマンドラインテスト](sdk/headless-testing.md)
- [逆アセンブル](sdk/disassembly.md)
- [jr8nm](sdk/jr8nm.md)
- [jr8objdump](sdk/jr8objdump.md)
- [jr8objcopy](sdk/jr8objcopy.md)
- [jr8rom](sdk/jr8rom.md)
- [JR-800コマンドライン実行](core/jr800-headless-runner.md)

## ファイル形式

| 拡張子 | 形式 | 用途 |
| --- | --- | --- |
| `.rom` | 32 KiB RAW ROM | `$8000-$FFFF`へ固定配置する従来形式。Web UIで確認後に読込 |
| `.j8r` | [JR8ROM](formats/jr8rom-v1.md) | アドレスと完全性情報を持つ推奨ROM形式 |
| `.jro` | [JRO](formats/jro-v1.md) | 再配置可能オブジェクト |
| `.j8l` | [JR8LD link script](sdk/link-script-v1.md) | メモリー領域とセクション配置 |
| `.j8a` | [JR8APP](formats/jr8app-v1.md) | ロード可能な機械語アプリ |
| `.j8d` | [JR8DBG](formats/jr8dbg-v1.md) | ソース位置とシンボルのデバッグ情報 |
| `.wav` | [MSAVE録音／FSK](formats/rom-dump-transport-v1.md) | 実機のROMまたはRAMプログラム録音 |

ROM、ROMを含むWAV、配布された機械語プログラム、変換結果はリポジトリへ追加しないでください。
