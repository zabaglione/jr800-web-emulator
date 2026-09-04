# JR-800 Webエミュレーター

JR-800 Webエミュレーターは、National JR-800をブラウザー上で動かし、BASICの操作や機械語プログラムの調査を行うためのソフトウェアです。
実機から自分で取り出したROMを使い、ROMデータを外部へ送信せずにローカル環境で実行します。

現在は開発中の実験版です。
所有者が実機から取得したROMでは、明示的な仮設定の下でBASICの起動と代表的なキー入力を確認しています。
LCDの物理的な構成、未確認のキーボード行、電源管理、音声、プリンターなどには未実装または未確定の部分があります。

機械語アプリを作る方法は、[JR-800アプリ開発ガイド](docs/sdk/application-development.md)に分けて説明しています。

## ROMについて

このリポジトリにJR-800のROMは含まれていません。
利用者自身が所有する実機からROMを取得してください。

ROMや録音したWAVには権利上・プライバシー上の注意が必要です。
取得したファイルは公開、共有、Gitへの追加をせず、自分の環境内で保管してください。
以下の例では、Gitの管理対象外である`local-data/`へ保存します。

ブラウザーはJR8ROM形式と32 KiBのRAW形式を読み込めます。
推奨するのは、格納アドレスと完全性検査を持つJR8ROM形式です。
推奨ファイル名は次のとおりです。

```text
jr800-basic-8000-ffff.j8r
```

JR8ROMの拡張子は`.j8r`です。
以前から使われている32 KiBのRAWデータは`.rom`として読み込めます。
`.rom`にはアドレスや完全性検査がないため、選択時に`.j8r`への変換を案内する確認ダイアログが表示されます。
キャンセルすれば読み込みは行われず、続行すればRAWデータを`$8000-$FFFF`へ配置します。
`.bin`やその他の拡張子は受け付けません。

## 必要なもの

ROMの取得からWeb UIの実行までには、次のものが必要です。

- National JR-800実機
- JR-800のカセット出力を録音できる機器
- 48,000 Hz、16-bit PCMのWAVを保存できる録音環境
- CMake 3.25以降
- C++20対応コンパイラー
- Ninja
- Python 3.11以降
- 有効化済みのEmscripten SDK
- Node.js

使用できるかは、次のコマンドで確認できます。

```sh
cmake --version
ninja --version
python3 --version
c++ --version
emcc --version
node --version
```

## 1. ROM変換ツールをビルドする

リポジトリのルートでNative Release版をビルドします。

```sh
cmake --preset native-release
cmake --build --preset native-release
```

ROM録音の変換には、生成された`jr8wav`と`jr8rom`を使います。

## 2. 実機からROMを取得する

### 保存先を用意する

録音と変換後のROMを、Git管理外のディレクトリーへ保存します。

```sh
mkdir -p local-data/recordings
mkdir -p local-data/rom
```

### ROMを録音する

JR-800のBASICで次のコマンドを実行し、カセット出力をWAVとして録音します。

```text
MSAVE "ROM8000",&H8000,&HFFFF
```

`ROM8000`はJR-800側の保存名です。
録音ファイルは次の名前で保存してください。

```text
local-data/recordings/rom8000.wav
```

確認済みの録音条件は48,000 Hz、signed 16-bit stereo PCMで、JR-800の信号は左チャンネルに入っていました。
デコーダーはmonoまたはstereoの16-bit PCM WAVに対応し、stereoでは信号の強いチャンネルを選びます。
接続方法と録音レベルは使用する機器の説明書に従い、信号をクリップさせないでください。

### `.j8r`へ変換する

録音したWAVをJR8ROMへ変換します。

```sh
build/native-release/tools/jr8wav decode-native-msave \
  local-data/recordings/rom8000.wav \
  local-data/rom/jr800-basic-8000-ffff.j8r
```

チェックサムエラーなどがある場合、出力ROMは作成されません。
成功時の表示にはROM内容のSHA-256が含まれるため、その表示も公開しないでください。

完成したファイルを検証します。

```sh
build/native-release/tools/jr8rom verify \
  local-data/rom/jr800-basic-8000-ffff.j8r

build/native-release/tools/jr8rom inspect \
  local-data/rom/jr800-basic-8000-ffff.j8r
```

`inspect`の表示で、格納範囲が`$8000-$FFFF`を完全に覆っていることを確認してください。

## 3. Web UIをビルドして起動する

Emscripten SDKを有効にしたシェルで、WASM Release版をビルドします。

```sh
emcmake cmake --preset wasm-release
cmake --build --preset wasm-release
```

生成物をHTTPサーバーで配信します。
`index.html`をファイルとして直接開かないでください。

```sh
python3 -m http.server 8000 --directory build/wasm-release/web
```

ブラウザーで次のURLを開きます。

```text
http://127.0.0.1:8000/
```

