# JR-800アプリ開発ガイド

この文書は、プロジェクト付属SDKでHD6301V1向けの機械語アプリを作り、Native環境またはWeb UIで確認する手順を説明します。
エミュレーターを実機ROMで起動する方法は、プロジェクトルートの[README](../../README.md)を参照してください。
実機用に配布された機械語プログラムWAVを変換して動かす方法は、[機械語プログラムWAVの利用ガイド](../user/machine-language-wav.md)を参照してください。

## 現在のSDKの位置づけ

現在のSDKは、プロジェクト独自のアセンブラー、リンカー、実行・デバッグ用ツールを使うROM不要の開発環境です。
生成したJR8APPは、初期値0の合成64 KiB RAM上で実行します。

この実行モデルはSDKとデバッガーの動作確認用です。
JR8APPをそのままJR-800実機へ転送して動かせることや、実機のメモリーマップ、I/O、起動方法との一致を保証するものではありません。

実機ROMを使う`jr8run jr800`と、SDKアプリを合成RAMで動かす`jr8run app.j8a`は別の実行経路です。
Web UIには、SDKアプリを合成RAMでデバッグする`Synthetic application`と、BASIC起動後のJR-800 RAMへ変換済みプログラムを読み込む`RAM program .J8A`があります。
このガイドでは前者を扱います。

## 必要な環境

- CMake 3.25以降
- C++20対応コンパイラー
- Ninja
- Python 3.11以降
- GNU Make互換の`make`

Web UIでも確認する場合は、Emscripten SDKとNode.jsも必要です。

## SDKツールをビルドする

リポジトリのルートでNative Release版をビルドします。

```sh
cmake --preset native-release
cmake --build --preset native-release
```

ツールは`build/native-release/tools/`に生成されます。

| ツール | 用途 |
| --- | --- |
| `jr8as` | アセンブリーソースをJROオブジェクトへ変換します。 |
| `jr8ld` | 1個以上のJROを配置・結合し、JR8APPとデバッグ情報を生成します。 |
| `jr8run` | JR8APPを合成RAMへ読み込み、実行、停止、検査、テストを行います。 |
| `jr8nm` | JROまたはJR8DBGのシンボルを表示します。 |
| `jr8objdump` | JROまたはJR8APPを検証し、構造と逆アセンブル結果を表示します。 |
| `jr8objcopy` | JROまたはJR8APPから、明示した格納済みバイト範囲を取り出します。 |

バージョンは、たとえば次のように確認できます。

```sh
build/native-release/tools/jr8as --version
build/native-release/tools/jr8ld --version
build/native-release/tools/jr8run --version
```

## 最短でサンプルを動かす

ROM不要のサンプル`write-watch`を使います。

```sh
cd sdk/examples/write-watch

export JR8AS=../../../build/native-release/tools/jr8as
export JR8LD=../../../build/native-release/tools/jr8ld
export JR8RUN=../../../build/native-release/tools/jr8run

make
make run
make debug
make test
```

`make`は、`main.s`をアセンブルして`main.jro`を作り、`memory.j8l`に従って次のファイルを生成します。

```text
build/write-watch.j8a
build/write-watch.j8d
build/write-watch.map
build/write-watch.sym
```

`make run`は命令数を制限してアプリを実行し、最終メモリー値を検査します。
`make debug`は`result + 1`への書き込みで停止します。
`make test`は停止理由、レジスター、サイクル数、シンボル、メモリーをまとめて検査します。

サンプルには次の追加ターゲットがあります。

| コマンド | 確認内容 |
| --- | --- |
| `make run-to` | 指定アドレスまで実行します。 |
| `make run-to-source` | `main.s:9`まで実行します。 |
| `make run-to-symbol` | `loop`シンボルまで実行します。 |
| `make break-if` | 条件式が成立したときだけ停止します。 |
| `make step-over` | 1命令をステップオーバーします。 |
| `make step-out` | リターンまでの実行を、上限付きで試します。 |
| `make trace` | 指定範囲への書き込みだけを表示します。 |
| `make clean` | このサンプルの生成物だけを削除します。 |

