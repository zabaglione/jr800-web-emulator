# jr8objdumpでJROとJR8APPを調べる

`jr8objdump`はJROバージョン1またはJR8APPバージョン2を検証し、構造と格納済みバイトを表示します。

```sh
jr8objdump input.jro
jr8objdump linked.j8a
jr8objdump --debug linked.j8d linked.j8a
```

JROの実行可能`PROGRAM_BITS`は、オブジェクトに記録されたCPUプロファイルと共通のC++逆アセンブル機能で表示します。
実行不可の`PROGRAM_BITS`は16進データ、`NO_BITS`は論理サイズだけを表示します。

再配置対象のオペランドは未解決の仮値です。
該当行は`+relocation`と表示し、正しい再配置種別、シンボル、加数を末尾の表に出します。
部分リンクした値のようには扱いません。
未知オペコードとセクション末尾で切れた命令は、それぞれ`unknown-opcode`、`truncated-instruction`の1バイト`.byte $NN`として表示します。

JROは全体を検証し、CPUプロファイルに確認済み命令メタデータがある場合だけ表示します。
`jr800_unresolved`をHD6301V1として代用しません。
セクション名とシンボル名は端末制御を防ぐ形でエスケープします。

JR8APPでは、CPUプロファイル、エントリーポイント、検証済みSHA-256、各DATA／ZERO_FILLセグメントを表示します。
JR8APPには実行可能属性がないため、DATAは`linear-stored-byte-decode`として線形に解釈します。
セグメント全体がコードだとは断定しません。
エントリーポイントが命令途中にある場合は`+entry-inside`と表示します。

`--debug`はJR8APPと組み合わせた場合だけ使えます。
JR8DBG全体、CPUプロファイル、JR8APP完全性値を照合してから、各行に`SYMBOLS`と`SOURCE`を加えます。
命令途中のシンボルやソース位置は`@+1`などのオフセット付きで表示します。
JR8DBGのパスは識別子として扱い、ソースファイルを開きません。

壊れた入力や不一致のデバッグ情報は、部分的な逆アセンブルを出す前に拒否します。
JR8APP内のコードとデータの境界を推測せず、所有者のROMも読みません。
