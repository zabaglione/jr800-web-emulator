# CIの対象選択

CIはNative Debug / Release、WASM Debug / Releaseを維持し、各構成で未検証の部分だけをビルド・テストします。WASM Releaseはキャンペーン全体、WASM Debugは短い動作確認を行い、どちらも同じ入力をNativeで再生して比較します。

| 変更内容 | 検証する範囲 |
| --- | --- |
| 1作品のソース・ステージ・画像生成コード | その作品のNativeルールテスト、WASM動作、Native再生 |
| 共通SDK・ゲーム共通コード・共通ゲーム検証コード | 全作品。SDK変更時はNativeの共通テストも実行 |
| CPU・デバイス・アセンブラー・WASM API・CI設定 | 関連する本体の再ビルドと全作品の検証 |
| Webの見た目・画面処理 | WASM構成のWeb共通テスト。ゲームと本体は再利用 |
| WASMアダプター・共通起動設定 | 全作品のWASM動作とNative再生も実行 |
| Wiki原稿・スクリーンショット・README | ビルドとゲームテストを省略 |

共通ファイルの変更は保守的に対象を広げます。例えばRELIC DIVEの元サンプルもSDKの入力に含め、別バージョンへの影響を見落とさないようにしています。

DOT CLAIMの表示設定は`games/dot-claim/hud.py`、専用の操作・点滅検査は`games/dot-claim/check.mjs`で管理します。ゲームのソースだけでなく、これらの修正もDOT CLAIMだけを検証対象にします。共有の描画生成器や検証用ランタイム自体を変更した場合は全作品を検証します。

## 検証済み結果の再利用

`tools/ci.py`がソースの内容からSHA-256を計算し、直前のコミットではなく、実際に成功した検証結果と照合します。途中のCIが失敗・中止された場合や、コード変更後にドキュメントだけを追加した場合も、未検証のコードを検出できます。

再利用する実行ファイル・ゲーム成果物もダイジェストを確認します。欠損・破損したゲームはその作品を再検証し、本体の成果物を再利用できない場合は全体を検証します。検証が失敗した実行は成功記録を保存しません。Pull Requestはキャッシュを読むだけで、成功記録のキャッシュを保存しません。

Actionsを許可リストで制限しているリポジトリでは、`.github/workflows/ci-check.yml`に固定した`actions/cache/restore`と`actions/cache/save`のコミットを許可してください。

初回・キャッシュ失効時・CI設定変更時は全体の検証が必要です。通常の差分実行は、各構成の成功記録ができた次の実行から有効になります。Actionsの各ジョブのSummaryに、本体ビルド・共通テストの有無と対象ゲーム名を表示します。

## PagesとWiki

PagesはCIのWASM Releaseで検証した同じ成果物を公開します。独立した二度目のビルド・テストは実行しません。NativeとWASMの全構成が成功してから、公開中のページと比較し、内容が違う場合だけデプロイします。別構成の失敗後に再実行した場合も、検証済みの成果物から公開を再開できます。

ゲームを1本だけビルドするときも、変更のない作品の検証済み成果物と合わせて全作品の台帳を生成します。ゲーム削除時は配信対象から除外します。配信は既存のCMakeのファイル一覧に限定し、ROMや状態ファイルを含めません。

Wiki原稿の同期は既存の`tools/sync_game_wiki.py`を使います。この変更でWikiへの自動pushは追加していません。Wiki原稿だけの更新では、成功記録を再利用できればゲームのビルド・テストは発生しません。

## 手動での確認

Actionsの「CI」→「Run workflow」で`full`を有効にすると、成功記録を無視して全体を検証します。

ローカルの通常のMake操作は引き続き使えます。Nativeのルールテストは作品別の名前になりました。

```sh
make -C games/box-shift test
ctest --preset native-release -R '^game_rules_box-shift$'
ctest --preset wasm-release -R '^(game_box-shift|native_replay_box-shift)$'
python3 tests/ci_selection_test.py
```

CIと同じ対象選択・実行をローカルで確認する場合は、次の順序で実行します。キャッシュと実行計画は`build/`配下に置かれます。

```sh
python3 tools/ci.py plan --preset native-release
python3 tools/ci.py run --preset native-release
```

これらは静的確認とエミュレーター検証です。所有ROMを使うブラウザー操作確認や実機検証の代替にはなりません。
