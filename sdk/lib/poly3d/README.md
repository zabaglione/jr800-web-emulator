# 小さな凸多面体の描画ライブラリ

HD6301V1の機械語で三角錐・八面体・立方体を描く、MITライセンスの独立したソースです。用途は、固定した視点で少数の立体を操作するJR-800ゲームです。[15の実証サンプル](../../examples/lcd/15-poly3d-lab/README.md)と、[16のゲート飛行](../../examples/lcd/16-gate-flight/README.md)、[POLY DEFENDER](../../../games/poly-defender/README.md)、[18の回転合わせ](../../examples/lcd/18-turn-match/README.md)が同じAPIを使います。

座標変換・陰影・走査線の塗りつぶしは実行時に計算します。角度ごとの画像を格納する方式ではありません。14 POLYGON FIGHTERの専用コードからも独立しています。

## 実現範囲

| 項目 | 契約 |
| --- | --- |
| メッシュ | 3〜8頂点、1〜12三角形。面が外側を向いた凸形状 |
| モデル座標 | 符号付き8ビットのXYZ、各成分−24〜24 |
| 回転 | yawとpitch、各64段階。下位6ビットを使用 |
| 投影 | 平行投影。画面中心と拡大率を物体ごとに指定 |
| 陰影 | 回転した法線と固定光源、4段階の2×2ドット密度 |
| 表示 | 不透明な面、または前面だけの線表示。裏面を除去 |
| 画面端 | 左右と指定した上下範囲で切り詰める |
| 作業RAM | 描画器387バイト。既存の1,536バイトの描画面を共有 |
| 合成順 | 呼び出し側が遠い物体から描く。物体同士が重ならない配置も利用可能 |

透視投影、任意軸のroll、Zバッファー、交差する物体や凹形状の正確な隠面処理、3D衝突判定は含みません。16と17の奥行きは拡大率と画面座標で表し、ゲームの判定はレーンや画面上の範囲で行います。小さいドット数と整数演算のため、回転には量子化による輪郭の揺れがあります。

## 組み込み

`render.s`と、必要なら`models.s`をアセンブルしてリンクします。`lcd/display.s`の`framebuffer`、`lcd/dirty.s`の変更範囲、呼び出し側が実装する`p3_poll`が必要です。フォントやBASIC復帰処理は描画器自体の依存ではありません。

```asm
.extern p3_init
.extern p3_begin
.extern p3_draw
.extern p3_x
.extern p3_y
.extern p3_yaw
.extern p3_pitch
.extern p3_scale
.extern p3_model_tetra
.extern input_poll
.global p3_poll

    JSR p3_init          ; 起動時。既存の画面は消さない
    JSR p3_begin         ; フレームの先頭で1回
    LDAA #96
    STAA p3_x
    LDAA #32
    STAA p3_y
    LDAA #64             ; 64/128 = 0.5倍
    STAA p3_scale
    LDAA #8              ; 8/64周 = 45度
    STAA p3_yaw
    LDAA #4
    STAA p3_pitch
    LDX #p3_model_tetra
    JSR p3_draw
    ; ほかの物体も設定を変えてp3_draw。最後にdirty_begin/dirty_next。

p3_poll:
    JMP input_poll
```

これは描画部分の抜粋です。起動・LCD初期化・終了まで含む例は各サンプルの`main.s`と[`poly3d-runtime.s`](../../examples/lcd/common/poly3d-runtime.s)を参照してください。独自のゲームは`app_init`、`app_update`、`app_draw`を実装し、[`poly3d.mk`](../../examples/lcd/common/poly3d.mk)を使えます。

## API

すべての処理はA、B、X、CCRを破壊します。再入不可です。割込みから並行して呼ばないでください。描画中のコールバックも`p3_*`の処理・状態や描画面を変更しないでください。

| API／変数 | 入出力・役割 |
| --- | --- |
| `p3_init` | 占有範囲と設定を初期化。x=96、y=32、scale=96、yaw=0、pitch=4、mode=0、描画範囲0〜63 |
| `p3_begin` | 前フレームの立体が使ったバイト範囲を消去し、変更範囲へ追加。エラーをクリア |
| `p3_draw` | X=メッシュ記述子。現在の設定で1個描く。Aと`p3_status`が0なら成功、1なら不正な引数 |
| `p3_line` | X=4バイトの`x0,y0,x1,y1`。黒い線をOR描画。端点は画面内に置く。上下範囲で切り詰める |
| `p3_x`, `p3_y` | 画面の原点。x=0〜191、y=0〜63。変換後の頂点は画面外でもよい |
| `p3_scale` | 1〜128。128を1倍とする整数倍率 |
| `p3_yaw`, `p3_pitch` | 各0〜63。モデルのY軸回転の後、画面のX軸回転 |
| `p3_mode` | 0=陰影付きの面、1=前面の線表示。線表示でも裏面は描かない |
| `p3_clip_top`, `p3_clip_bottom` | 上端は8の倍数、下端は8で割った余りが7。0≤上端≤下端≤63 |
| `p3_error` | フレーム中のどれかの呼び出しが不正なら1を保持。`p3_begin`でクリア |
| `p3_visible_count` | 最後の`p3_draw`で前面と判定した三角形数。完全に画面外でも数える |
| `p3_projected` | 最後の変換結果。1頂点につき符号付きX16ビット・Y8ビット、最大24バイト。16ビット値は上位バイトが先 |
| `p3_poll` | 呼び出し側で用意するサブルーチン。頂点・面・走査線・消去帯の処理間で呼ばれる。A/B/Xを破壊してよい |

