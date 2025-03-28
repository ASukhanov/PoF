#!/usr/bin/env python3
"""Support of the PoF PIM board"""
__version__ = '0.4.6 2025-03-27'# Exit if serial interface is disconnected
#TODO: handle <?ERR?...> and <T...> messages from SIM

import sys, time, threading
timer = time.perf_counter
import numpy as np
import serial

from liteserver import liteserver
LDO = liteserver.LDO
AppName = 'pim'
SerDev = None
#Event = threading.Event()
get_data_lock = threading.Lock()#Important! To avoid missing writes
DevInstance = None

Rsense = 20.# Ohms
SRate = [5.88,11.76,23.52,47.01,93.93,187.45,373.28,740.18]#,1499.49,2816.35'
SRateLV = [str(i) for  i in SRate]
laserLV = ['','OFF','MinPower','MaxPower']
SerialReadTimout = 0.4# Timeout for serial input, If it less than 0.2 that may cause cropped input.

#````````````````````````````Helper functions`````````````````````````````````
def printTime(): return time.strftime("%m%d:%H%M%S")
def croppedText(txt, limit=200):
    if len(txt) > limit:
        txt = txt[:limit]+'...'
    return txt
def prints(prefix, msg):
    try:
        DevInstance.PV['status'].value = f'{prefix}{msg}'
        DevInstance.PV['status'].timestamp = time.time()
    except Exception as e: 
        print(f'Exception in prints: {e}')
    print(f'{prefix}{AppName}@{printTime()}: {msg}')
        
def printi(msg): prints('', msg)
def printw(msg): prints('WAR:', msg)
def printe(msg): prints('ERR:', msg)
def _printv(msg, level=0):
    if pargs.verbose > level:
        print(f'dbg{level}@{printTime()}: '+msg)
def printv(msg):   _printv(msg, 0)
def printvv(msg):  _printv(msg, 1)

def b2i(buf):
    return int.from_bytes(buf, 'little')

def open_serdev():
    try:
        r = serial.Serial(pargs.tty, pargs.baudrate, timeout=SerialReadTimout)#writeTimeout=5)
    except serial.SerialException as e:
        printe(f'Could not open {pargs.tty}: {e}')
        sys.exit(1)
    print(f'wt: {r.writeTimeout}')
    return r

def get_data():
    """Read data from the serial interface"""
    #printvv('>get_data')
    #payload = SerDev.readline()
    with get_data_lock:
        payload = SerDev.read_until(b'>')
    return payload

