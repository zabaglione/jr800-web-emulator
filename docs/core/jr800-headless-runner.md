# JR-800をコマンドラインで実行する

`jr8run jr800`は、所有者が用意したJR8ROMを`Jr800Machine`で命令数上限付き実行します。
画面操作ではなく、自動検査や障害位置の絞り込みに使う機能です。
指定する仮設定が実機の電源投入状態と一致するとは断定しません。

## 入力とROMの扱い

入力は、`$8000-$FFFF`の32,768アドレスを完全に覆うJR8ROMバージョン1の`.j8r`です。
範囲は複数の隣接セグメントに分かれていても構いません。
先頭、中間、末尾のどこか1バイトでも欠けていれば拒否します。
コマンドライン版はRAWの`.rom`を受け付けないため、先に`.j8r`へ変換してください。

入力はローカルで読み、複製ファイルを書きません。
通常の結果には、命令数、サイクル数、停止分類、粗いメモリー領域、割り込み回数、キーボード読込回数、LCDの集計値など、内容を復元できない集計だけを表示します。
ROMバイト、完全性ダイジェスト、正確な障害アドレス、レジスター、キーボードのアドレスと値、LCD画像は表示しません。
ROMとROMを含む実行記録はリポジトリへ追加しないでください。

## コマンド

```text
jr8run jr800 [--max-instructions <count>]
                [--max-suspended-cycles <count>]
                [--basic-boot-experiment]
                [--reset-sp <word>]
                [--reset-x <word>]
                [--reset-a <byte>]
                [--reset-b <byte>]
                [--reset-cc <value:known-mask>]
                [--internal-ram-initial <byte>]
                [--standard-ram-initial <byte>]
                [--expansion-ram-initial <byte>]
                [--lcd-unknown-data <byte>]
                [--calendar-address-source <a0-a3|a1-a4|a2-a5|a3-a6|a4-a7|a5-a8>]
                [--calendar-upper-read <zero|one>]
                [--calendar-cpu-cycle-ratio <e030-nominal-1.2288mhz>]
                [--port1-pins <value:known-mask>]
                [--port2-pins <value:known-mask>]
                [--ram-standby <valid|invalid>]
                [--keyboard-window-value <byte>]
                [--keyboard-response <address:byte>]...
                <rom.j8r>
```

数値は10進、`0x`付き16進、`$`付き16進です。
命令数上限の既定値は100,000、停止状態のサイクル上限は65,536です。
これらは処理資源の上限であり、実機の性質ではありません。

`--basic-boot-experiment`は、Web UIの`Boot BASIC experiment`と同じ現在の仮入力を使います。
CPU内RAM、標準RAM、拡張RAMをゼロ、LCD不明読込値をゼロ、calendarをA0-A3と上位ゼロで接続し、CPUサイクルとの自動連動を無効にします。
Port 1は`$FF`、Port 2下位は`$1E`、RAM standbyはinvalid、キーボード範囲は`$FF`です。
リセット時のレジスター値と実行上限は変更しません。

この一括指定と個別のハードウェア入力オプションは併用できません。
一括指定を暗黙に変更しないためです。

## 個別の仮入力

指定しない入力は不明または未接続のままです。

- SP、X、A、Bは、指定したレジスターだけを既知にします。
- `--reset-cc`は`$2F`内のH/N/Z/V/Cだけを既知にでき、known mask外の値ビットを拒否します。
- PCはリセットベクターから読み、高位CCRビットとreset Iは変更できません。
- CPU内RAM、標準RAM、拡張RAMは別々の入力です。
- 拡張RAMには標準RAMの指定も必要です。
- LCDとcalendarは必要な組を完全に指定した場合だけ接続します。
- `e030-nominal-1.2288mhz`は、calendar接続時だけCPU E cycleを`2/75`で変換します。
- Port入力とRAM standbyは、指定しなければ不明です。
- `--keyboard-window-value`は`$0C00-$0FFF`の全1,024アドレスへ同じRAW値を設定します。
- `--keyboard-response`は指定した1アドレスだけを上書きし、同じアドレスの重複、1バイト範囲外、キーボード範囲外はエラーになります。

## 終了条件

要求した命令数を完了した場合だけ成功終了します。
CPU fault、停止サイクル上限、無効または不完全なJR8ROM、不正なオプションは失敗です。
割り込み要求をモデル化していない状態でCPUがsleepを続けた場合、有限の上限で失敗するのは意図した動作です。

結果は内部割り込み種別ごとの回数、直近の割り込み後に完了した命令数、calendar alarm terminal、Port 2 timer outputなどを固定語彙で表示します。
LCD接続時は、置換読込回数とunknown／off／onのドット数を集計します。
キーボードについては、実行開始後の読込試行数と異なるアドレス数だけを集計します。
これらの情報は、実行経路へ到達したかを確認する診断値であり、物理信号や実機表示を証明するものではありません。
