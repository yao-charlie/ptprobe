import multiprocessing
import logging
import argparse
import multi_read_to_csv

import dash
from dash import dcc, html, ALL
import pandas as pd
import numpy as np
import plotly.graph_objects as go
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
# if platform.system() == 'Windows':
#     multiprocessing.set_start_method('spawn')


data = pd.read_csv("avocado.csv")
data["Date"] = pd.to_datetime(data["Date"], format="%Y-%m-%d")
# data.sort_values("Date", inplace=True)

sensor_data_columns = [ "Time", 
                        "CH_0 Active T",    "CH_1 Active T",    "CH_2 Active T",    "CH_3 Active T",	
                        "CH_0 Error Code",    "CH_1 Error Code",    "CH_2 Error Code",    "CH_3 Error Code",
                        "CH_0 Temp",	"CH_1 Temp",	"CH_2 Temp",	"CH_3 Temp",	
                        "CH_0 Ref Temp",	"CH_1 Ref Temp",	"CH_2 Ref Temp",	"CH_3 Ref Temp",
                        "CH_0 Pressure",	"CH_1 Pressure",	"CH_2 Pressure",	"CH_3 Pressure"
                    ]
# sensor_data = pd.read_csv(r"test.csv", header=None, names=sensor_data_columns)
sensor_data = {}

#note that sensor data from file is actually delayed a few seconds. It is not instantaneous.


external_stylesheets = [
    {
        "href": "https://fonts.googleapis.com/css2?"
        "family=Lato:wght@400;700&display=swap",
        "rel": "stylesheet",
    },
]

app = dash.Dash(__name__, external_stylesheets=external_stylesheets)
app.title = "Prototype Sensors"




def serve_layout():
   

    return(
        html.Div(
            children=[
                dcc.Interval(
                    id='interval',
                    interval=1000, # in milliseconds
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
                    # children=[
                    #     html.Div([
                    #         html.P(
                    #             children="Motor RPM",
                    #             className="menu-title"
                    #         ),
                    #         dcc.Graph(
                    #             id="arb_gauge",                           
                    #         ),
                    #         ],
                    #         className="card",
                    #     ),
                    # ],
                    id="gauge_list",
                    className="wrapper",

                ),
            ]
        )
    )



    # gauge_list = []
    # for item in args.ports:
    #     gauge_list.append(
    #     html.Div(
    #                 children=[
    #                     html.Div([
    #                         html.P(
    #                             children=item,
    #                             className="menu-title"
    #                         ),
    #                         dcc.Graph(
    #                             id=item,                           
    #                         ),
    #                         ],
    #                         className="card",
    #                     ),
    #                 ],
    #                 className="wrapper",
    #             ),)


app.layout = serve_layout

@app.callback(
    [
        Output("gauge_list", "children"),
        ],
    Input("interval", "n_intervals")
)
def update_intervals(interval):
    
    # print(queue.qsize())
    for item in args.ports:
        sensor_data[item] = dictparser(queueList[item].get())[item]
    # sensor_data = dictparser(queueList[].get())["COM5"]
    # print("sensor data:")
        print(sensor_data[item])
    # print('end data')


    # test = []

    # for i in "wtf":
    #     test.append(f"number {i}")

    arb_gauge_figure = go.Figure()
    
    arb_gauge_figure.add_trace(go.Indicator(
        value = sensor_data[item][3][0],
        # delta = {'reference': 160},
        gauge = {'axis': {'visible': True}},
        # gauge_axis_dtick=10, 
        domain = {'x': [0, 1], 'y': [0, 1]},
        # title = {'text': index},
        mode = "gauge+number"
        ))

    arb_gauge_figure_list = []

    temperature_set = 3
    channel = 0

    for item in args.ports:
        for channel in range(4):
            index_guage_figure = go.Figure(go.Indicator(
                value = sensor_data[item][temperature_set][channel],
                gauge = {'axis': {'visible': True}},
                domain = {'x': [0, 1], 'y': [0, 1]},
                mode = "gauge+number"
            ))
            arb_gauge_figure_list.append(dcc.Graph(figure=index_guage_figure))

    return [arb_gauge_figure_list]

    # return html.Div(
    #     # [arb_gauge_figure]
    #     # [html.Div(f'{item}') for item in args.ports]
    #     [test]
    #     )
    # print(arb_gauge_figure_list)



    # return html.Div(
    #             [((
    #                 html.P(
    #                     children=item,
    #                     className="menu-title"
    #                 ),
    #                 dcc.Graph(
    #                     id=item,                           
    #                 )
    #             ) for item in args.ports)],

    #         className="card",
    #         )

# @app.callback(
#     [
#         Output("gauge_list", "figure"),
#         ],
#     Input("interval", "n_intervals")
# )
# def update_graphs(interval):
#     for item in args.ports:
#         sensor_data[item] = dictparser(queue)


if __name__ == '__main__':


    queue = multiprocessing.Queue()

    format = "%(asctime)s: %(message)s"
    logging.basicConfig(format=format, level=logging.INFO, datefmt="%H:%M:%S")

    parser = argparse.ArgumentParser(description='Read PTProbe sensors over serial')
    parser.add_argument('-m','--max-count', type=int, default=0,  
            help='Maximum number of samples to record. Default 0 (no maximum)')
    parser.add_argument('-t','--timeout', type=int, default=0,  
            help='Collection time for sampling (s). Default is 0 (no timeout). The nominal sample rate is 5Hz.')
    parser.add_argument('-p', '--ports', default='/dev/ttyACM0',  nargs='+',
            help='Serial port name(s). Default is /dev/ttyACM0.')
    parser.add_argument('-f', '--filename', default='', help='Prefix filename for CSV file data output with extension as specified')
    args = parser.parse_args()
    logging.info("Starting demo")
    logging.info(args)

    queueList = {}
    process = {}

    for item in args.ports:
        queueList[item] = multiprocessing.Queue()

    # with multiprocessing.Manager() as manager:

    for item in args.ports:
        dataWriter = multi_read_to_csv.readTo(args.filename, [item])
        process[item] = multiprocessing.Process(target = dataWriter.readToCSV, args = (args.max_count, args.timeout, queueList[item]))
        process[item].start()

    # p1 = multiprocessing.Process( target= dataWriter.readToCSV, args = (args.max_count, args.timeout, queue))
    # p2 = dashboard
    # p1.start()

    # p_consumer = multiprocessing.Process(target = consumer, args = (queue,))
    # p_consumer.start()
    app.run_server(debug=True)

    for item in args.ports:
        process[item].join()

    # p1.join()
    # p_consumer.join()

