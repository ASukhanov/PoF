# Procman configuration for SIM board"""
__version__ = 'v0.0.1 2025-12-20'
import os
rootDir = os.environ['HOME']
pypath = rootDir +'/venv/bin/python -m'

# abbreviations:
help,cmd,proc,cd,shell = ['help','cmd','process','cd','shell']
def  _screen(name, cmd): return f'screen -h 1000 -dmS {name} {cmd}'

#``````````````````Properties, used by procman`````````````````````````````````
title = 'Testing of the SIM board'

startup = {
'SIM server':{help:'LiteServer of the SIM board',
  cd:	rootDir + '/github/PoF/pof',
  cmd:_screen('SIM', f'{pypath} sim -ilocalhost -p9700'),
  proc: 'sim -ilocalhost -p9700',
  shell: True,
  },
'SIM page':{help:'Control page for SIM board', 
  cmd:f'{pypath} pypeto -f control/PoF_SIM',
  proc:'pypeto -f control/PoF_SIM',
  },
'htop':{help:'Process viewer in separate xterm',
  cmd:'xterm htop',
  },
}

