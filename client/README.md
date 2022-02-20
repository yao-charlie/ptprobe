# PT Probe Client

The PT Probe Client provides an interface to the sensor board, using serial
communication and implementing the commands to configure the board, check status
and return samples of pressure and temperature. 

The client consists of a `Controller`, which provides access to the board. The
`Controller` can be instanciated and called directly for one-shot sampling,
board configuration, and status reporting. It also provides a method for 
free-running sample collection, which will write data back to a caller-provided
`SampleSink`. This is designed for execution in a separate thread and supports
user or caller termination. 

## Examples

Useage examples can be found in the `./examples` path. These include a basic
one-shot read example and a free-running sample collection that writes data to
a CSV file. 

The following shows a simple example of reading the temperature from the probe
on channel 0. In this example, the board is connected on the serial port 
`/dev/ttyACM0`.

```python
import board
pt = board.Controller('/dev/ttyACM0')
print("Board ID: {}".format(pt.board_id()))
print("Temp. 0 (C): {}".format(pt.temperature(0)[0]))
```

Note that the one-shot reads return a tuple with two entries: the value
and an error code. If the error code is non-zero, the value may be invalid.
