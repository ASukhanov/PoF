#!/usr/bin/env python3
"""Support of the PoF SIM board"""
__version__ = '0.3.1 2024-08-23'# STS vRef added, get_data_lock, serdev timeout reduced to 0.1

import sys, time, threading
timer = time.perf_counter
import numpy as np
import serial

from liteserver import liteserver
LDO = liteserver.LDO
AppName = 'sim'
SerDev = None
#Event = threading.Event()
get_data_lock = threading.Lock()#Important! To avoid missing writes
DevInstance = None

SRate = [5.88,11.76,23.52,47.01,93.93,187.45,373.28,740.18]#,1499.49,2816.35'
SRateLV = [str(i) for  i in SRate]

#````````````````````````````Helper functions`````````````````````````````````
def printTime(): return time.strftime("%m%d:%H%M%S")
def croppedText(txt, limit=200):
    if len(txt) > limit:
        txt = txt[:limit]+'...'
    return txt
def prints(prefix, msg):
    try:
        DevInstance.PV['status'].value = f'{prefix}_{msg}'
        DevInstance.PV['status'].timestamp = time.time()
        DevInstance.publish()
    except Exception as e: 
        print(f'Exception in prints: {e}')
    print(f'{prefix}_{AppName}@{printTime()}: {msg}')
        
def printi(msg): prints('', msg)
def printw(msg): prints('WAR', msg)
def printe(msg): prints('ERR', msg)
def _printv(msg, level=0):
    if pargs.dbg is None:
        return
    if pargs.dbg > level:
        print(f'dbg{level}@{printTime()}: '+msg)
def printv(msg):   _printv(msg, 0)
def printvv(msg):  _printv(msg, 1)

def b2i(buf):
    return int.from_bytes(buf, 'little')

def open_serdev():
    timeout = 0.1
    try:
        r = serial.Serial(pargs.tty, pargs.baudrate, timeout=timeout)#writeTimeout=5)
    except serial.SerialException as e:
        printe(f'Could not open {pargs.tty}: {e}')
        sys.exit(1)
    print(f'wt: {r.writeTimeout}')
    return r

def get_data():
    """Read data from the serial interface"""
    printvv('>get_data')
    #payload = SerDev.readline()
    with get_data_lock:
        payload = SerDev.read_until(b'>')
    return payload

def decode_sts(txt):
    # Translate response of the STS? command to map
    s = txt.replace(' ','')
    tokens = s.split(',')
    r = {}
    for token in tokens:
        key,val = token.split(':')
        r[key] = val
    return r

def write_uart(cmd:str):
    for i in cmd:
        SerDev.write(i.encode())
        time.sleep(0.01)
