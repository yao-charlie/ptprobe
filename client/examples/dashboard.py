import multiprocessing
import logging
import argparse
import multi_read_to_csv
import read_kx134_to_csv

import dash
from dash import dcc, html, ALL
import numpy as np
from scipy import signal
import matplotlib.pyplot as plt

import plotly.graph_objects as go
import plotly.express as px
from dash.dependencies import Output, Input

import os
import sys

def info(title):
    print(title)
    print ('module name:{}'.format(__name__))
    print ('parent process:{}'.format(os.getppid()))
    print ('process id:{}'.format(os.getpid()))

def f(name):
    sys.stdout = open(str(os.getpid()) + ".out", "w", buffering=0)
    info('function f')
    print ('hello{}'.format(name))


def dictparser(item):
    dict={}
    dict[item[0]] = item[1]
    return dict

#possibly needed for platform specific behaviour from multiprocessing:
# if platform.system() == 'Windows':
#     multiprocessing.set_start_method('spawn')



sensor_data_columns = [ "Time", 
                        "CH_0 Active T",    "CH_1 Active T",    "CH_2 Active T",    "CH_3 Active T",	
                        "CH_0 Error Code",    "CH_1 Error Code",    "CH_2 Error Code",    "CH_3 Error Code",
                        "CH_0 Temp",	"CH_1 Temp",	"CH_2 Temp",	"CH_3 Temp",	
                        "CH_0 Ref Temp",	"CH_1 Ref Temp",	"CH_2 Ref Temp",	"CH_3 Ref Temp",
                        "CH_0 Pressure",	"CH_1 Pressure",	"CH_2 Pressure",	"CH_3 Pressure"
                    ]

sensor_data = {}
accel_sensor_data = {}



external_stylesheets = [
    {
        "href": "https://fonts.googleapis.com/css2?"
        "family=Lato:wght@400;700&display=swap",
        "rel": "stylesheet",
    },
]

app = dash.Dash(__name__, external_stylesheets=external_stylesheets)
app.title = "Prototype Sensors"

ptLog = {}
logTime = 100
inter_interval=2000
logSamples = logTime/inter_interval*1000


def serve_layout():
    return(
        html.Div(
            children=[
                dcc.Interval(
                    id='interval',
                    interval=inter_interval, # in milliseconds (>1000 as component updates can't occur faster)
                    n_intervals=0
                ),
                html.Div(
                    children=[
                        html.H1(
                            children="Prototype Sensors", className="header-title"
                        ),
                    ],
                    className="header",
                ),
                html.Div(
                    children=[],
                    id="graph_list",
                    className="wrapper",

                ),
            ]
        )
    )


app.layout = serve_layout

