# Procman configuration for SIM board"""
__version__ = 'v0.0.1 2025-12-20'
import os
rootDir = os.environ['HOME']
py = rootDir +'/venv/bin/python -m'
pofDir = rootDir +'/github/PoF/'

# abbreviations:
help,cmd,proc,cd,shell = ['help','cmd','process','cd','shell']
def  _screen(name, cmd): return f'screen -h 1000 -dmS {name} {cmd}'

#``````````````````Properties, used by procman`````````````````````````````````
title = 'Testing of the SIM board'

startup = {
'SIM server':{help:'LiteServer of the SIM board',
  cd:	pofDir + 'pof',
  cmd:_screen('sim', f'{py} sim -ilocalhost -p9700'),
  proc: 'sim -ilocalhost -p9700',
  shell: True,
  },
'SIM page':{help:'Control page for SIM board',
  cd:	pofDir,
  cmd:f'{py} pypeto -f control/PoF_SIM',
  proc:'pypeto -f control/PoF_SIM',
  },
'Miniterm':{help:'Connect to serial port on Nucleo STM32L432KC',
  cmd:  f'lxterminal -e {py} serial.tools.miniterm /dev/ttyUSB0 57600',
  proc: 'serial.tools.miniterm /dev/ttyUSB0 57600',
  #shell: True,
  },
'htop':{help:'Process viewer in separate xterm',
  cmd:'xterm htop',
  },
}
