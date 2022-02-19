import sys
sys.path.append('../ptprobe')

import argparse
import board

if __name__ == "__main__":
    
    parser = argparse.ArgumentParser(description='Read PTProbe sensors over serial')
    parser.add_argument('port', type=str, help='Serial port name, e.g. /dev/ttyACM0')
    args = parser.parse_args()

    pt = board.Controller(port=args.port)

    print("Board ID: {}".format(pt.board_id()))

    for ich in range(4):
        print("Channel {}:".format(ich))
        print("  Probe temp. (C): {}".format(pt.temperature(ich)[0]))
        print("  Board temp. (C): {}".format(pt.ref_temperature(ich)[0]))
        print("  Pressure (kPa):  {}".format(pt.pressure(ich)[0]))




