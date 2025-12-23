#Automatically created configuration file for pypeto.

_Namespace = "LITE"

dev = "localhost:dev1"

#``````````````````Definitions````````````````````````````````````````````````
# Python expressions and functions, used in the spreadsheet.
def span(x,y): return {'span':[x,y]}
def color(*v): return {'color':v[0]} if len(v)==1 else {'color':list(v)}
def font(size): return {'font':['Arial',size]}

#``````````````````PyPage Object``````````````````````````````````````````````
class PyPage():
    def __init__(self, instance="localhost;9700", title=None):
        """instance: unique name of the page.
        For single-device page it is commonly device name or host;port.
        title: is used for name of the tab in the GUI. 
        """
        print(f'Instantiating Page {instance,title}')
        hostPort = instance
        dev = f"{hostPort}:dev1:"
        print(f'Controlling device {dev}')
        server = f"{hostPort}:server:"
        #pvplot = f"python3 -m pvplot -a L:{dev} x,y"

        #``````````Mandatory class members starts here````````````````````````
        self.namespace = 'LITE'
        self.title = f'PeakSim@{hostPort}' if title is None else title

        #``````````Page attributes, optional`````````````````````````
        self.page = {**color(240,240,240)}
        #self.page['editable'] = False

        #``````````Definition of columns`````````````````````````````
        self.columns = {
            1: {'width': 70,"justify": "right", "color": [230,230,230]},
            2: {"width": 100},
            3: {"width": 60,"justify": "right", "color": [230,230,230]},
            4: {"width": 100},
            5: {"width": 60,"justify": "right"},
        }

        #``````````Definition of rows````````````````````````````````
        self.rows = [
[{'Control of the PoF SIM':{**span(5,1),**font(14),"justify": "center"}}],
['Version:',{dev+"version":span(2,1)},''],
["adcScale:", dev+"adcScale",'vRef:',dev+'vRef',''],
["Run", dev+"run","rps,cycle:", dev+"rps", dev+"cycle"],
[{dev+"status":{**span(5,1),'color':'lightcyan'}},'','','',''],
#["version", dev+"version"],
["send", dev+"send"],
["msg:", {dev+"msg":span(4,1)},'','',''],
[{"ADC sampling rate:":{**span(3,1),"justify": "right"}},'','',dev+"srate"],
["recLimit:", dev+"recLimit","timeout:", dev+"timeout"],
["nsamples:", dev+"nsamples","nstats:", dev+"nstats"],
["mean:", dev+"mean","rms:", dev+"rms"],
["p2p:", dev+"p2p"],
#["samples", dev+"samples"],
#["xaxis", dev+"xaxis"],
        ]
