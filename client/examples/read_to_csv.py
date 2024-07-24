import sys
sys.path.append('../src/ptprobe')
import os
from datetime import datetime
import operator

import logging
import threading
import time
import argparse
import board
from sinks import CsvSampleSink

#Google Sheets imports:
from google.auth.transport.requests import Request
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build
from googleapiclient.errors import HttpError
SCOPES = ["https://www.googleapis.com/auth/spreadsheets"]


#TODO: auto create graphs?
#TODO: add modulo
#TODO: add fields arguments

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
    parser.add_argument('-g', '--gsheets_ID', default='', help='Google Sheets UUID link.')
    parser.add_argument('-n', '--newGSheetTabs', default='', help='Flag to generate new Google Sheets Tabs on first creation of sheet.')
    parser.add_argument('-w', '--sampleWindow', default='', help='How many samples to window in GSheets')


    args = parser.parse_args()

    #Google sheets authentication:

    if args.gsheets_ID:
        SCOPES = ["https://www.googleapis.com/auth/spreadsheets"]

        creds = None
        # The file token.json stores the user's access and refresh tokens, and is
        # created automatically when the authorization flow completes for the first
        # time.
        if os.path.exists("token.json"):
            creds = Credentials.from_authorized_user_file("token.json", SCOPES)
        # If there are no (valid) credentials available, let the user log in.
        if not creds or not creds.valid:
            if creds and creds.expired and creds.refresh_token:
                creds.refresh(Request())
            else:
                flow = InstalledAppFlow.from_client_secrets_file(
                    "..\..\security\credentials_OAuth2.json", 
                    SCOPES
                )
                creds = flow.run_local_server(port=0)
            # Save the credentials for the next run
            with open("token.json", "w") as token:
                token.write(creds.to_json())

        try:
            service = build("sheets", "v4", credentials=creds)
            # Call the Sheets API
            sheet = service.spreadsheets()
        except HttpError as err:
            print(err)

    logging.info("Starting demo")
    logging.info(args)

    (prefix, extension) = os.path.splitext(args.filename)  

    for index, item in enumerate(args.ports):
        logging.info("Creating sink for {}".format(item))
        port_label = item.split('/')[-1] 
        sink_name = ''.join(port_label)
        sinks.append(CsvSampleSink("{}-{}-{}{}".format(prefix, sink_name, datetime.now().strftime("%Y-%m-%d_%Hh-%Mm-%Ss"), extension)))
        sinks[index].open()


    for index, item in enumerate(args.ports):
        boards.append(board.Controller(port=item, sinks=[sinks[index]]))
        logging.info("Board IDs: {}".format(boards[index].board_id()))


    logging.info("Main: creating threads")

    if args.gsheets_ID:
        for index, item in enumerate(args.ports):

            #Create all the new tabs

            if args.newGSheetTabs:
                try:
                    result = (
                    sheet.batchUpdate(
                        spreadsheetId=args.gsheets_ID, 
                        body = {
                        'requests': [{
                            'addSheet': {
                                'properties': {
                                    'title': item,
                                }
                            }
                        }]
                    }
                        )
                        .execute()
                    )
                except HttpError as err:
                    print(err)
                finally:
                    time.sleep(1.) #Google Sheets DDOS limits
                #Comment this out re-running with existing tabs.

            threads.append(threading.Thread(
                target=boards[index].collect_samples, 
                args=(args.max_count,),
                kwargs={
                    'gSheetsLink': args.gsheets_ID,
                    'sheet': item,
                    'modulo': args.sampleWindow
                    },
                ))
            logging.info("Creating thread for {}".format(item))
            threads[index].start()

    else:
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