@app.callback(
    [
    Output("graph_list", "children"),
        ],

    Input("interval", "n_intervals"),
    
)
def update_intervals(interval):

    arb_graph_figure_list = []
    temperature_set = 3
    pressure_set = 5
    uSecondsToSeconds = 1000000
    dimDict = {
                    0:'x',
                    1:'y',
                    2:'z'
                }

    if args.ports:
        for item in args.ports:
            sensor_data[item] = dictparser(queueList[item].get())[item]
            ptLog[item].append(sensor_data[item])
            if len(ptLog[item]) > logSamples:
                ptLog[item].pop(0)

    if args.accelPorts:
        for item in args.accelPorts:
            accel_sensor_data[item] = dictparser(accelQueueList[item].get())[item]
            numpyAccelData = np.array(accel_sensor_data[item])

            # xfft = fft.fft(numpyAccelData[:,1])
            # xfreq = fft.fftfreq(numpyAccelData[:,1].shape[-1])

            # # FFT:
            # xfreqfigure = go.Figure(
            #     data=[go.Scatter(x=xfreq, y=xfft)]
            # )
            # arb_graph_figure_list.append(dcc.Graph(figure=xfreqfigure))

            #PSD:
            
            samplingFreq = uSecondsToSeconds/np.average(numpyAccelData[:,0])

            for dimension in range(3):
                dim_spdf, dim_spd = signal.periodogram(numpyAccelData[:,dimension+1], samplingFreq)

                dim_spdfFigure = go.Figure(
                    data = [go.Scatter(x=dim_spdf, y=dim_spd), ],
                    layout_yaxis_range=[0,10],
                    layout_title="PSD Port {}, {}".format(item, dimDict[dimension]),
                    layout_xaxis_labelalias = "test"
                )
                arb_graph_figure_list.append(dcc.Graph(figure=dim_spdfFigure, className="psdGraph"))


    for item in args.ports:

        for channel in range(4):
            channelTemp = []
            for i, sample in enumerate(ptLog[item]):
                channelTemp.append(ptLog[item][i][temperature_set][channel])

            index_temp_figure = px.line(channelTemp, title="Port {}- Channel {}-Temp".format(item, channel))

            arb_graph_figure_list.append(dcc.Graph(figure=index_temp_figure, className="graph"))


        #TODO: refactor to function taking in parameter, returning list of figures:
        for channel in range(4):
            channelPressure = []
            for i, sample in enumerate(ptLog[item]):
                channelPressure.append(ptLog[item][i][pressure_set][channel])

            index_pressure_figure = px.line(channelPressure, title="Port {}- Channel {}-Pressure".format(item, channel))

            arb_graph_figure_list.append(dcc.Graph(figure=index_pressure_figure, className="graph"))

    return [arb_graph_figure_list]


if __name__ == '__main__':

    format = "%(asctime)s: %(message)s"
    logging.basicConfig(format=format, level=logging.INFO, datefmt="%H:%M:%S")

    parser = argparse.ArgumentParser(description='Read PTProbe sensors over serial')

    # Not valuable:
    parser.add_argument('-m','--max-count', type=int, default=0,  
            help='DEPRECATED. DO NOT USE as sampling rates between PT and accel are not the same.')
    
    parser.add_argument('-t','--timeout', type=int, default=0,  
            help='Collection time for sampling (s). Default is 0 (no timeout). The nominal sample rate is 5Hz.')
    parser.add_argument('-p', '--ports',  nargs='+',
            help='Serial port name(s). No default.')
    parser.add_argument('-a', '--accelPorts', nargs='+',
            help='Serial port name(s) for accelerometers. No default')
    parser.add_argument('-r', '--accelRate', type=float, default=1.0,
            help='Aceelerator reporting rate in seconds. Default is 1s')
    parser.add_argument('-f', '--filename', default='', help='Prefix filename for CSV file data output with extension as specified')
    args = parser.parse_args()
    logging.info("Starting demo")
    logging.info(args)

    queueList = {}
    accelQueueList = {}
    process = {}


    if args.ports:
        for item in args.ports:
            queueList[item] = multiprocessing.Queue()
            ptLog[item] = []

        for item in args.ports:
            dataWriter = multi_read_to_csv.readTo(args.filename, [item])
            process[item] = multiprocessing.Process(target = dataWriter.readToCSV, args = (args.max_count, args.timeout, queueList[item]))
            process[item].start()

    if args.accelPorts:
        print("accelPorts Running")
        for item in args.accelPorts:
            accelQueueList[item] = multiprocessing.Queue()

        for item in args.accelPorts:
            dataWriterAccel = read_kx134_to_csv.readTo(args.filename+"-Accel", item)
            process[item] = multiprocessing.Process(target = dataWriterAccel.readToCSV, args = (args.max_count, args.timeout, accelQueueList[item], args.accelRate))
            process[item].start()


    print("running server")
    # app.run(debug=False, host='192.168.1.217', port='7591')
    app.run(debug=False)

    print("joining main processes")
    for item in args.ports+args.accelPorts:
        process[item].join()



