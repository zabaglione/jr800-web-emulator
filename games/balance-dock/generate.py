# SPDX-License-Identifier: MIT
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap
root=Path(__file__).parent
sprites=[Bitmap(8,8),Bitmap(8,8)];sprites[1].line(0,7,7,7)
assets(root,'BALANCE DOCK','balance-dock',16,7,1,1,sprites,3,aux=('FLIP','RESET'))