画面上部に`Worker ready; ABI ...`と表示されれば準備完了です。

## 4. BASICを起動する

1. `JR-800 hardware model`の`.J8R / .ROM`で`jr800-basic-8000-ffff.j8r`を選びます。
2. `Boot BASIC experiment`を押します。
3. LCDにBASICの画面が出るまで待ちます。
4. 画面上の仮想キーボード、またはPCのキーボードで操作します。

`Boot BASIC experiment`は、現在BASIC起動に使っている仮のRAM、LCD、カレンダー、ポート、キーボード入力値を明示的に適用します。
これらは実機の確定した電源投入値ではありません。

## 5. WAVで配布された機械語プログラムを動かす

JR-800向けの機械語プログラムは、WAVで配布されていることがあります。
これはROMイメージではなく、作者がRAM上に作った機械語プログラムを`MSAVE`で保存したカセット音声です。

実機では、JR-800のカセット入力へWAVを再生できる機器を接続し、配布元の指示に従ってWAVを再生しながら次のコマンドを実行します。

```text
MLOAD "",,R
```

このエミュレーターでは、WAVをいったん`.j8a`へ変換してから読み込みます。
元のWAVと変換結果はROMと同様にGitへ追加せず、`local-data/`など利用者だけが読める場所へ保存してください。

```sh
mkdir -p local-data/programs

build/native-release/tools/jr8wav decode-native-program \
  path/to/program.wav \
  local-data/programs/program.j8a
```

変換に成功したら、Web UIで次の順に操作します。

1. `.J8R / .ROM`で自分の`jr800-basic-8000-ffff.j8r`を選び、`Boot BASIC experiment`を押します。
2. BASICの画面が安定するまで待ち、まだ実行中なら`Pause`を押します。
3. `RAM program .J8A`で変換した`.j8a`を選び、`Load RAM program`を押します。
4. `Resume emulation`を押します。

この読込方法は、WAVのヘッダーにあるロードアドレスへプログラムを直接配置し、実行アドレスから再開するものです。
カセット音声の再生時間や`MLOAD`内部の信号処理を再現するものではありません。

詳しい手順と制限は[機械語プログラムWAVの利用ガイド](docs/user/machine-language-wav.md)を参照してください。

## Web UIの見方

| 表示・操作 | 用途 |
| --- | --- |
| `Device layout` | JR-800を意識した配置でLCDとキーボードを表示し、初期状態ではこの配置を使います。 |
| `Workbench` | LCD、入力部、デバッガーを縦方向に確認しやすい配置へ切り替えます。 |
| LCD | 192×64ドットの現在状態を表示し、ドットは不明、消灯、点灯を区別します。 |
| 画面上のキーボード | マウスやタッチでJR-800の物理キーを押し、`CTRL`と`SHIFT`は組み合わせ入力用に保持できます。 |
| `Boot BASIC experiment` | 現在の仮設定を適用してROMを読み込み、BASICの連続実行を始めます。 |
| `Pause` | BASICの連続実行を一時停止します。 |
| `Resume emulation` | 一時停止した実行を再開します。 |
| `RAM program .J8A` | `jr8wav decode-native-program`でWAVから変換したRAMプログラムを選ぶ欄で、JR-800のROM読込後だけ使用できます。 |
| `Load RAM program` | 選択したプログラムをRAMへ配置してWAVのヘッダーに記録された実行アドレスへ移動し、停止した状態を保ちます。 |
| `Load only` | 個別のハードウェア仮説を調べるために通常は不明のままROMだけを読み込む機能で、一般利用では使いません。 |
| `Explicit experimental configuration` | RAMや周辺機器の仮入力を個別に指定する研究用設定です。 |
| `Developer debugger` | レジスター、ステップ実行、ブレークポイント、ウォッチポイント、逆アセンブル、メモリー、履歴、トレースを表示します。 |
| `RESET` | 確認後に現在のエミュレーターを強制終了して読み込んだセッションを破棄するため、再開にはROMの再読み込みが必要です。 |

ROMとRAMプログラムはブラウザー内のローカルWorkerメモリーだけに置かれます。
このWeb UIには、これらをアップロードしたり永続保存したりする処理はありません。

LCD左右のインジケーターは、現時点では候補RAMの生値を表示する診断情報です。
値が0か非0かだけで、実機の消灯・点灯を確定することはできません。
`OFF`と`ON`は配置確認用で、現在は操作できません。

## PCキーボードの対応

入力は文字ではなく、ブラウザーの`KeyboardEvent.code`で物理キー位置を判定します。
キーボード配列によって刻印と入力位置が異なる場合は、画面上の仮想キーボードを使用してください。

