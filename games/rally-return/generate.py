# SPDX-License-Identifier: MIT
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from grid_assets import assets,Bitmap
root=Path(__file__).parent
sprites=[Bitmap(8,8) for _ in range(4)]
sprites[1].line(0,0,7,0);sprites[2].line(0,7,7,7)
sprites[3].line(0,0,0,1);sprites[3].line(0,4,0,5)
assets(root,'RALLY RETURN','rally-return',16,7,1,1,sprites,3,aux=('NEUTRAL','RESET'))
