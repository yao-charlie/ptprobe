from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS
from datetime import datetime

import sqlite3

class SampleSink:
    """The abstract base class for sinks to record streaming sample data"""

    def write(self, sample):
        """Write sample data to the sink.

        :param sample: The data sample as a iterable container (e.g. list)
        :type sample: list
        :returns: None
        """
        raise NotImplementedError("Abstract method")

class CsvSampleSink (SampleSink):
    """Write sample data to a text file in comma separated value (CSV) format"""

    def __init__(self, filename):
        self.filename = filename
        self.hf = None

    def open(self): 
        self.close()
        self.hf = open(self.filename, "w")

    def close(self):
        if self.hf is not None:
            self.hf.close()
        self.hf = None

    # def write(self, sample):
    def write(self, sample, port = 0, queue = False):
        seps = [', ']*len(sample)
        seps[-1] = '\n'
        # print(sample[4])
        # queue.put([item, sample[4], sample[5]]
        if queue.qsize()<2:
            queue.put([port, sample])
        for v,s in zip(sample,seps):
            self.hf.write("{}{}".format(v,s).replace("[","").replace("]",""))

class ListSampleSink (SampleSink):
    """Write sample data to an array"""

    def __init__(self):
        self.data = []

    def write(self, sample):
        self.data.append(sample)

class InfluxDBSampleSink (SampleSink):
    """Write sample data to InfluxDB Cloud"""

    def __init__(self, token, org, bucket):
        self.client = None
        self.token = token
        self.org = org
        self.bucket = bucket
        self.board_id = 0

    def set_board_id(self, id):
        self.board_id = id

    def open(self):
        self.client = InfluxDBClient(
                url="https://us-west-2-1.aws.cloud2.influxdata.com",
                token=self.token,
                org=self.org)

    def close(self):
        if self.client is not None:
            self.client.close()

    def write(self, sample):
        if self.client is None:
            raise RuntimeError("No sink initialized for write")

        write_api = self.client.write_api(write_options=SYNCHRONOUS)

        pt = Point("board").tag("board_id", self.board_id)
        for ich in range(4):
            pt.field("T{}".format(ich), sample[3][ich])
            pt.field("Tref{}".format(ich), sample[4][ich])
            pt.field("P{}".format(ich), sample[5][ich])
            pt.field("Tfault{}".format(ich), sample[2][ich])
        pt.time(datetime.utcnow(), WritePrecision.MS)
        #pt.time(sample[0], WritePrecision.MS)

        write_api.write(self.bucket, self.org, pt)

class SQLiteSampleSink (SampleSink):
        
    def __init__(self, url):
        self.client = None
        self.url=url
        self.board_id = 0

    def create(self, com, date):
        if self.client is None:
            raise RuntimeError("No sink initialized for write")
        cursor = self.client.cursor()
        # cursor.execute("CREATE TABLE {}_{}(channel, timestamp, active_T, fault_T, temperature, ref_temperature, pressure)".format(com, date))
        cursor.execute("CREATE TABLE {}_{}(channel, timestamp, fault_T, temperature, ref_temperature, pressure)".format(com, date))

    def open(self):
        self.client = sqlite3.connect(self.url)

    def close(self):
        if self.client is not None:
            self.client.close()

    def write(self, sample, com, date):
        if self.client is None:
            raise RuntimeError("No sink initialized for write")
        
        #Table exist check?
        
        cursor = self.client.cursor()

        # pt = Point("board").tag("board_id", self.board_id)
        # for ich in range(4):
        #     pt.field("T{}".format(ich), sample[3][ich])
        #     pt.field("Tref{}".format(ich), sample[4][ich])
        #     pt.field("P{}".format(ich), sample[5][ich])
        #     pt.field("Tfault{}".format(ich), sample[2][ich])
        # pt.time(datetime.utcnow(), WritePrecision.MS)

        #need to parse sample into table format?

        for ich in range(4):
            cursor.execute("""
                    INSERT INTO {}_{} VALUES
                        (Channel{}, {}, {}, {}, {}, {} )
                """.format(
                    #Table:
                    com, 
                    date, 

                    ich, #channel
                    datetime.utcnow(), #timestamp
                    sample[2][ich], #fault_T
                    sample[3][ich], #temperature
                    sample[4][ich], #ref_temp
                    sample[5][ich], #pressure
                    ))