| PC側のキーコード | JR-800側 |
| --- | --- |
| `ShiftLeft` / `ShiftRight` | `SHIFT` |
| `ControlLeft` / `ControlRight` | `CTRL` |
| `ContextMenu` | `MENU` |
| `Enter` / `NumpadEnter` | `RETURN` |
| `Space` | `SPACE` |
| `Pause` | `BREAK` |
| `Home` | `CLS` |
| `Digit0`～`Digit9` | メインキーボードの`0`～`9` |
| `Equal` | `^` |
| `KeyA`～`KeyZ` | `A`～`Z` |
| `Semicolon` | `;` |
| `Quote` | `:` |
| `Comma` | `,` |
| `Period` | `.` |
| `F1`～`F10` | `PF1`～`PF10` |
| `Backspace` | `RUB` |
| `ArrowUp` | `↑` |
| `Shift` + `ArrowUp` | `↓` |
| `ArrowRight` | `→` |
| `Shift` + `ArrowRight` | `←` |
| `Numpad0`～`Numpad9` | テンキーの`0`～`9` |
| `NumpadMultiply` | テンキーの`*` |
| `NumpadAdd` | テンキーの`+` |
| `NumpadEqual` | テンキーの`=` |
| `NumpadSubtract` | テンキーの`-` |
| `NumpadDecimal` | テンキーの`.` |
| `NumpadDivide` | テンキーの`/` |

PC側の`ArrowDown`と`ArrowLeft`は直接割り当てていません。
下方向は`Shift` + `ArrowUp`、左方向は`Shift` + `ArrowRight`で入力します。

`Shift`を押したときの主な対応は次のとおりです。

| 通常 | `Shift`併用 |
| --- | --- |
| `1 2 3 4 5 6 7 8 9 0` | `! " # $ % & ' ( ) @` |
| `^` | `¥` |
| `;` | `?` |
| `:` | `-` |
| `,` | `<` |
| `.` | `>` |
| `PF1`～`PF10` | `PF11`～`PF20` |
| `RUB` | `INS` |
| `CLS` | `HOME` |

`CTRL`と数字キーの組み合わせは、順に`ERASE`、`HCOPY`、`SAVE`、`LOAD`、`VERIFY`、`OPEN`、`CLOSE`、`CTRL`、`CAP.L`、`GRAPH`です。
`CTRL` + `^`は`KANA`、`CTRL` + `BREAK`は`CLEAR`、`CTRL` + `CLS`は`LIST`、`CTRL` + `RUB`は`POKE`として表示されます。
機能モードと制御モードの確定していない文字キーは、Web UI上で断定した機能名へ置き換えません。

PCキーボードが反応しない場合は、ファイル選択、入力欄、ボタン、リンクなどからフォーカスを外し、LCD周辺の操作要素ではない場所を一度クリックしてください。
ブラウザーが非表示になった場合やフォーカスを失った場合は、押したままのキー状態を自動的に解除します。

## コマンドラインでROM起動を確認する

画面操作を行わず、決められた命令数だけROMを実行して異常停止の有無を確認できます。

```sh
build/native-release/tools/jr8run jr800 \
  --basic-boot-experiment \
  --max-instructions 1000000 \
  --max-suspended-cycles 64000000 \
  local-data/rom/jr800-basic-8000-ffff.j8r
```

これは対話用UIではなく、同じ仮設定をNative環境で再現するための確認手段です。
CPU fault、未対応アクセス、停止時間上限などに到達した場合は失敗として終了します。

## 困ったとき

- `.j8r`または`.rom`を選べない場合はファイルの拡張子を確認し、`.rom`は32 KiBちょうどにしてください。
- `index.html`を直接開いて動かない場合は、HTTPサーバー経由で開いてください。
- `RESET`後はセッションが破棄されるため、ROMを選び直してください。
- `Load only`で進まない場合は、通常利用向けの`Boot BASIC experiment`を使ってください。
- WAVを`RAM program .J8A`で選べない場合は、先に`jr8wav decode-native-program`で`.j8a`へ変換してください。
- `Load RAM program`が使えない場合は、先にJR8ROMを読み込み、実行中なら`Pause`を押してください。
- LCD表示や周辺機器が実機と異なる場合は、開発中の未実装または未確定な機能である可能性があります。

## 関連文書

- [JR-800アプリ開発ガイド](docs/sdk/application-development.md)
- [機械語プログラムWAVの利用ガイド](docs/user/machine-language-wav.md)
- [JR8ROM形式](docs/formats/jr8rom-v1.md)
- [JR8ROMの作成と検証](docs/sdk/jr8rom.md)
- [Native JR-800ランナー](docs/core/jr800-headless-runner.md)
- [ROM取り扱い方針](ROM_POLICY.md)

## ライセンスと出典

新規に作成したプロジェクトコードは、個別の記載がない限りMIT Licenseです。
JR-800のROM、ROM由来データ、録音WAVは配布対象に含まれません。
詳細は[LICENSE](LICENSE)、[PROVENANCE.md](PROVENANCE.md)、[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)を参照してください。
