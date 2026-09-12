# プレイ動画

全51作品を[動画ギャラリー](https://zabaglione.github.io/jr800-web-emulator/videos/)と各作品のWikiから視聴できます。スクリーンショットは従来どおり掲載しています。

約30秒を基本に、建設やステージ移行などを見せる作品は延長しています。通常のCPUクロックで動かしたWASMエミュレーターのLCDとスピーカーを記録しています。操作は自動入力ですが、人が操作する程度の間隔でキーを押し、移動や演出の途中を省略していません。実機で撮影した映像ではありません。

動画はH.264 / AAC、800 × 288、30 fps、48 kHzモノラルです。ゲームの192 × 64画面を整数倍で拡大しています。実機ROMは使わず、プロジェクト内で作成した起動用コードとビルド済みゲームを使用します。動画にはゲーム画面とゲーム音声だけを含めます。

## 更新する

Node.js、Python 3、ffmpeg / ffprobeと、通常のゲーム・WASM Releaseビルドが必要です。

```sh
node tools/record_gameplay_video.mjs lamp-grid build/game-videos-review 30
# 生成したMP4を視聴し、操作・演出・音声を確認してから取り込みます。
python3 tools/publish_game_videos.py --game lamp-grid
python3 tools/ci.py plan --preset wasm-release
python3 tools/ci.py run --preset wasm-release
```

全作品を取り込む場合は、全作品を収録・確認したあと `--game` を省略します。収録時間は第3引数で指定できます。入力手順は `tools/video_playthroughs.mjs` に集約しています。公開ギャラリーのHTMLを変更した場合も、取り込みコマンドで配信リストのハッシュを更新してください。

`<ゲームID>.mp4` が動画の原本です。`catalog.json` に動画と収録時のゲームプログラムのSHA-256、長さを記録します。配信時は同じ動画をハッシュ付きURLに配置するため、更新後に古い動画がキャッシュから表示されません。タイトル画像は既存の `docs/games/titles/` を参照します。

`web/site-media.json` が配信対象の明示リストです。共通の配信ツールがパスと内容のハッシュを検証し、動画の変更を既存のCIの配信判定に伝えます。動画を更新するために、変更のないゲームを再ビルド・再テストする必要はありません。
