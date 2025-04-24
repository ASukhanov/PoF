#!/usr/bin/env python3
"""Liteserver for aggregation of several PoF chains"""
__version__ = '0.0.1 2025-03-05'# a

import sys, time, threading
timer = time.perf_counter
import numpy as np

from liteserver import liteserver
LDO = liteserver.LDO
from liteaccess import Access as LA
AppName = 'multipim'
sourcePVTuples = (('localhost;9700:dev1','current'),('localhost;9701:dev1','current'))

#````````````````````````````Lite Data Objects````````````````````````````````
class Dev(liteserver.Device):
    
    def __init__(self):
        self.pim2_vt = {'value': [0.], 'timestamp': 0.}
        self.forcePublish = False# for use in setters to trigger publishing
        pars = {
'version':  LDO('RI',f'{AppName} version', 'version '+__version__),
'sumCurrent':  LDO('R','Sum of currents from PoF', 0., units='A'),
# General monitors
#'cycle':    LDO('RI','Cycle number, updates every 10 s',0),
}
        super().__init__('dev1', pars)
        self.start()
    #``````````````Overridables```````````````````````````````````````````````        
    def start(self):
        for pvt in sourcePVTuples:
            print(f'subscribe {pvt}')
            LA.subscribe(self.callback, pvt)

    def stop(self):
        print(f'>{AppName}.stop()')
        LA.unsubscribe()
#,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    def callback(self, *args):
        #print(f'cb: {args[0]}')
        for devPv, vt in args[0].items():
            v,t = vt.values()
            if devPv == sourcePVTuples[0]:
                dt = t - self.pim2_vt['timestamp']
                #print(f'pim1: {v[0],t}, dt: {dt}, tPim2: {self.pim2_vt}')
                if dt < 10.:
                    sumCurrent = v[0] + self.pim2_vt['value'][0]
                    #print(f'update sum: {sumCurrent,t}')
                    self.PV['sumCurrent'].value = sumCurrent
                    self.PV['sumCurrent'].timestamp = t
                    shippedBytes = self.publish()                    
            elif   devPv == sourcePVTuples[1]:
                self.pim2_vt = vt

if __name__ == "__main__":
    # parse arguments
    import argparse
    parser = argparse.ArgumentParser(description=__doc__
        ,formatter_class=argparse.ArgumentDefaultsHelpFormatter
        ,epilog=f'{AppName} {__version__}, liteserver {liteserver.__version__}')
    defaultIP = liteserver.ip_address('')
    parser.add_argument('-b', '--baudrate', type=int, default=115200, help=\
'Baud rate of the tty')
    parser.add_argument('-i','--interface', default = defaultIP, help=\
'Network interface of the server.')
    parser.add_argument('-p','--port', type=int, default=9702, help=\
'Serving port.') 
    pargs = parser.parse_args()

    devices = [Dev()]
    server = liteserver.Server(devices, interface=pargs.interface,
        port=pargs.port)

    server.loop()