上下範囲を8ドット単位に限定するのは、前フレームの消去がLCDの1バイト、つまり縦8ドット単位だからです。サンプルは上端8・下端55とし、上下の文字欄を保護します。立体と同じバイトを共有する背景・文字は、毎フレーム`p3_begin`の後に描き直してください。

描画器は前フレームの占有範囲だけを記録し、`dirty_min`、`dirty_max`、`dirty_pending`を更新します。LCD転送は呼び出し側の責任です。`p3_init`の再呼び出しは前の占有範囲を破棄するため、画面切替時は描画面の消去と全画面転送指定も行います。

## メッシュの形式

```asm
mesh:
    .byte 4,4           ; 頂点数、三角形数
    .word vertices,faces
vertices:
    .byte x,y,z         ; 頂点数だけ繰り返す。負数は8ビットの2の補数
faces:
    .byte i,j,k,nx,ny,nz,255
                        ; 頂点番号3個、外向き法線3成分、陰影指定
```

面法線は長さ約96に正規化し、各成分を符号付き8ビットで持ちます。正のZが手前で、変換後の法線Zが正なら前面です。陰影指定255は光源から計算、0・2・4・6は固定濃度です。0が黒、6が白に近く、輪郭は黒で描きます。

実行時は頂点数・面数・座標範囲・頂点番号・陰影指定・描画設定を検査します。ポインターの有効性、凸性、面法線の整合性と長さは呼び出し側の責任です。不正な引数の描画は行わずエラーを返します。サンプル共通処理はエラーを検出すると`POLY3D ERROR`を表示します。

`generate_models.py`に独自モデルを追加して生成します。このスクリプトは凸性・座標範囲・面の退化を検査し、外向き法線を作ります。形状と法線のデータだけを出力し、実行時の回転や陰影を省略しません。

```sh
python3 sdk/lib/poly3d/generate_models.py
python3 sdk/lib/poly3d/generate_models.py --check
```

## メモリー配置と検証

3本のサンプルとPOLY DEFENDERは標準RAM内の[`poly3d-memory.j8l`](../../examples/lcd/common/poly3d-memory.j8l)を使います。

| 範囲 | 用途 |
| --- | --- |
| `$2800–$4BFF` | プログラムと固定データ。最大9,216バイトの領域 |
| `$4C00–$51FF` | フレームバッファー1,536バイト |
| `$5400–$57FF` | 既存のBASIC復帰コード用領域 |
| `$5800–$58FF` | BASICの状態退避用領域 |
| `$5900–$5DFF` | 描画器・入力・ゲーム状態用領域 |
| `$5E00–$5FFF` | スタック用の予約領域。初期SP=`$5FFF` |

上記は予約した範囲です。実際の使用量は各サンプルの`.map`と`verification.json`に記録します。コードと固定データもRAMを消費します。共通LCD処理は既存のCPU内RAM `$80–$91`を使います。

```sh
cmake --preset native-release
cmake --build --preset native-release --target poly3d_library_test
ctest --preset native-release -R '^poly3d_library_test$'
make -C sdk/examples/lcd/15-poly3d-lab test
make -C sdk/examples/lcd/16-gate-flight test
make -C games/poly-defender test
make -C sdk/examples/lcd/18-turn-match test
```

Native検査は5,328条件の立体描画を、独立したQ7演算と走査線交点の式に照合します。前後の不透明合成、不正引数、画面外、メモリー保護、斜線、864条件の水平・垂直線と上下文字欄の保護も検査します。WASM検査は実際のJR-800キー入力を使い、全LCD画素と描画RAMの一致、スタック、コード・周辺RAMの保護、ゲームの進行を確認します。

公称1,228,800 Eサイクル/秒による速度換算はエミュレーター上の結果です。実機のLCD残像・BUSY時間・カセット転送・操作感は未検証です。既存のLCD・入力・BASIC復帰モデルを使い、新しいハードウェア動作の仮定は追加していません。同じキー選択行の同時押しは実機で未確認のため、操作手順と自動プレイは同時押しを必要としません。

構成別の速度・配置サイズ・実施した検査は[検証記録](../../../docs/sdk/poly3d-validation.md)を参照してください。