#````````````````````````````Lite Data Objects````````````````````````````````
class Dev(liteserver.Device):
    
    samples = []# Sample storage between reports
    timeOfFirstSample = 0.
    timeOfLastSample = 0.
    
    def __init__(self):
        self.initialized = False
        self.dummy = 0
        vref = float(pargs.ref)
        pars = {
'version':  LDO('RI',f'{AppName} version', __version__),
'send':     LDO('RWEI','Send command to device','STS?',setter=self.set_send),
'vRef':     LDO('RC','Reference voltage', vref, units='V'),
'msg':      LDO('R','Message from {AppName}',['']),
'adcScale': LDO('RC','Scale to convert ADC readings to volts', vref/2**23,units='V'),
'nsamples': LDO('R','Number of samples, accumulated since last report', 0),
'nstats':   LDO('R','Samples in on-board statistics calculation', 0),
'mean':     LDO('R','On-board-calculated mean', 0., units='V'),
'rms':      LDO('R','On-board-calculated rms', 0., units='V'),
'p2p':      LDO('R','On-board peak-to-peak amplitude', 0., units='V'),
'samples':  LDO('R','ADC samples', [0.], units='V'),
'xaxis':    LDO('R','Time axis array for samples (approximate)', [0.], units='s'),
'srate':    LDO('RWE','Sampling rate of the ADC', SRateLV[0], units='Hz',
                legalValues=SRateLV, setter=self.set_srate),
'recLimit': LDO('RWE','Limit of samples accumulated between reports',0,
                setter=self.set_recLimit),
'timeout':  LDO('RWE','Timeout for receiving one character from PIM, it defines data rate',
                0, units='ms', setter=self.set_timeout),
'cycle':    LDO('RI','Cycle number, updates periodically',0),
'rps':      LDO('RI','Cycles per second',[0.],units='Hz'),
        }
        super().__init__('dev1', pars)
        if not pargs.stop:
            self.start()

        self.initialized = True
        printi('Initialization finished')
        
    #``````````````Overridables```````````````````````````````````````````````        
    def start(self):
        thread = threading.Thread(target=self.seriaListener, daemon = True)
        thread.start()
        #if not Event.wait(.2):
        #    printe('Listener did not start')
        #    sys.exit(1)
        self.execute_command('STS?')

    def stop(self):
        printi(f'>{AppName}.stop()')
    #,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    def div10(self, pvname):
        pv = self.PV[pvname]
        value = pv.value[0]
        pv.value[0] = value/10.

    def execute_command(self, cmd:str, sts=False):
        with get_data_lock:
            write_uart(f'<{cmd}>')
        if not sts:
            time.sleep(0.1)
            with get_data_lock:
                write_uart('<STS?>')

    def set_send(self):
        cmd = self.PV['send'].value[0]
        sts = False
        if cmd == 'STS?':
            # clear status and msg
            sts = True
            self.PV['msg'].set_valueAndTimestamp('')
            self.PV['status'].set_valueAndTimestamp('Sent STS?')            
        self.execute_command(cmd, sts)
        return 0

    def set_srate(self):
        value = self.PV['srate'].value[0]
        idx = SRateLV.index(value)
        self.execute_command(f'S {idx}')
        return 0

    def set_timeout(self):
        value = self.PV['timeout'].value[0]
        self.execute_command(f'TO {value}')
        return 0

    def set_recLimit(self):
        value = self.PV['recLimit'].value[0]
        self.execute_command(f'R {value}')
        return 0

    def handle_devPacket(self, record):
        # return True if something was collected for publishing
        #print(f'payload: {payload}')
        #for record in payload[:-1].split(b'>'):
        #print(f'record: {record}')
        ts = self.timestamp
        try:
            txt = record.decode()
        except Exception as e:
            printw(f'Exception in record.decode: {e}')
            return False
        if len(txt) <= 1:
            return False
        if txt[0] == '\n':
            txt = txt[1:]
        if txt[0] != '<':
            # This should not happen with recent firmware
            printw(f'msg: {txt}')
            self.PV['msg'].set_valueAndTimestamp(txt, ts)
            return True
        printv(f'>handle {txt}')
        ag = self.PV['adcScale'].value[0]
        if txt[1] == 'M':
            # Regular report
            txtnums = txt[2:-1].split(',')
            for i,ng in enumerate([('nsamples',0),('nstats',0),('mean',.1),
                    ('rms',.1),('p2p',1.)]):
                name,gain = ng
                try:
                    v = int(txtnums[i]) if gain==0 else float(txtnums[i])*gain*ag
                except Exception as e:
                    printw(f'Statistics record corrupted: {e}')
                    return False
                self.PV[name].set_valueAndTimestamp([v],ts)
            #print(f'samples: {Dev.samples}')
            l = len(Dev.samples)
            self.PV['samples'].set_valueAndTimestamp(Dev.samples,ts)
            try:
                tstep = (Dev.timeOfLastSample - Dev.timeOfFirstSample)/l
                xaxis = np.arange(l)*tstep
                self.PV['xaxis'].set_valueAndTimestamp(xaxis,ts)
            except: pass
            Dev.samples = []
            Dev.timeOfFirstSample = 0.
            return True

        elif txt[1] == 'R':
            try:
                values = (np.fromstring(txt[2:-1],sep=',')*ag).round(9)
            except:
                return
            t = time.time()
            Dev.timeOfLastSample = t
            if Dev.timeOfFirstSample == 0:
                Dev.timeOfFirstSample = t
            Dev.samples += list(values)
            return False

        elif txt[1] == 'T':
            s = txt[2:-1]
            printv(f'msg: {txt}')
            if txt[2:4] == 'SR': 
                # Response to STS
                m = decode_sts(s)
                printv(f'amap: {m}')
                i = [int(m['SR'])][0]
                self.PV['srate'].set_valueAndTimestamp([str(SRate[i])],ts)
                self.PV['recLimit'].set_valueAndTimestamp([int(m['RL'])],ts)
                self.PV['timeout'].set_valueAndTimestamp([int(m['TO'])],ts)
            else:
                #self.PV['msg'].set_valueAndTimestamp(s, ts)
                self.PV['status'].set_valueAndTimestamp(s, ts)
            return True
        #elif txt[1] =='?':
        #    #Error message
        #    self.PV['status'].set_valueAndTimestamp(txt[1:], ts)
        else:
            self.PV['msg'].set_valueAndTimestamp(txt[2:-1], ts)
            printe(f'Unexpected message')#: {txt[1:-1]}')
        return True

    def seriaListener(self):
        print('``````````Listener Started``````````````````````````````')
        #time.sleep(.2)# give time for server to startup

        prevCycle = 0
        self.timestamp = time.time()
        periodic_update = self.timestamp
        pv_cycle = self.PV['cycle']
        pv_run = self.PV['run']
        while not Dev.EventExit.is_set():
            try:
                if pv_run.value[0][:4] == 'Stop':
                    break
            except: pass

            # Periodic update
            dt = self.timestamp - periodic_update
            if dt > 10.:
                ts = time.time()# something funny with the binding, cannot use self.timestamp directly
                periodic_update = ts
                msg = f'periodic update {self.name} @{round(self.timestamp,3)}'
                self.PV['rps'].set_valueAndTimestamp\
                  ((pv_cycle.value[0] - prevCycle)/dt, ts)
                self.PV['cycle'].timestamp = ts
                prevCycle = pv_cycle.value[0]

            # Wait/Receive data from device
            self.timestamp = time.time()
            try:
                payload = get_data()
            except KeyboardInterrupt:
                print(' Interrupted')
                SerDev.close()
                sys.exit(1)
            except serial.SerialException as e:
                printe(f'ERR: serialException: {e}')
                SerDev.close()
                sys.exit(1)

            if not self.initialized:
                printvv('Initialization not finished')
                # __init__() did not finish, no sense to proceed further
                continue
            pv_cycle.value[0] += 1

            if not self.handle_devPacket(payload):
                continue

            #print('publish all modified parameters of '+self.name)
            # invalidate timestamps for changing variables, otherwise the
            # publish() will ignore them
            #for i in ['cycle']:
            #    self.PV[i].timestamp = self.timestamp
            ts = timer()
            shippedBytes = self.publish()
            printv(f'shipped: {shippedBytes}')

        print('########## listener exit ##########')
