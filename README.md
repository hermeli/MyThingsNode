# MyThingsNode
Code for The Things Node

*My Things Node (SWy, 30.12.2020)*

This sketch is forked from
https://github.com/ch2i/arduino-node-lib
which was forked from the main 
https://github.com/TheThingsNetwork/arduino-node-lib
 
Contact: wyss (AT) superspider (DOT) net
 
- It uses Ultra Low Power techniques and consumes about 40uA sleep 
  current on a battery powered Node
- Binary (or Cayenne) is used as data upstream format
- Upstream of temperature and battery voltage to the cloud at connection_interval minutes or button release event 
- First byte of downstream data is connection_interval (in minutes)
- Downstreamed connection_interval is stored in EEPROM and readout upon reboot
  