## ファイルの流れ

| 拡張子 | 役割 |
| --- | --- |
| `.s` | アセンブリーソースです。 |
| `.jro` | 再配置可能なJROオブジェクトです。 |
| `.j8l` | メモリー領域とセクション配置を定義するリンクスクリプトです。 |
| `.j8a` | 実行可能なJR8APPアプリです。 |
| `.j8d` | ソース位置とシンボルを持つJR8DBGデバッグ情報です。 |
| `.lst` | アセンブラーのリスティングです。 |
| `.map` | リンカーの配置結果です。 |
| `.sym` | リンカーのシンボル一覧です。 |

基本の流れは次のとおりです。

```text
main.s
  -> jr8as
main.jro
  -> jr8ld + memory.j8l
app.j8a + app.j8d + app.map + app.sym
  -> jr8run または Web UI
```

## アセンブリーソースを書く

`write-watch/main.s`は、2つの即値をメモリーへ保存した後、無限ループへ入る最小例です。

```asm
.section .text, code
.global entry
.global result
.local loop
entry:
    LDAA #$42
    STAA result
    LDAA #$99
    STAA result + 1
loop:
    BRA loop

.section .bss, bss
result:
    .space 2
```

主な構文規則は次のとおりです。

- 命令ニーモニックとディレクティブ名は大文字・小文字を区別しません。
- シンボル名は大文字・小文字を区別します。
- 16進数は`$2A`、2進数は`%10101010`、10進数は`42`と書きます。
- 即値には`#`を付けます。
- `;`から行末まではコメントです。
- `.section`で`code`、`data`、`bss`のセクションを選びます。
- `.global`は外部公開、`.local`はローカル定義、`.extern`は別オブジェクトのシンボル参照です。
- `.byte`、`.word`、`.space`でデータ領域を定義できます。

ソースはUTF-8で読み込まれますが、言語上の識別子はASCIIです。
現在のバージョンにはinclude、macro、条件付きアセンブルはありません。
未対応の命令形式や範囲外の値は、推測して変換せずエラーになります。

完全な規則は[アセンブリー構文version 1](assembly-syntax-v1.md)を参照してください。

## リンクスクリプトを書く

`write-watch/memory.j8l`は、ゼロページとコード領域を明示的に分けます。

```text
target hd6301v1
entry entry
region ZP $0000 $0100
region CODE $0200 $0100
place .text CODE
place .bss ZP
```

`target`は入力JROと一致するCPUプロファイルを指定します。
`entry`は開始位置となるglobalシンボルです。
`region`は16-bitアドレス空間内の開始位置と長さを定義します。
`place`は、セクション名を領域へ正確に割り当てます。

ワイルドカード配置や暗黙の配置はありません。
空でない全セクションを、重複や領域越えが起きないように明示してください。

完全な規則は[リンクスクリプトversion 1](link-script-v1.md)を参照してください。

## 手動でアセンブル、リンクする

この節のコマンドは、リポジトリのルートで実行します。

次の例は、リスティング、デバッグ情報、マップ、シンボル一覧をすべて生成します。

```sh
build/native-release/tools/jr8as \
  --target hd6301v1 \
  --listing app.lst \
  -o main.jro \
  main.s

build/native-release/tools/jr8ld \
  --script memory.j8l \
  -o app.j8a \
  --debug app.j8d \
  --map app.map \
  --symbols app.sym \
  main.jro
```

複数ソースの場合は、各`.s`を別々の`.jro`へ変換し、すべてのJROを`jr8ld`へ順番に渡します。
別ファイルのglobalシンボルを使う側では、`.extern`で宣言します。
オブジェクトの指定順は配置順の一部です。

## Native環境で実行・テストする

