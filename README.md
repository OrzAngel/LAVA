# LAVA

*Temperature + Pebble = heat + stone = LAVA!*

A Pebble smartwatch app that monitors temperature on remote Arduino sensor with configurable display and alert modes

Team member : Juncheng Chen, Felix Fan, Yiwei Guo. 

## Description
This project is a Pebble smartwatch application that is able to get data from and send control information to a remote sensor and display driven by an Arduino microcontroller.

The system consists of three major components, described as follows:

#### Sensor and Display:

This component is an Arduino including a temperature sensor, a seven-segment display and an RGB light, and a c++ program that reads the temperature and writes to the serial port once a second.

#### Middleware: 

A C++ program that handles all communication between the sensor and the user interface. The Arduino is connected via USB and the user interface is connected via Internet.

#### User interface: 

The user interface will be a Pebble smartwatch application, which communicates with the middleware over the Internet via an iOS phone.


## Function

•The user is able to see the most recent temperature sensor reading as well as the average, low, and hightemperature sensor reading for the past hour\
•The user is able to decide whether to see temperature readings (and statistics) either in Fahrenheit or Celsius. Additionally, the user is also able to change the 7-segment display on the sensor to show the temperature either in Fahrenheit or Celsius.\
•The user is able to put the sensor into a stand-by mode in which it is not reporting temperature readings(either to the user interface or to the display)\
•If the user interface becomes disconnected from the middleware, or if the middleware cannot get a reading from the sensor,, an appropriate message would be shown to the user when it tries to connect.
