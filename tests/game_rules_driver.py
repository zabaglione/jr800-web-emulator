# SPDX-License-Identifier: MIT
import subprocess,sys
root,assembler,linker,runner=sys.argv[1:]
subprocess.run(['make','-s','-C',root+'/games','JR8AS='+assembler,'JR8LD='+linker],check=True)
subprocess.run([runner,root],check=True)
