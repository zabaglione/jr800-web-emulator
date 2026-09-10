# SPDX-License-Identifier: MIT
"""Generate Japanese game manuals and source-controlled Wiki pages."""
import argparse,json,shutil
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--live-check',type=Path);a=p.parse_args()
site='https://zabaglione.github.io/jr800-web-emulator/'
repo='https://github.com/zabaglione/jr800-web-emulator'
raw='https://raw.githubusercontent.com/zabaglione/jr800-web-emulator/main/docs/games/screenshots'
ids=[p['id'] for p in json.loads((root/'games/catalog.json').read_text())['programs']]
verified=set()
if a.live_check:
 check=json.loads(a.live_check.read_text())
 if check.get('passed') is not True or check.get('siteUrl')!=site:raise ValueError('A successful public-site check is required')
 verified=set(check['programs'])
data={
'box-shift':('箱を押してすべての丸い目標に置く、全20面の倉庫パズルです。','箱は1個ずつ押せます。引くことはできません。壁際で動かせなくなったら1手戻すか、面をやり直してください。手数は最大9999まで表示します。','方向キーで移動。RETURNのメニューから **UNDO ONE MOVE**（1手戻す）、**HELP**（ヒント表示）、**RETRY**（面の再挑戦）を選べます。SPACEはメニューの決定に使います。',['最初の倉庫と箱・目標','箱を押す順序を考える場面','後半の倉庫配置']),
'mirror-link':('鏡を回転させて、すべての受光器へレーザーを導く全20面のパズルです。','白い輪が未点灯の受光器、塗られた輪が点灯済みです。後半は2〜3個の光源が登場します。壁と光源で光は止まります。同じ位置・方向に戻る光路も有限の処理で停止します。','方向キーでカーソルを移動し、SPACEで鏡を90度回転します。RETURNの **UNDO ROTATION** で直前の回転を戻し、**RESET MIRRORS** でその面の鏡を初期配置へ戻せます。',['1本の光路と鏡','2回の反射を使う面','複数の光源を接続する後半の面']),
'step-strike':('移動・射撃・待機の1手でだけ世界が進む、全20面の戦術パズルです。','全員の敵を倒すとクリア。隣の敵に移動すると近接攻撃で倒せます。射撃は最後に移動した向きへ最大4マス届きます。敵は3手ごとに、壁で遮られていない縦・横の直線上へ弾を撃ちます。敵弾に触れると失敗です。','方向キーで移動・向き変更、SPACEで射撃。RETURNの **WAIT ONE TURN** で1手待機し、**HELP** で案内を切り替えます。移動できない壁へ押した場合は手が進みません。',['敵の配置を見て作戦を立てる','遮蔽物を使って接近する','敵と弾道を見ながら次の1手を選ぶ']),
'circuit-deck':('手札3枚とエナジー3を使う、9戦制のカードバトルです。最終戦にはボスが待っています。','カードのCOSTが残りエナジー以下なら使用できます。使ったカードは捨て札へ移り、ターン終了時に残りの手札も捨てて3枚引き直します。山札が尽きると捨て札をシャッフルします。敵のNEXT表示は次の行動です。8回の勝利報酬では3枚から1枚を選んでデッキへ追加し、HPを12回復します（上限40）。難易度1〜3を選べます。','左右または上下で手札を選び、SPACEで使用します。RETURNの **END TURN** でターンを終え、**TOGGLE HELP** で案内を切り替えます。報酬画面でも方向キーとSPACEでカードを選びます。',['手札・エナジー・敵の予告','勝利後の3択報酬','最終戦のボス']),
'pocket-factory':('資源A・Bをベルトと加工機で運び、製品2種類を出荷する全12課題の工場パズルです。','左側の資源口から原料が出ます。PRESS Aは資源Aを製品Aへ、PRESS Bは資源Bを製品Bへ加工し、右向きに排出します。右側の対応する出荷口へ届け、SHIP/GOALの上段A・下段Bを両方達成するとクリア。行き先が埋まっている資源はその場で待つため、逆向きのベルトや合流の詰まりに注意してください。','方向キーで配置カーソルを動かし、SPACEで現在の部品を置きます。RETURN → **SELECT TOOL** → 方向キー → SPACEで、4方向のベルト・2種類の加工機・消去を選択します。選択中のRETURNは取消です。**RUN / PAUSE** で生産を実行・停止します。配置を変えるときは停止してください。',['出荷口を確認して工場を設計','加工機を通って資源が流れる','別の地形で2系統の製品を出荷']),
'arc-duel':('風を読み、地形を削りながらCPUの砲台と戦う弾道対戦です。6種類の地形と3段階の難易度があります。','Aは角度（15〜75度）、Pは威力（2〜9）、Wは風です。正の風は右向き、負の風は左向きです。爆風は近い砲台に最大40のダメージを与え、自分にも当たります。HPを0にするとラウンド勝利。2勝先取で対戦クリアです。次のラウンドでは地形が変わります。CPU AIM中もメニューを開けます。','上下で角度、左右で威力を調整し、SPACEで発射します。ラウンド終了後もSPACEで続行。RETURNの **NEXT TERRAIN** は次の地形で対戦を最初からやり直し、**TOGGLE HELP** は案内を切り替えます。',['風・角度・威力を決める','放物線を描く砲弾','爆発で変形した地形での対戦'])}
wiki=root/'docs/games/wiki';wiki.mkdir(parents=True,exist_ok=True)
common='''## 共通操作

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
'''
home='# JR-800 ゲーム\n\nSDKサンプルとは別に開発した、6本のオリジナル機械語ゲームです。ゲーム内表示は英語、案内は日本語です。\n\n| タイトル | 内容 |\n|---|---|\n'
for ident in ids:
 title=ident.replace('-',' ').upper();summary,rules,controls,captions=data[ident]
 home+=f'| [{title}]({repo}/wiki/{ident.upper()}) | {summary} |\n'
 source=f'{repo}/tree/main/games/{ident}'
 launch=f'[遊ぶ]({site}?program={ident}) · [ビルド可能なソース]({source})' if ident in verified else f'[ビルド可能なソース]({source})'
 page=f'# {title}\n\n{launch}\n\n![タイトル画面]({raw}/{ident}/title.png)\n\n{summary}\n\n## 遊び方\n\n{rules}\n\n{controls}\n\n'
 if ident=='circuit-deck':
  page+='| カード | COST | 効果 |\n|---|---:|---|\n'
  for name,cost,effect in [('STRIKE',1,'6ダメージ'),('GUARD',1,'防御6'),('SPARK',0,'2ダメージ'),('PIERCE',2,'12ダメージ'),('WALL',2,'防御12'),('HEAL',1,'HPを5回復'),('VENOM',1,'毒3を付与。敵ターンごとに継続ダメージ'),('CHARGE',0,'エナジーを1増やす'),('DRAIN',2,'7ダメージ、HPを4回復'),('NOVA',3,'20ダメージ'),('FOCUS',1,'この戦闘中の攻撃力を2増やす'),('ECHO',1,'4ダメージ、防御4')]:page+=f'| {name} | {cost} | {effect} |\n'
  page+='\n防御は次の敵行動まで有効です。毒と攻撃力上昇は各99が上限で、次の戦闘ではリセットされます。\n\n'
 page+='## ゲーム画面\n\n'
 for i,caption in enumerate(captions,1):page+=f'![{caption}]({raw}/{ident}/gameplay-{i}.png)\n\n{caption}。\n\n'
 page+=common+f'\n## ビルド\n\n```sh\nmake -C games/{ident}\nmake -C games/{ident} test\n```\n\n環境の準備・一括ビルドは[Games README]({repo}/tree/main/games)を参照してください。画像とマップを含む新規制作物はMIT Licenseです。\n'
 (wiki/(ident.upper()+'.md')).write_text(page)
 (root/'games'/ident/'README.md').write_text(page.replace(raw+'/'+ident,'../../docs/games/screenshots/'+ident))
 dest=root/'docs/games/screenshots'/ident;dest.mkdir(parents=True,exist_ok=True)
 for name in ['title','gameplay-1','gameplay-2','gameplay-3']:
  for suffix in ['', '-1x']:
   shutil.copyfile(root/'build/games'/ident/(name+suffix+'.png'),dest/(name+suffix+'.png'))
home+='''\nタイトル画面はSPACEで進みます。ROMを同じサイト・パスで保存済みなら、確認済みの「遊ぶ」リンクからタイトル画面へ直接進めます。初回は手元のBASIC ROMを選び、Start BASICを押してください。音声はSPACEや画面内操作で有効になります。通常のエミュレーターURLは手動起動のままです。

'''+common
(wiki/'Home.md').write_text(home)
(wiki/'_Sidebar.md').write_text(f'[ゲーム一覧]({repo}/wiki)\n\n'+'\n'.join(f'- [{ident.replace("-"," ").upper()}]({repo}/wiki/{ident.upper()})' for ident in ids)+'\n')
(root/'docs/games/README.md').write_text(home.replace(repo+'/wiki/', 'wiki/').replace(') |', '.md) |'))
