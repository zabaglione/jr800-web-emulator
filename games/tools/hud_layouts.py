# SPDX-License-Identifier: MIT
"""Game-owned HUD contents and art direction; expressions compile to HD6301 code."""
def b(name):return 'LDAB '+name+'\nCLRA'
def w(name):return 'LDD '+name
def c(n):return 'LDD #'+str(n)
def inc(name):return b(name)+'\nADDD #1'
def sub(n,name):return 'LDAB #'+str(n)+'\nSUBB '+name+'\nCLRA'
def lut(name,index,word=False):
    return 'LDAB '+index+('\nASLB' if word else '')+'\nLDX #'+name+'\nABX\n'+('LDD 0,X' if word else 'LDAB 0,X\nCLRA')
def n(label,code,digits=3):return dict(label=label,code=code,digits=digits)
def t(label,code,values):return dict(label=label,code=code,values=values)
def flag(name):return 'CLRB\nTST '+name+'\nBEQ @done\nINCB\n@done:\nCLRA'
def status(code,values):return t('',code,values)
def setup(view,theme,brand,fields,state=None,meter=None):return dict(view=view,theme=theme,brand=brand,fields=fields,status=state or status(c(0),['SPACE']),meter=meter)

SPECS={
'box-shift':setup(64,'ticket','CRATES',[n('MOVES',w('moves'),4),n('ROOM',inc('stage'))],status(flag('show_help'),['PUSH / RETURN','UNDO IN MENU'])),
'mirror-link':setup(32,'optics','OPTIC',[n('LIT',b('lit_count')),n('GOALS',b('goal_count'))],status(c(0),['ROTATE'])),
'step-strike':setup(64,'terminal','TACTIC',[n('HOSTILES',b('guard_count')),n('TURNS',b('turns'))],status(flag('step_help'),['SPACE: FIRE','MENU: WAIT'])),
'lamp-grid':setup(0,'switchboard','LIGHTS',[n('ON',b('grid_stat')),n('FLIPS',b('moves'))],status(c(0),['SPACE: FLIP'])),
'slide-nine':setup(64,'tile','SLIDER',[n('LEFT',b('grid_stat')),n('STEPS',b('moves'))],status(c(0),['MOVE THE GAP'])),
'ice-route':setup(32,'ice','ICE',[n('GEMS',b('grid_stat')),n('SLIDES',b('moves'))],status(c(0),['SLIDE'])),
'switch-maze':setup(0,'circuit','SWITCH',[n('KEYS',b('grid_stat')),n('STEPS',b('moves')),n('GATES',b('gates'))],status(c(0),['FIND THE EXIT'])),
'line-four':setup(32,'scoreboard','DROP',[n('MOVES',b('moves')),n('FREE',b('grid_stat'))],status(b('four_result'),['DROP','YOU WIN','CPU WIN','DRAW'])),
'reversi-mini':setup(64,'folio','DISKS',[n('YOU',b('grid_stat')),n('CPU',b('rev_cpu_disks')),n('MOVES',b('moves'))],status(b('rev_passed'),['PLACE A DISK','YOU PASS','CPU PASSES'])),
'five-stones':setup(32,'ink','GOMOKU',[n('MOVES',b('moves')),n('FREE',b('grid_stat'))],status(b('five_result'),['PLACE','YOU WIN','CPU WIN','DRAW'])),
'hex-front':setup(0,'terminal','RELAY',[n('NEED',b('grid_stat')),n('RELAY',b('hex_charge')),n('TURNS',b('moves'))],status(flag('selection_active'),['PLACE A NODE','CONVERT NODE'])),
'knight-tour':setup(64,'folio','KNIGHT',[n('LEFT',b('grid_stat')),n('HOPS',b('moves'))],status(c(0),['KNIGHT MOVE'])),
'peg-rescue':setup(32,'pegboard','PEGS',[n('LEFT',b('grid_stat')),n('JUMPS',b('moves'))],status(flag('selection_active'),['SELECT','JUMP'])),
'pawn-race':setup(64,'scoreboard','PAWNS',[n('FOES',b('grid_stat')),n('TURNS',b('moves'))],status(flag('selection_active'),['SELECT A PAWN','PICK A SQUARE'])),
'dot-claim':setup(32,'graph','DOTS',[n('LEFT',b('grid_stat')),n('MOVES',b('moves')),n('YOU',b('dot_you')),n('CPU',b('dot_cpu'))],status(c(0),['CONNECT'])),
'pipe-weave':setup(0,'plumbing','FLOW',[n('DRY',b('grid_stat')),n('TURNS',b('moves'))],status(c(0),['SPACE: TURN'])),
'number-rail':setup(64,'ticker','MERGE',[n('TOP',lut('rail_values','grid_stat',True),4),n('GOAL',lut('rail_goals','stage')+'\nASLB\nLDX #rail_values\nABX\nLDD 0,X',4),n('MOVES',w('rail_moves'),4),n('SCORE',w('rail_score'),5)],status(c(0),['SLIDE TO MERGE'])),
'mine-field':setup(0,'hazard','MINES',[n('SAFE',b('grid_stat')),n('FLAGS',b('mine_flags')),n('MINES',b('mine_count')),n('MOVES',b('moves'))],status(flag('selection_active'),['OPEN CELL','FLAG CELL'])),
'loop-trace':setup(32,'trace','LOOP',[n('LEFT',b('grid_stat')),n('LINKS',b('moves')),t('NEXT',b('loop_next'),['-','A','B','C','-'])],status(flag('grid_stat'),['CLOSE','DRAW'])),
'ace-stack':setup(64,'card','ACE 13',[n('LEFT',b('grid_stat')),t('WASTE','LDAB #28\nJSR ace_available\nTAB\nCLRA',['-','A','2','3','4','5','6','7','8','9','10','J','Q','K']),n('STOCK',sub(24,'ace_stock_pos')),n('MOVES',b('moves'))],status('CLRB\nLDAA cursor\nCMPA #28\nBNE @board\nINCB\n@board:\nTST selection_active\nBEQ @done\nLDAB #2\n@done:\nCLRA',['SUM TO 13','> WASTE','CHOOSE A PAIR'])),
'suit-run':setup(0,'card','SUITS',[n('SCORE',w('suit_score'),4),t('WASTE',b('suit_waste'),['-','A','2','3','4','5','6','7','8','9','10','J','Q','K']),n('LEFT',b('grid_stat')),n('STOCK',sub(16,'suit_stock_pos')),n('CHAIN',b('suit_chain'))],status(c(0),['ONE UP OR DOWN'])),
'wall-break':setup(0,'arcade','BREAK',[n('SCORE',w('wall_score'),4),n('BRICKS',b('wall_left')),n('LIVES',b('wall_lives'))],status(flag('wall_active'),['SPACE: LAUNCH','KEEP IT IN'])),
'tail-trail':setup(64,'pixel','SNAKE',[n('FOOD',b('tail_eaten')),n('GOAL',lut('tail_goals','stage')),n('LENGTH',b('tail_length'))],status(flag('tail_running'),['SPACE: START','KEEP MOVING'])),
'maze-chase':setup(32,'arcade','CHASE',[n('FOOD',b('grid_stat')),n('LIVES',b('maze_lives')),n('POWER',b('maze_power')),n('SCORE',w('maze_score'),4)],status(flag('maze_running'),['START','CHASE'])),
'river-hop':setup(0,'water','CROSS',[n('TIME',b('river_time')),n('HOME',b('grid_stat')),n('LIVES',b('river_lives')),n('SCORE',w('river_score'),4)],status(flag('river_running'),['SPACE: START','CROSS SAFELY']),meter=80),
'tower-leap':setup(64,'tower','CLIMB',[n('FLOOR',b('tower_highest')),n('LIVES',b('tower_lives')),n('SAVE',b('tower_checkpoint'))],status(c(0),['SPACE: JUMP'])),
'bomb-vault':setup(0,'hazard','VAULT',[n('KEYS',b('grid_stat')),n('LIVES',b('bomb_lives')),n('FUSE',b('bomb_fuse')),n('SCORE',w('bomb_score'),4)],status(c(0),['SPACE: BOMB'])),
'grid-claim':setup(32,'graph','CLAIM',[n('YOU',b('claim_wins')),n('CPU',b('claim_losses')),n('MAP',inc('claim_arena'))],status(b('claim_mode'),['DRAW','MOVING','NEXT'])),
'gravity-run':setup(64,'vector','FLIP',[n('DIST',w('gravity_distance'),4),n('LIVES',b('gravity_lives')),n('STARS',b('gravity_stars')),n('SCORE',w('gravity_score'),4)],status(flag('gravity_direction'),['GRAVITY DOWN','GRAVITY UP'])),
'star-patrol':setup(32,'radar','PATROL',[n('FOES',b('star_left')),n('LIVES',b('star_lives')),n('SCORE',w('star_score'),4)],status(flag('star_running'),['START','FIRE'])),
'orbit-guard':setup(0,'radar','ORBIT',[n('CORE',b('orbit_core')),n('FOES',b('orbit_left')),n('PULSE',b('orbit_pulse')),n('SCORE',w('orbit_score'),4)],status(flag('orbit_running'),['SPACE: START','SPACE: FIRE']),meter=10),
'target-range':setup(64,'target','RANGE',[n('SCORE',w('range_score'),4),n('ROUND',inc('range_round')),n('TIME',b('range_time_display')),n('MISS',b('range_misses'))],status(b('range_mode'),['SPACE: START','AIM / FIRE','NEXT TARGET'])),
'ricochet-ops':setup(32,'vector','BANK',[n('FOES',b('rico_left')),n('AMMO',b('rico_ammo')),n('ANGLE',b('rico_aim')),n('SCORE',w('rico_score'),4)],status(flag('rico_active'),['FIRE','FLIGHT'])),
'relay-quest':setup(64,'stone','RELIC',[n('HP',b('quest_hp')),n('KEYS',b('quest_keys')),n('TONIC',b('quest_pot')),n('RELAY',b('quest_left')),n('SCORE',w('quest_score'),4)],status(c(0),['SPACE: ACT']),meter=20),
'echo-cavern':setup(0,'sonar','SONAR',[n('AIR',b('echo_air')),n('GEMS',b('echo_left')),n('MAPPED',b('echo_mapped')),n('SCORE',w('echo_score'),4)],status(c(0),['SPACE: PING']),meter=100),
'micro-rogue':setup(64,'folio','DEPTHS',[n('HP',b('rogue_hp')),n('BLADE',b('rogue_sword')),n('ARMOR',b('rogue_armor')),n('TONIC',b('rogue_pot')),n('FOES',b('rogue_left')),n('FLOOR',inc('rogue_floor')),n('SCORE',w('rogue_score'),4)],status(c(0),['SPACE: MELEE']),meter=20),
'signal-ghost':setup(32,'terminal','STEALTH',[n('LINKS',b('grid_stat')),n('STEPS',b('moves')),n('SCORE',w('signal_score'),4)],status(c(0),['HACK'])),
'rail-dispatch':setup(64,'timetable','SIGNAL',[n('DONE',b('dispatch_done')),n('LEFT',b('dispatch_total')+'\nSUBB dispatch_done'),t('EXIT',b('dispatch_switch'),['A','B']),n('TIME',b('dispatch_time')),t('NEXT','LDAA dispatch_next_train\nCMPA dispatch_total\nBCS @next\nLDD #8\nBRA @done\n@next:\nLDAB #3\nMUL\nADDD dispatch_schedule\nXGDX\nLDAB 1,X\nASLB\nADDB 2,X\nCLRA\n@done:',['A>A','A>B','B>A','B>B','C>A','C>B','D>A','D>B','END'])],status(flag('dispatch_running'),['SET SWITCHES','RUNNING'])),
'orchard-days':setup(0,'garden','ORCHARD',[n('COINS',w('orchard_coins'),4),n('ACT',b('orchard_actions')),n('DAY',inc('orchard_day')),n('LIMIT',b('orchard_limit')),n('GOAL',w('orchard_goal'),4),t('SEED',b('orchard_seed'),['BEAN','BERRY']),t('SKY',flag('orchard_rain'),['SUN','RAIN'])],status(c(0),['SPACE: TEND'])),
'wind-putt':setup(32,'golf','GOLF',[n('POWER',b('putt_power')),n('ANGLE',lut('putt_angles','putt_aim',True)),n('SHOT',b('putt_shots')),t('WIND','CLRB\nTST putt_wind\nBEQ @done\nLDAB #1\nBPL @done\nLDAB #2\n@done:\nCLRA',['CALM','RIGHT','LEFT'])],status(flag('putt_left'),['SWING','FLIGHT'])),
'rally-return':setup(32,'scoreboard','RALLY',[n('YOU',b('rally_you')),n('CPU',b('rally_them')),t('SPIN','LDAB rally_spin\nINCB\nCLRA',['-1','0','+1'])],status(flag('rally_active'),['SERVE','RALLY'])),
'penalty-arc':setup(0,'stadium','KICKS',[n('GOALS',b('penalty_goals')),n('POWER',b('penalty_power')),n('SHOT','LDAB penalty_kicks\nCMPB #10\nBCC @done\nINCB\n@done:\nCLRA'),t('HEIGHT',b('penalty_height'),['LOW','HIGH'])],status(b('penalty_mode'),['SPACE: READY','SPACE: KICK','IN FLIGHT','NEXT KICK']),meter=8),
'beat-step':setup(64,'equalizer','BEAT',[n('SCORE',w('beat_score'),4),n('COMBO',b('beat_combo')),n('LIFE',b('beat_life')),n('NOTES',b('beat_index')),n('TOTAL',b('beat_total')),t('SOUND',flag('beat_mute'),['ON','OFF'])],status(b('beat_judgement'),['SPACE: START','PERFECT','GOOD','MISS'])),
'balance-dock':setup(32,'cargo','DOCK',[n('HEIGHT',b('dock_count')),n('WIDTH',b('dock_width')),n('FLIPS',b('dock_flips')),n('GOAL',b('dock_goal')),n('SCORE',w('dock_score'),4)],status(b('dock_judgement'),['READY','MOVING','DROP','PERFECT','TRIM','MISS'])),
}
# Keep display units and limits identical to the game rules.
SPECS['gravity-run']['fields'][0]=n('DIST',b('gravity_distance'))
SPECS['orchard-days']['fields'][4]=n('GOAL',b('orchard_goal'))
SPECS['dot-claim']['fields'][2]=n('YOU',b('dot_player_score'))
SPECS['dot-claim']['fields'][3]=n('CPU',b('dot_cpu_score'))
SPECS['switch-maze']['fields']=SPECS['switch-maze']['fields'][:2]
SPECS['pipe-weave']['fields'].append(n('LEAKS',b('pipe_leaks')))
SPECS['river-hop']['meter']=100
SPECS['orbit-guard']['meter']=5
SPECS['relay-quest']['meter']=9
SPECS['micro-rogue']['meter']='rogue_max_hp'
SPECS['echo-cavern']['meter']=99
SPECS['target-range']['fields'][1]=n('ROUND','LDAB range_round\nCMPB #20\nBCS @next\nLDAB #19\n@next:\nINCB\nCLRA')
SPECS['target-range']['status']=status('LDAB range_result\nTST range_mode\nBNE @done\nLDAB #5\n@done:\nCLRA',['SPACE: FIRE','HIT','SAFE','MISS','WRONG TARGET','SPACE: START'])
SPECS['wind-putt']['fields'][3]=t('WIND','CLRB\nTST putt_wind\nBEQ @done\nBMI @west\nLDAB #1\nBRA @done\n@west:\nLDAB #2\n@done:\nCLRA',['CALM','RIGHT','LEFT'])
SPECS['penalty-arc']['fields'][2]=n('SHOT','LDAB penalty_kicks\nLDAA penalty_mode\nCMPA #3\nBEQ @done\nINCB\n@done:\nCLRA')
SPECS['penalty-arc']['fields'].append(n('NEED',b('stage')+'\nADDD #6'))
SPECS['penalty-arc']['status']=status('LDAB penalty_mode\nCMPB #3\nBNE @done\nLDAB penalty_reason\nADDB #2\n@done:\nCLRA',['SPACE: READY','SPACE: KICK','IN FLIGHT','GOAL','SAVE','WIDE'])
SPECS['penalty-arc']['meter']=None
SPECS['pocket-factory']=setup(64,'circuit','FACTRY',[n('SHIP A',b('shipped_a')),n('GOAL A',b('target_a')),n('SHIP B',b('shipped_b')),n('GOAL B',b('target_b')),t('TOOL',b('tool'),['RIGHT','DOWN','LEFT','UP','PRESSA','PRESSB','ERASE'])],status('CLRB\nTST running\nBEQ @build\nLDAB #1\nBRA @done\n@build:\nTST selection_active\nBEQ @done\nLDAB #2\n@done:\nCLRA',['BUILD','RUNNING','CHOOSE TOOL']))
SPECS['dice-hold']=setup(0,'card','DICE',[n('TOTAL',w('dice_total'),4),n('GOAL',lut('dice_goals','stage',True)),n('LEFT',sub(13,'dice_round')),n('BONUS',b('dice_bonus'))],status(flag('selection_active'),['SPACE: HOLD','PICK A SCORE']))
SPECS['push-luck']=setup(0,'card','LUCK',[n('YOU',b('luck_scores')),n('CPU',b('luck_scores + 1')),n('TARGET',c(100))],status(flag('luck_side'),['ROLL OR BANK','CPU TURN']))
SPECS['ricochet-ops']['fields'][2]=t('AIM',b('rico_aim'),['UP','UP-R','RIGHT','DN-R','DOWN','DN-L','LEFT','UP-L'])
SPECS['grid-claim']['status']=status(b('claim_mode'),['START','RACING','YOU WIN','LOST','DRAW'])
SPECS['rail-dispatch']['fields'][4]['values']=['U>A','U>B','D>A','D>B','-','-','-','-','END']
SPECS['rail-dispatch']['status']=status('LDAB dispatch_reason\nBEQ @normal\nINCB\nBRA @done\n@normal:\nLDAB dispatch_running\n@done:\nCLRA',['SET SWITCHES','RUNNING','CRASH','ROUTE','TIME'])
