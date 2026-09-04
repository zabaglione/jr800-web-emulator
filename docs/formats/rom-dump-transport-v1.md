# JR-800のMSAVE録音とプロジェクトFSK形式

## 実機のMSAVE録音

JR-800のROM取得とRAM上の機械語プログラム保存は、どちらもBASICの`MSAVE`が出力するカセット音声をWAVへ録音します。
1回の`MSAVE`につき1個の録音ファイルです。
録音開始後、操作していない区間が先頭の無音として残っても、複数回の録音や複数データを意味しません。

信号は、先頭の無音、スタートビット相当の音、ヘッダー部、データ部の順に現れます。
デコーダーは音声から開始位置を探すため、録音ソフトで無音を厳密に切り詰める必要はありません。
ただし、信号部分の欠落、過大入力によるクリッピング、途中停止は復元できません。

ROMを録音する例は次のとおりです。

```text
MSAVE "ROM8000",&H8000,&HFFFF
```

録音したWAVをアドレス付きJR8ROMへ変換します。

```sh
build/native-release/tools/jr8wav decode-native-msave \
  local-data/recordings/rom8000.wav \
  local-data/rom/jr800-basic-8000-ffff.j8r
```

`decode-native-msave`は、ネイティブヘッダーとデータのチェックサムが正しい場合だけ、録音から得た開始アドレスへ1セグメントのJR8ROMを書きます。
アドレスを後から入力し直す必要はありません。
失敗時は出力しません。

必要に応じて、別々に録音した2個のWAVが同じ内容になるかを比較できます。
これは1個の録音に2ブロックあるという意味ではありません。
各WAVは、それぞれ1回の`MSAVE`を録音したものです。

```sh
build/native-release/tools/jr8wav verify-native-msave \
  first.wav second.wav output.j8r
```

2入力形式は、入力WAV自体が異なり、復号したファイル名、アドレス、長さ、データが一致する場合だけ出力します。
通常の利用者手順では`decode-native-msave`による1録音の変換で構いません。

RAM上の機械語プログラムも同じMSAVE音声ですが、ROMではありません。
`decode-native-program`でJR8APPへ変換します。
詳しくは[機械語プログラムWAVの利用ガイド](../user/machine-language-wav.md)を参照してください。

## プロジェクト定義のFSK形式

本プロジェクトには、実機のMSAVE形式とは別に、合成テスト用のROM dump transport version 1があります。
CRC付きフレームと最終SHA-256を、決定的な1,200 baud FSKとしてmono 16-bit PCM WAVへ格納します。
この形式をJR-800実機のカセット形式やJR8ROMと同一視しないでください。

すべての複数バイト整数は符号なしビッグエンディアンです。
RIFF/WAVE部分だけはWAVE仕様に従いリトルエンディアンです。

## ROM dump frame

各フレームは24バイトのヘッダー、可変長payload、2バイトCRC-16で構成します。

| オフセット | サイズ | フィールド | 条件 |
| ---: | ---: | --- | --- |
| 0 | 4 | magic | ASCII `J8RF` |
| 4 | 1 | version | `1` |
| 5 | 1 | frame type | `1`はblock、`2`はfinal |
| 6 | 2 | header size | `24` |
| 8 | 2 | segment address | dumpの先頭CPUアドレス |
| 10 | 2 | block number | blockは0始まり、finalは`$FFFF` |
| 12 | 2 | block count | セグメント内block数 |
| 14 | 2 | payload length | ヘッダー後、CRC前のバイト数 |
| 16 | 4 | block offset | segment addressからの位置。finalはゼロ |
| 20 | 4 | total length | セグメント全体の論理バイト数 |
| 24 | 可変 | payload | blockデータまたはfinal digest |
| 末尾 | 2 | frame CRC | ヘッダーとpayloadのCRC-16/CCITT-FALSE |

CRC-16/CCITT-FALSEは多項式`$1021`、初期値`$FFFF`、入出力のbit反転なし、final XORなしです。
ASCIIの`123456789`に対するcheck valueは`$29B1`です。

block frameは1～4,096バイトを持ち、既定block sizeは256バイトです。
block numberは共通block count未満、offsetとpayload lengthの和はtotal length以下でなければなりません。

final frameはblock number `$FFFF`、offset 0、payload 32バイトです。
payloadはアドレス順に復元したセグメントに対するSHA-256です。
同じ復元単位の全フレームはsegment address、block count、total lengthが一致しなければなりません。
空のセグメント、16ビット範囲外、重複、未格納範囲は拒否します。

デコーダーは、不正形式、未知version、CRC不一致、メタデータ不一致、重複、欠落block、finalの欠落または重複、SHA-256不一致を区別します。
空きを推測値で埋めません。

## FSK packetとWAV

各フレームを別々のsignal burstへ格納します。

| フィールド | サイズ |
| --- | ---: |
| preamble | `$55`を24バイト |
| synchronization | ASCIIの`J8FS`を4バイト |
| frame length | ビッグエンディアン4バイト |
| frame | 指定長のROM dump frame |

パケットは非同期8N1です。
start bitは0、データ8ビットはLSB first、stop bitは1です。
0は1,200 Hzの1周期、1は2,400 Hzの2周期で、どちらも1/1,200秒を使います。

エンコーダーは48,000 sample/s、signed 16-bit mono PCM、振幅12,000を使います。
最初のburst前、burst間、最後のburst後へ40 msの無音を入れます。
同じフレームと設定からは同じWAVを生成します。

デコーダーはmono 16-bit PCM WAVを受け付け、DC平均を除去し、burstを探し、1,200 baud周辺の有限範囲でtimingを合わせます。
`J8FS`と8N1を厳密に検査し、信号欠落、同期不一致、途中終了、framing失敗、不正長を拒否します。

## コマンド

任意のローカルデータをプロジェクトFSKへ変換します。

```sh
build/native-debug/tools/jr8wav encode \
  --address 0x8000 --block-size 256 input.bin output.wav
```

プロジェクトFSKを検証し、JR8ROMへ変換します。

```sh
build/native-debug/tools/jr8wav decode input.wav output.j8r
```

全frame、CRC、SHA-256、格納範囲が正しい場合だけ、復元したアドレスを持つ1セグメントの`.j8r`を出力します。
不完全または未検証の入力は終了状態2、引数、I/O、WAVエンコードの誤りは終了状態1です。

プロジェクトFSKのテストは合成データだけを使います。
clean round trip、DC offset、gain、noise、clipping、3%の速度差、同期異常、burst欠落、block破損、frame欠落、範囲不足を検査します。
これらはホスト側codecのテストであり、JR-800実機の電気信号を検証するものではありません。
