# ISAメタデータ

`hd6301-minimal.json`は、プロジェクトが管理する命令メタデータの原本です。
確認済みの命令から段階的に追加し、参照エミュレーターや非互換ツールチェーンの表をコピーして作りません。

## バージョン1のデータ契約

最上位の`schema_version`は、`isa/tools/generate_isa.py`が検査する契約を指定します。
未知フィールド、必須フィールド不足、識別子重複、エンコード重複、未対応schema versionはエラーです。
旧形式へのfallbackはありません。

JSONは次の項目を持ちます。

- `sources`：一次資料の識別情報と固定URL
- `status_flags`：`H`、`I`、`N`、`Z`、`V`、`C`のbit順
- `addressing_modes`：アドレッシング形式とオペランドバイト数
- `instruction_classes`：デバッガー用の制御フロー分類とstep-over方針
- `cpu_profiles`：正確なCPUプロファイルと未確定machine選択
- `instructions`：確認済みのエンコード、長さ、cycle、flag、分類、operation、profile、出典位置

各命令の主要フィールドは次のとおりです。

| フィールド | 意味 |
| --- | --- |
| `id` | 小文字の安定したメタデータ／テスト識別子 |
| `opcode` | 大文字`0xNN`の1バイトopcode |
| `mnemonic` | 標準の大文字mnemonic |
| `addressing_mode` | `addressing_modes`で宣言した識別子 |
| `operand_bytes` | opcode後のバイト数 |
| `base_cycles` | 資料に記載された基本cycle数 |
| `flags` | 読み取るflagと、書込／保持／未定義出力の完全な区分 |
| `classification` | debugger stepping用の制御フロー分類 |
| `operation` | C++実装が結び付ける安定したdispatch識別子 |
| `profiles` | 記録値が適用される正確なCPUプロファイル |
| `source_locations` | 一次資料とpage／table位置 |

CPUプロファイル間で命令情報を継承しません。
同じopcodeでもcycle数などが異なる場合があるため、記録値がすべて一致する場合だけ同じ行を共有します。
`jr800_unresolved`はfail-closedの未設定値であり、命令、clock、memory decode、bus timingを暗黙に与えません。

## 生成物

CMakeはbuild treeへ`jr800/isa/instruction_metadata.hpp`と実装を生成します。
生成APIは次の機能を提供します。

- `find_encoding`：assembler用のencoding検索
- `decode_instruction`：disassemblerとCPU dispatch用のdecode
- `Operation`：C++命令処理へ結び付ける識別子
- `is_step_over_candidate`：debugger分類
- `instruction_test_cases`：メタデータから作るテスト一覧

generatorは命令の実行処理を生成しません。
CPU処理、flag計算、memory access順、cycle状態変化は、確認したC++コードで実装します。

生成せずにJSONを検証できます。

```sh
python3 isa/tools/generate_isa.py \
  --input isa/hd6301-minimal.json \
  --validate-only
```

通常のCMakeビルドは検証と生成を自動実行します。
生成物にtimestampとabsolute pathを含めないため、同じ入力から同じバイト列を得られます。
