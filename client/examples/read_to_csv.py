import sys
sys.path.append('../src/ptprobe')

import logging
import threading
import time
import argparse
import board
from datetime import datetime
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
    # parser.add_argument('-f', '--filename', default='test.csv', help='CSV file for data output')
    args = parser.parse_args()

    logging.info("Starting demo")
    logging.info(args)

    for index, item in enumerate(args.ports):
        # print(index)
        # print(item)
        logging.info("Creating sink for {}".format(item))
    # logging.info("Creating sink {}".format(args.filename))
        sinks.append(CsvSampleSink("{}-{}.csv".format(item, datetime.now().strftime("%Y-%m-%d_%Hh-%Mm-%Ss"))))
    # csv = CsvSampleSink(args.filename)
        sinks[index].open()
    # csv.open()



    for index, item in enumerate(args.ports):
        boards.append(board.Controller(port=item, sinks=[sinks[index]]))
    # pt = board.Controller(port=args.ports, sinks=[csv])
        logging.info("Board IDs: {}".format(boards[index].board_id()))
    # logging.info("Board ID: {}".format(pt.board_id()))


    logging.info("Main: creating threads")

    for index, item in enumerate(args.ports):
        threads.append(threading.Thread(target=boards[index].collect_samples, args=(args.max_count,)))
    # x = threading.Thread(target=pt.collect_samples, args=(args.max_count,))
        logging.info("Creating thread for {}".format(item))
    # logging.info("Main: starting thread")
        threads[index].start()
    # x.start()


    stop_collection_lambda = lambda: list(map(lambda board: board.stop_collection(), boards))
    
    # timer = threading.Timer(args.timeout, list(map(lambda board:board.stop_collection(), boards)))
    timer = threading.Timer(args.timeout, stop_collection_lambda)
    # t = threading.Timer(args.timeout, pt.stop_collection)
    if args.timeout > 0:
        timer.start()

    # print(threads): [<Thread(Thread-1 (collect_samples), started 28484)>, <Thread(Thread-2 (collect_samples), started 29488)>]
    # print(sinks): [<sinks.CsvSampleSink object at 0x0000020B6AAE3690>, <sinks.CsvSampleSink object at 0x0000020B696B5D10>]
    # print(boards): [<board.Controller object at 0x0000020B6AC3CE90>, <board.Controller object at 0x0000020B6AC3D090>]
    # print(list(map(lambda thread:thread.is_alive(), threads))): [True, True]

    heartbeat = 0
    while any(list(map(lambda thread:thread.is_alive(), threads))):
    # while x.is_alive():
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
    # x.join()
        
    logging.info("Main: closing sinks")
    for sink in sinks:
        sink.close()
    # csv.close()
        

    logging.info("Main: done")