def decode_status(txt):
    # Translate response of SIM and PIM boards to STS? command
    s = txt.replace(' ','')
    tokens = s.split(',')
    r = {}
    try:
        for token in tokens:
            key,val = token.split(':')
            r[key] = val
    except:
        printw(f'Status format error: {txt}')
    #printv(f'decoded status: {r}')
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
        self.forcePublish = False# for use in setters to trigger publishing
        vref = float(pargs.ref)
        pars = {
'version':  LDO('RI',f'{AppName} version', 'version '+__version__),
'send':     LDO('RWEI','Send command to device','STS?',setter=self.set_send),
'vRef':     LDO('RC','Reference voltage', vref, units='V'),
'msg':      LDO('R','Message from {AppName}',['']),
'Clean!':    LDO('WEI','Clear error and warning messages', None, setter=self.set_clean),
'laser':    LDO('RWE','Laser Control',[''], legalValues=laserLV,
                setter=self.set_laser),
'adcScale': LDO('RC','Scale to convert ADC readings to volts', vref/2**24,units='V'),
'nsamples': LDO('R','Number of samples, accumulated by SIM since last report, depends on srate and timeout', 0),
'received': LDO('R','Number of samples received from SIM', 0),
'nstats':   LDO('R','Number of samples in on-board statistics calculation', 0),
#'mean':     LDO('R','On-board-calculated mean of the voltage over Rsense', 0., units='V'),
'current':  LDO('R','Average current through Rsense, averaged over 1 s.', 0., units='A'),
'rsense':   LDO('R','Sense resistor', Rsense, units='Ohm'),
'noise':    LDO('R','Standard deviation of the current', 0., units='A'),
'p2p':      LDO('R','Peak-to-peak current amplitude', 0., units='A'),
'samples':  LDO('R','Current samples, accumulated during 1 s', [0.], units='A'),
'xaxis':    LDO('R','Time axis array for samples (approximate)', [0.], units='s'),
'srate':    LDO('RWE','Sampling rate of the ADC', SRateLV[0], units='Hz',
                legalValues=SRateLV, setter=self.set_srate),
'recLimit': LDO('RWE','Limit of samples accumulated between reports (1 s)',0,
                setter=self.set_recLimit),
'timeout':  LDO('RWE','Timeout in SIM for receiving one character from PIM, it defines data rate',
                0, units='ms', setter=self.set_timeout),
# PIM-related parameters:
'pim_LTMP': LDO('R','PIM laser temperature', 0., units='C`'),
'pim_LRBK': LDO('R','PIM laser current readback', 0., units='V'),
'pim_Status': LDO('R','PIM status reply', ['']),

# General monitors
'cycle':    LDO('RI','Cycle number, updates every 10 s',0),
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
        self.execute_command('STS?', update_status=True)

    def stop(self):
        printi(f'>{AppName}.stop()')
    #,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    def div10(self, pvname):
        pv = self.PV[pvname]
        value = pv.value[0]
        pv.value[0] = value/10.

    def execute_command(self, cmd:str, update_status=True):
        printv(f'execute_command: {cmd}')
        with get_data_lock:
            write_uart(f'<{cmd}>')
        if update_status:
            time.sleep(0.1)
            with get_data_lock:
                write_uart('<STS?>')

    def set_send(self):
        cmd = self.PV['send'].value[0]
        sts = True
        if cmd == 'STS?':
            # clear status and msg
            sts = False
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

    def set_laser(self):
        value = self.PV['laser'].value[0]
        cmd = {'OFF':'LSR 0','MinPower':'LSR 1','MaxPower':'LSR 2'}.get(value)
        if cmd is None:
            return 1
        self.execute_command(f'{cmd}')
        return 0

    def set_clean(self):
        ts = time.time()
        for pvname in ('status','msg','pim_Status'):
            self.PV[pvname].set_valueAndTimestamp('',ts)
        for pvname in ('srate','recLimit','timeout','received','current','noise','p2p'):
            self.PV[pvname].set_valueAndTimestamp([0.],ts)
        #self.publish() This would lock!!! 
        self.forcePublish = True# trigger one publish() in main thread
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
        idx = txt.find('<')
        if idx == -1:
           return False
        txt = txt[idx:]
        #printvv(f'>handle {txt}')
        ag = self.PV['adcScale'].value[0]
        if len(txt) == 0:
            printw('empty message `<` from pim')
            return False
        if txt[1] == 'M':
            # SIM statistics: <M7,7,-2670,14,4,110868713>
            printv(f'>handle M: {txt}')
            txtnums = txt[2:-1].split(',')
            for i,ng in enumerate([('nsamples',0),('nstats',0),('current',.1/Rsense),
                    ('noise',.1/Rsense),('p2p',1./Rsense)]):
                name,gain = ng
                try:
                    v = int(txtnums[i]) if gain==0 else float(txtnums[i])*gain*ag
                except Exception as e:
                    printw(f'Statistics record corrupted: {e}')
                    return False
                #printv(f'set_value {v,ts}')
                self.PV[name].set_valueAndTimestamp([v],ts)
            #print(f'samples: {Dev.samples}')
            l = len(Dev.samples)
            self.PV['received'].set_valueAndTimestamp(l,ts)
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
            # SIM samples
            printvv(f'>handle R: {txt}')
            try:
                values = (np.fromstring(txt[2:-1],sep=',')*ag).round(9)
            except:
                return
            # accumulate Dev.samples
            t = time.time()
            Dev.timeOfLastSample = t
            if Dev.timeOfFirstSample == 0:
                Dev.timeOfFirstSample = t
            Dev.samples += list(values)
            return False

        elif txt[1] in 'T':
            # SIM status: <TSR:0,TO:3,RL:800,T:110869248,V:0.1.4>
            printv(f'>handle T: {txt}')
            s = txt[2:-1]
            printi(f'SIM status: {txt}')
            if txt[2:4] == 'SR': 
                # Response to STS
                m = decode_status(s)
                i = [int(m['SR'])][0]
                self.PV['srate'].set_valueAndTimestamp([str(SRate[i])],ts)
                self.PV['recLimit'].set_valueAndTimestamp([int(m['RL'])],ts)
                self.PV['timeout'].set_valueAndTimestamp([int(m['TO'])],ts)
            else:
                #self.PV['msg'].set_valueAndTimestamp(s, ts)
                self.PV['status'].set_valueAndTimestamp(s, ts)
            return True

        elif txt[1] in 'V':
            # PIM status: <V:0.1,T:112733543,ERR:0,SST:1,LSW:1,LRT:1,LTMP:40.4,LRBK:2.5>
            printv(f'>handle V: {txt}')
            s = txt[1:-1]
            printi(f'PIM status: {txt}')
            m = decode_status(s)
            self.PV['pim_Status'].set_valueAndTimestamp([txt],ts)
            try:
                self.PV['pim_LTMP'].set_valueAndTimestamp([float(m['LTMP'])],ts)
                self.PV['pim_LRBK'].set_valueAndTimestamp([float(m['LRBK'])],ts)
            except:
                printe(f'Corrupted PIM Status: `{txt}`')
                return False

            # update laser status if changed
            try:    currentStatus = {'0':'OFF','1':'MinPower','2':'MaxPower'}[m['LRT']]
            except Exception as e:
                printw(f'Unexpected laser status: {s}')
                return False
            if currentStatus != self.PV['laser'].value[0]:
                print(f"laser status changed to {currentStatus} from `{self.PV['laser'].value[0]}")
                self.PV['laser'].set_valueAndTimestamp([currentStatus],ts)

            return True

        #elif txt[1] =='?':
        #    #Error message
        #    self.PV['status'].set_valueAndTimestamp(txt[1:], ts)
        else:
            self.PV['msg'].set_valueAndTimestamp(txt[2:-1], ts)
            printe(f'Unexpected message: `{txt}`')
            return False
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
                self.execute_command('STS?', update_status=False)

            # Wait/Receive data from device
            self.timestamp = time.time()
            try:
                payload = get_data()
            except KeyboardInterrupt:
                printi(' Interrupted')
                SerDev.close()
                sys.exit(1)
            except serial.SerialException as e:
                printe(f'ERR: serialException: {e}')
                SerDev.close()
                Dev.EventExit.set()
                break

            if not self.initialized:
                printvv('Initialization not finished')
                # __init__() did not finish, no sense to proceed further
                continue
            pv_cycle.value[0] += 1

            if not self.handle_devPacket(payload):
                if not self.forcePublish:
                    continue

            #print('publish all modified parameters of '+self.name)
            ts = timer()
            shippedBytes = self.publish()
            self.forcePublish = False
            printv(f'shipped: {shippedBytes}')

        printi('########## listener exit ##########')
#,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
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
    parser.add_argument('-p','--port', type=int, default=9700, help=\
'Serving port.') 
    parser.add_argument('-r','--ref', default='3.3', help=\
'Reference voltage.')
    parser.add_argument('-S','--stop',  action='store_true', help=\
'Do not start')
    parser.add_argument('-v', '--verbose', action='count', default=0, help=\
      'Show more log messages (-vv: show even more).')
    parser.add_argument('tty', nargs='?', default='/dev/ttyUSB0', help=\
'Serial device for communication with hardware')
    pargs = parser.parse_args()

    SerDev = open_serdev()

    #liteserver.Server.dbg = pargs.dbg >> 2
    DevInstance = Dev()
    devices = [DevInstance]

    server = liteserver.Server(devices, interface=pargs.interface,
        port=pargs.port)

    print('`'*79)

    server.loop()