#,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
if __name__ == "__main__":
    # parse arguments
    import argparse
    parser = argparse.ArgumentParser(description=__doc__
        ,formatter_class=argparse.ArgumentDefaultsHelpFormatter
        ,epilog=f'{AppName} {__version__}, liteserver {liteserver.__version__}')
    defaultIP = liteserver.ip_address('')
    parser.add_argument('-b', '--baudrate', type=int, default=57600, help=\
'Baud rate of the tty')
    parser.add_argument('-d', '--dbg', type=int, default=0, help=\
f'Debugging level: bits[0:1] for {AppName}, bits[2:3] for liteServer')
    parser.add_argument('-i','--interface', default = defaultIP, help=\
'Network interface of the server.')
    parser.add_argument('-p','--port', type=int, default=9700, help=\
'Serving port.') 
    parser.add_argument('-r','--ref', default='3.3', help=\
'Reference voltage.')
    parser.add_argument('-S','--stop',  action='store_true', help=\
'Do not start')
    parser.add_argument('tty', nargs='?', default='/dev/ttyUSB0', help=\
'Serial device for communication with hardware')
    pargs = parser.parse_args()

    SerDev = open_serdev()

    liteserver.Server.dbg = pargs.dbg >> 2
    DevInstance = Dev()
    devices = [DevInstance]

    server = liteserver.Server(devices, interface=pargs.interface,
        port=pargs.port)

    print('`'*79)

    server.loop()
