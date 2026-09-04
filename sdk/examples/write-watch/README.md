# 書き込み監視サンプル

このサンプルはROMを使わず、ローカルのMakeベースSDKを一通り確認します。
プロジェクト内で作成したプログラムをアセンブル、リンクし、合成64 KiB RAMへ読み込み、`result + 1`への書き込み後に停止します。

build treeにあるツールを使う場合は、パスを指定します。

```sh
make \
  JR8AS=/path/to/jr8as \
  JR8LD=/path/to/jr8ld \
  JR8RUN=/path/to/jr8run
make test \
  JR8AS=/path/to/jr8as \
  JR8LD=/path/to/jr8ld \
  JR8RUN=/path/to/jr8run
```

各Make targetの役割は次のとおりです。

| target | 動作 |
| --- | --- |
| `make run` | 命令数上限付きで実行 |
| `make run-to` | 2回目のstore直前で停止 |
| `make run-to-source` | JR8DBGの`main.s:9`を使って同じ命令へ移動 |
| `make run-to-symbol` | JR8DBGの`loop`を完全一致で解決して停止 |
| `make break-if` | Aと指定メモリーバイトが条件式に一致した場合だけ`$020A`で停止 |
| `make step-over` | callではない1命令をstep over |
| `make step-out` | returnへ到達しない場合の命令数上限を確認 |
| `make trace` | `$0000`へのwriteだけを表示 |
| `make debug` | JR8DBGのソース検索とwrite watchpointを使用 |
| `make test` | `loop`、PC、SP、X、A、B、CCR、cycle数、結果2バイトを`--expect`で検査 |
| `make clean` | このサンプルの生成物だけを削除 |
