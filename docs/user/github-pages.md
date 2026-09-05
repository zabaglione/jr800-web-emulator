# GitHub Pagesで利用する

このリポジトリをビルドし、GitHub Pagesでエミュレーターを配信できます。

## 配信するリポジトリの設定

GitHub Actionsが無効になっている場合は、リポジトリのSettings → Actionsで有効にしてから進めます。

1. 配信するリポジトリのSettings → Pagesを開き、Build and deploymentのSourceを「GitHub Actions」にします。
2. `main`へ反映するか、Actionsの「GitHub Pages」からRun workflowを実行します。
3. 完了後、Pagesに表示されるURLを開きます。

手順は[GitHub公式のカスタムワークフロー説明](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages)に基づきます。
[配信ワークフロー](../../.github/workflows/pages.yml)は非公開リポジトリでは実行しません。
ブランチ名が`main`以外の場合はワークフローの対象ブランチも変更してください。

EmscriptenでReleaseビルドし、自動テストが成功した場合だけ`build/wasm-release/web/`を配信します。
相対URLでWorkerとWASMを読み込むため、`https://<owner>.github.io/<repository>/`形式のURLに対応します。
リポジトリ全体、ROM、WAV、非公開検証資料は配信対象にしません。

## ROMを一度設定した後

初回は手元の`.j8r`または32 KiBの`.rom`を選び、「BASICの起動」を押します。
読み込みに成功すると、そのROMと起動設定をブラウザー内に保存します。
再訪時は「保存済みROM」にファイル名を表示し、起動ボタンを押せる状態に戻します。
エミュレーターへの読み込みと起動は、ボタンを押した時点で行います。

ファイル選択欄にも保存済みROMのファイル名を復元します。
新しいファイルを選ぶと、次回の起動にはそのファイルを使います。
読み込めなかったファイルで以前の保存を置き換えることはありません。
「ブラウザー内のROMを削除」は保存コピーだけを消し、元ファイルと起動中の状態は維持します。

保存内容は同じブラウザーとサイトURLでのみ利用できます。
ローカルの検証URLから公開URLへの引き継ぎは行わないため、公開URLでは初回にROMを選択してください。
サイトデータの消去などでROMが消えた場合も再選択できます。
RAMとプログラムは次回へ引き継ぎません。必要なプログラムは先にJ8AまたはWAVへ保存してください。

## 配信前に確認できる範囲

`tests/browser_rom_storage_test.cjs`は、市販ROMを使わずに実ブラウザーで配下URLへの配信、ブラウザーの終了・再起動、明示的な起動、設定復元、無効ファイルの拒否、保存コピー削除、保存不可時の手動起動を確認します。
デプロイ完了後は、実際の配信URLで初回のROM選択と再訪時の起動を確認してください。
