# JR-800アセンブリー構文バージョン1

## 対象範囲

`jr8as`は本プロジェクト独自のアセンブラーです。
バージョン1は、ラベル、式、セクション、シンボル公開範囲、基本データディレクティブ、確認済みISAメタデータにある命令形式を扱います。
ASxxxx、A09、メーカー製アセンブラーとの互換モードではありません。

1個のソースから1個のJROを生成します。
複数ソースは個別にアセンブルし、生成したJROを`jr8ld`へ渡します。
include、macro、条件付きアセンブルはありません。

## 字句規則

- ソースはUTF-8、識別子はASCIIです。
- `;`から行末まではコメントです。
- 命令ニーモニックとディレクティブは大文字・小文字を区別しません。
- シンボル名は大文字・小文字を区別し、`[A-Za-z_.][A-Za-z0-9_.]*`に一致させます。
- ラベルはシンボルの後ろに`:`を書き、同じ行へ命令も書けます。
- 10進数は数字、16進数は`$`、2進数は`%`を先頭に付けます。
- 即値オペランドは式の前に`#`を付けます。

```asm
.equ value, 42
.byte %10101010, $2A, 42
LDAA #$2A
```

絶対シンボルは`.equ`だけで定義します。
代入形式は受け付けません。

## 式

演算子の優先順位は次のとおりです。

1. 単項`+`、`-`、`~`
2. `*`、`/`
3. `+`、`-`
4. `<<`、`>>`
5. `&`
6. `^`
7. `|`

絶対式は検査付き符号付き64ビット演算です。
0除算、不正なシフト、オーバーフローはエラーになります。
式は解析深さ128、構文ノード256までに制限し、超過時は`E2002`で失敗します。

再配置可能な式は、1個のシンボルと任意の定数加数だけで構成します。
`target`、`target + 2`、`target - 1`が例です。
2個の再配置シンボルを使う演算や、未解決シンボルへのビット演算は推測せず拒否します。

`.equ`と`.space`は、その行より前に定義済みの絶対シンボルだけを使えます。
命令とデータのオペランドでは、後方にあるラベルを参照できます。

## セクション

バイトを生成する前にセクションを選びます。

```asm
.section .text, code
.section .data, data
.section .bss, bss
```

| クラス | JRO表現 |
| --- | --- |
| `code` | `PROGRAM_BITS`、allocate＋execute |
| `data` | `PROGRAM_BITS`、allocate＋write |
| `bss` | `NO_BITS`、allocate＋write |

同名セクションへ戻ると末尾へ追加します。
同名を別クラスで開き直すとエラーです。
初期アラインメントは1、配置はrelocatableで、最終配置はリンカーが行います。

データディレクティブは次のとおりです。

- `.byte expression [, expression ...]`：各式を1バイトにし、絶対値は-128～255、シンボルは`ABS8`
- `.word expression [, expression ...]`：各式をビッグエンディアン2バイトにし、シンボルは`ABS16_BE`
- `.space absolute-expression`：`code`と`data`ではゼロバイトを格納し、`bss`では論理ゼロ領域を作成

バイトを生成するディレクティブと命令ごとに、JROへソース行範囲を記録します。

## シンボル

ラベルは指定がなければlocalです。

```asm
.local loop
.global entry
.extern helper
```

`.local`と`.global`は同じJRO内で定義するシンボルの結合属性を宣言します。
`.extern`は別JROで定義されるglobal undefinedシンボルを作ります。
参照するシンボルは、同じファイルで定義するか`.extern`で宣言してください。
暗黙の外部シンボルは作りません。

`.equ name, absolute-expression`はabsoluteシンボルを定義します。
結合属性の宣言は定義の前後どちらにも置けますが、矛盾した宣言と重複定義はエラーです。

## 対応する命令形式

アセンブラーは独自のオペコード表を持たず、指定CPUプロファイルのISAメタデータから形式を選びます。
現在の入力形式は次のとおりです。

```asm
NOP
INX
DEX
TAB
ABX
ASLB
ADDA #expression
CPX #expression
CMPA #expression
CMPB #expression
SBCA #expression
SEI
DECA
DECB
INCA
CLRA
LDAA #expression
LDAB #expression
LDAA expression
LDAA displacement-expression,X
LDD #expression
LDD expression
LDD displacement-expression,X
LDS #expression
LDS expression
LDS displacement-expression,X
LDX #expression
LDX expression
LDX displacement-expression,X
STAA expression
STAA displacement-expression,X
STAB expression
STAB displacement-expression,X
STX expression
STX displacement-expression,X
STS expression
STS displacement-expression,X
BRA symbol-plus-addend
BCC symbol-plus-addend
BNE symbol-plus-addend
BPL symbol-plus-addend
BSR symbol-plus-addend
JMP expression
JMP displacement-expression,X
JSR expression
PULA
PULB
PSHA
PSHB
PSHX
PULX
RTS
AIM #mask-expression, address-expression
OIM #immediate-expression, address-expression
OIM #immediate-expression, displacement-expression,X
TIM #immediate-expression, displacement-expression,X
CLR displacement-expression,X
```

`AIM`、`OIM`、`TIM`は、選択したCPUプロファイルのメタデータが対応する場合だけ使えます。
`TIM`は現在indexed形式だけが確認済みです。

directとextendedの両方がある命令では、値が確定していて0～255ならdirect、それより大きければextendedを選びます。
sectionまたはexternalシンボルは最終アドレスが未確定なのでextendedになります。
`JSR`も、page zeroの確定値はdirect、大きい値または未解決シンボルはextended、`displacement,X`はindexedです。

現在の`LDX expression`はHD6301V1のdirect形式だけです。
確定値はpage zero内、シンボルはリンカーが範囲検査する`DIRECT8`再配置になります。

indexed形式は末尾に`X`を書き、displacementは0～255です。
実効アドレスはXとdisplacementの和です。
`AIM`、`EIM`、`OIM`、`TIM`は即値バイト、displacementの順、`LDAA`、`STAA`、`JSR`はdisplacementだけを出力します。
ほかのインデックスレジスターや範囲外displacementはエラーです。

オペランドの役割と再配置形式は次の対応です。

- 即値バイト、indexed displacement、`.byte`：`ABS8`
- 即値word、extended address、`.word`：`ABS16_BE`
- direct address：`DIRECT8`
- branch target：`REL8`

相対分岐に数値だけを書くことはできません。
再配置セクションの最終アドレスが未確定なため、ラベルまたはexternalシンボルと任意の定数加数を使います。

## コマンドとエラー

```sh
jr8as --target hd6301v1 -o main.jro --listing main.lst main.s
```

`--target`、`-o`、入力ファイル1個が必須です。
`--listing`は省略できます。
エラーは`path:line:column`と安定したコードで表示し、失敗時はJROを出力しません。

listingはCPUプロファイル、ソース識別、セクション内アドレス、再配置前バイト、元のソース行を表示します。
デバッガーが使う構造化ソース対応はJRO本体に格納します。