通常実行の最小例です。

```sh
build/native-release/tools/jr8run \
  --max-instructions 100 \
  app.j8a
```

デバッグ情報を読み込み、アドレス`$0001`への書き込みで停止する例です。

```sh
build/native-release/tools/jr8run \
  --debug app.j8d \
  --watch write:0x0001 \
  --max-instructions 100 \
  app.j8a
```

停止理由と最終状態を自動検査できます。

```sh
build/native-release/tools/jr8run \
  --debug app.j8d \
  --watch write:0x0001 \
  --max-instructions 100 \
  --expect-stop memory-watchpoint \
  --expect 'PC == 0x020A && A == 0x99' \
  --expect 'mem8[0x0000] == 0x42' \
  app.j8a
```

`--expect`にはレジスター、フラグ、`cycles`、`mem8[address]`、一致するJR8DBGの`symbol("name")`を使えます。
式の評価不能、未知状態、範囲外アドレス、0除算、見つからないシンボルは明示的な失敗になります。

詳細は[ヘッドレスJR8APPテスト](headless-testing.md)を参照してください。

## Web UIで実行する

リポジトリのルートでWASM版をビルドし、HTTPサーバーを起動します。

```sh
emcmake cmake --preset wasm-release
cmake --build --preset wasm-release
python3 -m http.server 8000 --directory build/wasm-release/web
```

`http://127.0.0.1:8000/`を開き、次の順で読み込みます。

1. `Synthetic application`の`.J8A`で`app.j8a`を選びます。
2. ソース表示やシンボル操作を使う場合は、`.J8D`で対応する`app.j8d`も選びます。
3. `Initial SP`を確認します。
4. `Load application`を押します。
5. `Developer debugger`を開き、`Run`、`Step`、ブレークポイント、ウォッチ、メモリー、履歴、トレースを操作します。

初期SPの既定値は`$01FF`です。
JR8DBGは対応するJR8APPと整合性が一致するときだけ読み込まれます。

## 生成物を調べる

シンボルを表示します。

```sh
build/native-release/tools/jr8nm main.jro
build/native-release/tools/jr8nm app.j8d
```

JROまたはJR8APPを検証して表示します。

```sh
build/native-release/tools/jr8objdump main.jro
build/native-release/tools/jr8objdump app.j8a
build/native-release/tools/jr8objdump --debug app.j8d app.j8a
```

明示した格納済み範囲を取り出します。

```sh
build/native-release/tools/jr8objcopy \
  --segment 0 \
  -o app-segment.bin \
  app.j8a
```

`jr8objcopy`は、未配置データ、再配置情報を失うJRO、未指定の隙間を推測して出力しません。

## 新しいアプリを始めるとき

最初は[sdk/examples/write-watch](../../sdk/examples/write-watch)を別ディレクトリーへコピーし、次の3ファイルだけを変更する方法が簡単です。

- `main.s`: アプリ本体
- `memory.j8l`: メモリー領域とセクション配置
- `Makefile`: 入出力名と実行・テスト条件

アプリの動作を増やすときは、まず短い命令数上限で`make test`が通る状態を作り、その上に処理を追加してください。
未対応命令や実機固有I/Oが必要になった場合は、SDK側で値を仮定せず、プロジェクトの根拠資料と未確定事項を確認してください。

## 詳細資料

- [アセンブリー構文version 1](assembly-syntax-v1.md)
- [リンクスクリプトversion 1](link-script-v1.md)
- [ヘッドレスJR8APPテスト](headless-testing.md)
- [シンボル表示](jr8nm.md)
- [逆アセンブル](disassembly.md)
- [JRO/JR8APP表示](jr8objdump.md)
- [格納済みバイトの抽出](jr8objcopy.md)
- [JRO形式](../formats/jro-v1.md)
- [JR8APP形式](../formats/jr8app-v1.md)
- [JR8DBG形式](../formats/jr8dbg-v1.md)
