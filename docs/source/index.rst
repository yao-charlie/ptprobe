PT Probe Documentation
===================

The **PT Probe** is a 4 channel temperature and pressure sensing module that connects to the PC via USB.
It is composed of a hardware module based on an array of MAX31850 ICs that handle conversion from the raw K-type thermocouple signal to a temperature value.
The MAX31850 includes an internal cold-junction reference and can report diagnostic information (temperature out of range, probe not connected). 
Communication between the host and sensor is handled over a one-wire bus.
Pressure sensing is handled by a raw 5V analog transducer, whose value is digitized by an onboard ADC. 

Please see the :doc:`installation` section for instructions on configuring the host and installing the driver software. See the :doc:`usage` section for details and examples of calibrating and using the **PT Probe**.

Contents
--------

.. toctree::

    installation
    usage

