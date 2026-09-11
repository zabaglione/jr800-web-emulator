# JR-800 ゲームライブラリー

定番とモダンな遊びを組み合わせた**50本**のゲームを収録しています。SDKサンプルとは別の独立したゲームで、ゲーム内表示は英語です。

## ジャンルから探す

| ジャンル | 作品数 | 内容 |
|---|---:|---|
| [パズル・論理](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Logic) | 10 | 盤面を読み、手順や配置を考えるゲーム。 |
| [盤上戦略](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Board) | 8 | 定番の盤上遊戯と、一手ずつ考える対戦。 |
| [カード・ダイス](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Cards) | 5 | 手札、確率、リスクと報酬を使うゲーム。 |
| [アクション](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Action) | 8 | 移動、回避、ジャンプと空間の判断。 |
| [シューティング](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Shooting) | 6 | 射線、照準、弾道を使うゲーム。 |
| [探索・冒険](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Adventure) | 4 | 未知の場所を調べ、資源を管理して進むゲーム。 |
| [経営・シミュレーション](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Simulation) | 4 | 配置や運用を工夫して目標を達成するゲーム。 |
| [スポーツ・タイミング](https://github.com/zabaglione/jr800-web-emulator/wiki/Genre-Sports) | 5 | 角度、反応、間合いとタイミングを競うゲーム。 |

パズルを中心とした15作品は各40面、合計600面のチャレンジを収録。規定手数・追加目標・3段階評価と、面や全評価を復元するパスワードに対応しています。対象作品と詳しい説明は[パズルチャレンジ](https://github.com/zabaglione/jr800-web-emulator/wiki/Puzzle-Challenges)を参照してください。

箱・建物・駒などは陰影や斜めの辺で奥行きを表現し、数字も作品の雰囲気に合わせています。50作品それぞれの採用判断とメモリー方針は[奥行きと数字の意匠](https://github.com/zabaglione/jr800-web-emulator/blob/main/docs/games/depth-design.md)にまとめています。

各作品のページに概要・操作・タイトル画面とゲーム中3場面を掲載します。**公開環境で確認済みの作品だけ「遊ぶ」リンクを付けます。** 同じサイト・パスでROMを保存済みなら、リンクからタイトル画面へ直接進めます。初回は手元のBASIC ROMを選び、Start BASICを押してください。

## 共通操作

| 操作 | キー |
|---|---|
| 上・下・左・右 | テンキー8・2・4・6、またはW・S・A・D |
| 決定・開始・主要アクション | SPACE |
| 取消・戻る・操作メニュー | RETURN |
| BASICへ終了 | BREAK |

メニューを開いている間はゲームが停止します。決定・取消は押した瞬間だけ反応し、移動だけ長押しできます。

## 確認済み環境

JR-HuBASIC 1.0を使ったWebエミュレーターで確認しています。ゲームは標準16KB RAM内に収まり、拡張RAMなしのNative/WASM再生でも検証しています。ブラウザーのBASIC起動には既存のBASIC実験プロファイルを使用します。2.0は対応未確認です。実機動作・カセット転送・実機のLCD応答と音は未検証です。

BREAKで戻る際には、以前のBASICプログラムと変数が消去されます。必要な内容はゲームを読み込む前に保存してください。途中経過はエミュレーターの汎用状態保存で保存できます。ROMとROM入り状態ファイルは配布しません。
