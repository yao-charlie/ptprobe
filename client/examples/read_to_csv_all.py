import sys
sys.path.append('../src/ptprobe')

import datetime
import logging
import threading
import time
import argparse
import board
from sinks import CsvSampleSink

#Add COM ports of boards here:
ports = ["COM1", "COM2"]

#No action:
boards = []
sinks = []

if __name__ == "__main__":
    format = "%(asctime)s: %(message)s"
    logging.basicConfig(format=format, level=logging.INFO, datefmt="%H:%M:%S")
    
    parser = argparse.ArgumentParser(description='Read PTProbe sensors over serial')
    parser.add_argument('-m','--max-count', type=int, default=0,  
            help='Maximum number of samples to record. Default 0 (no maximum)')
    parser.add_argument('-t','--timeout', type=int, default=0,  
            help='Collection time for sampling (s). Default is 0 (no timeout). The nominal sample rate is 5Hz.')
    parser.add_argument('-p', '--port', default='/dev/ttyACM0',  
            help='Serial port name. Default is /dev/ttyACM0.')
    parser.add_argument('filename', help='CSV file for data output')
    args = parser.parse_args()

    logging.info("Starting demo")
    logging.info(args)

    logging.info("Creating sink {}".format(args.filename))
    csv = CsvSampleSink(args.filename)
    csv.open()
    
    currentDT = datetime.datetime.now()

    for item in range(len(ports)):
        sinks[item] = CsvSampleSink("{}_{}-{}-{}_{}-{}-{}".format(ports[item], currentDT.year, currentDT.month, currentDT.day, currentDT.hour, currentDT.minute, currentDT.second))
        boards[item] = board.Controller(ports=ports[item], sinks=[sinks[item]])

    pt = board.Controller(port=args.port, sinks=[csv])
    logging.info("Board ID: {}".format(pt.board_id()))

    logging.info("Main: creating thread")
    x = threading.Thread(target=pt.collect_samples, args=(args.max_count,))
    logging.info("Main: starting thread")
    x.start()
    
    t = threading.Timer(args.timeout, pt.stop_collection)
    if args.timeout > 0:
        t.start()

    heartbeat = 0
    while x.is_alive():
        heartbeat += 1
        logging.info("..{}".format(heartbeat))
        time.sleep(1.)

    # here either the timer expired and called halt or we processed 
    # max_steps messages and exited
    logging.info("Main: cancel timer")
    t.cancel()
    logging.info("Main: calling join")
    x.join()
    logging.info("Main: closing sink")
    csv.close()
    logging.info("Main: done")





