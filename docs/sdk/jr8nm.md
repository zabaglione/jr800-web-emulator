# jr8nmでシンボルを表示する

`jr8nm`は、JROバージョン1またはJR8DBGバージョン1を完全に検証してから、シンボル表を表示します。
入力ファイルを書き換えません。

```sh
jr8nm input.jro
jr8nm linked.j8d
```

JROでは、先頭行に形式とCPUプロファイルを表示し、続けて元のシンボルレコード順にタブ区切りで出力します。

```text
JRO 1.0 target=hd6301v1
INDEX  BINDING  DEFINITION  VALUE      SIZE  SECTION  NAME
0      global   section     $00000000  0     ".text"  "entry"
1      global   undefined   -          0     -        "helper"
```

`VALUE`は、section定義ではセクション内オフセット、absolute定義では元の値です。
リンク後のアドレスではありません。
undefinedシンボルは値とセクションを`-`で示し、仮の値を作りません。

JR8DBGはリンク済み16ビット値を持つため、別の列で表示します。

```text
JR8DBG 1.0 target=hd6301v1
INDEX  BINDING  KIND     VALUE  SIZE  SOURCE    NAME
0      global   address  $0200  0     "main.s"  "entry"
```

`KIND`はaddressとabsoluteを区別します。
`SOURCE`はソース番号を持つ場合の正確なパス、それ以外は`-`です。
JR8APPとのSHA-256照合はローダーの役割なので、`jr8nm`単体では行いません。

文字列中の引用符、バックスラッシュ、制御バイト、非ASCIIバイトはエスケープします。
異常な文字列が端末制御や偽の出力行を作ることを防ぎ、各バイトは大文字の`\xNN`で保持します。

コマンドは入力ファイルを1個だけ受け付け、正確なマジックで形式を決めます。
検証失敗時に途中まで表を出したり、別形式として再解釈したりしません。
レコードの並べ替え、名前修飾の解除、再配置の適用も行いません。
