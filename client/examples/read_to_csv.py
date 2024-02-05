import sys
sys.path.append('../src/ptprobe')
import os
from datetime import datetime

import logging
import threading
import time
import argparse
import board
from sinks import CsvSampleSink

boards = []
sinks = []
threads = []

if __name__ == "__main__":
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

    (prefix, extension) = os.path.splitext(args.filename)  

    for index, item in enumerate(args.ports):
        logging.info("Creating sink for {}".format(item))
        port_label = item.split('/')[-1] 
        sink_name = '_'.join(port_label)
        sinks.append(CsvSampleSink("{}-{}-{}.{}".format(prefix, sink_name, datetime.now().strftime("%Y-%m-%d_%Hh-%Mm-%Ss")), extension))
        sinks[index].open()


    for index, item in enumerate(args.ports):
        boards.append(board.Controller(port=item, sinks=[sinks[index]]))
        logging.info("Board IDs: {}".format(boards[index].board_id()))


    logging.info("Main: creating threads")

    for index, item in enumerate(args.ports):
        threads.append(threading.Thread(target=boards[index].collect_samples, args=(args.max_count,)))
        logging.info("Creating thread for {}".format(item))
        threads[index].start()


    stop_collection_lambda = lambda: list(map(lambda board: board.stop_collection(), boards))
    timer = threading.Timer(args.timeout, stop_collection_lambda)

    if args.timeout > 0:
        timer.start()

    heartbeat = 0
    while any(list(map(lambda thread:thread.is_alive(), threads))):
        heartbeat += 1
        logging.info("..{}".format(heartbeat))
        time.sleep(1.)

    # here either the timer expired and called halt or we processed 
    # max_steps messages and exited
    logging.info("Main: cancel timer")
    timer.cancel()

    logging.info("Main: calling joins")
    for thread in threads:
        thread.join()
        
    logging.info("Main: closing sinks")
    for sink in sinks:
        sink.close()
        

    logging.info("Main: done")





