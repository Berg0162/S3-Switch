# S3-Switch &nbsp;&nbsp;Smart-Solar-Surplus-Switch
In Europe many households are equiped with modern meters (a.k.a. smart meters) for billing the energy consumption without human interference. These smart meters send readings to the energy supplier automatically but they also have a P1 port that gives you access to all measured energy data.
The present project uses a commercial device to read constantly this P1 port: [Homewizard P1 Meter](https://www.homewizard.com/p1-meter/). This P1 Meter is compatible with most modern meters in Europe.</br>

It becomes even more interesting when your solar panels are involved in the energy equasion. The Homewizard P1 meter is able to monitor and record the energy management of the household at any moment in time. That includes the solar energy sent from the inverter back into the power grid with revenue grade accuracy. The total energy consumption is the basis of the utility bills from the energy supplier(s) every month. One wants a maximum benefit of the solar energy for good reasons!</br>

The net power is the difference between the power the household is importing from and the power the inverter exports to the utility grid at any moment in time. When the net power is negative the export (solar production) exceeds the import of power and you might want to switch on extra electric appliances (like a battery charger, kitchen boiler or something else) to have maximum benefit of the solar surplus power. A smart switch with exactly that capabibilty would come in very handy! No doubt commercial parties (like [Homewizard](https://www.homewizard.com/energy-plus/)) offer this service but always at a recurring pricing model and a security risk...</br>

The present project created a simple, reliable, cost-effective, secure and fully controlled by the owner: <b>Smart-Solar-Surplus-Switch</b>.</br>

## Homewizard WiFi P1 meter API </br>
Homewizard should be praised for their openness with respect to the well documentated access to the P1 Meter!</br>

[Get started with the Homewizard API](https://homewizard-energy-api.readthedocs.io/index.html)</br>

# 1 ESP32S3-T-display
## 1.1 Electronic Components </br>
### LilyGo ESP32S3 T-display<br>
<img src="https://github.com/Berg0162/s3-switch/blob/main/images/T-DISPLAY-S3.jpg" align="left" width="500" height="500" alt="S3-Switch">
LilyGo T-Display-S3 is an ESP32-S3 development board. It is equipped with a color 1.9" LCD screen (170*320) and two programmable buttons. Communication with the display is using an I8080 interface. Its overall size has the same layout as the T-Display. The ESP32S3 allows for USB communication and can be programmed in the Arduino Integrated Development Environment (IDE).<br>
See for specifications and use: [LilyGo ESP32S3 T-display](https://github.com/Xinyuan-LilyGO/T-Display-S3)
<br clear="left">

### 5V Relay 1-Channel High-active or Low-active</br>
<img src="https://github.com/Berg0162/s3-switch/blob/main/images/relay high low active 1 channel-600x600h.jpg" align="left" width="200" height="200" alt="S3-Switch">

Specifications:<br>
- Supply voltage: 5V DC
- Signal voltage: 3.3-5V
- Maximum voltage through relay: 250V AC or 110V DC
- Resistive load: 10A (125V AC), 7A (240V AC) or 7A (28V DC)
- Inductive load: 3A (120V AC) or 3A (28V DC)
- Status led shows relay switched ON or OFF
  
Pinout:<br>
- DC+: 5V supply voltage
- DC-: Ground/GND
- IN: Signal pin
- NO: Relay normally open
- COM: Relay common
- NC: Relay normally closed
<br clear="left">

### PCB Power supply</br>
<img src="https://github.com/Berg0162/s3-switch/blob/main/images/hi-link-pcb-power-supply-5vdc-1a-hlk-5m05-front-side-600x600.jpg" align="left" width="200" height="200" alt="S3-Switch"> 

Specifications:<br>
- Input voltage (AC pins): 100 - 240V AC (recommended), 90 - 264V AC (maximum)
- Output voltage (+Vo and -Vo pins): 5V DC
- Maximum output current: 1000mA (continuous)
- Voltage control:  ±0.2%
- Load regulation:  ±0.5%
- Exit ripple: <70mV
<br clear="left">

## 1.2 Circuitry and physical setup</br>
<img src="https://github.com/Berg0162/s3-switch/blob/main/images/Circuitry.jpg" width="820" height="461" ALIGN="left" alt="S3-Switch">
<br clear="left">

>[!WARNING] 
>Some components are connected to 220 Volt AC mains, you really need to know what you are doing since this can be potentially dangerous!

<img src="https://github.com/Berg0162/s3-switch/blob/main/images/113236.jpg" width="620" height="440" ALIGN="left" alt="S3-Switch"></br>
<br clear="left">

<img src="https://github.com/Berg0162/s3-switch/blob/main/images/093314.jpg" width="492" height="536" ALIGN="left" alt="S3-Switch"></br>
<br clear="left">

## 1.3 Functionality
<img src="https://github.com/Berg0162/s3-switch/blob/main/images/093440.jpg" width="315" height="453" ALIGN="left" alt="S3-Switch"><br>
- Connects to your local WiFi network (you have to supply SSID and its Passphrase)
- Will autodetect the Homewizard P1 Meter on the same network and connects to it. Every 5 seconds it will poll for new info.
- Displays actual time retrieved from Internet
- Displays Switch Status in addition to the Led status of the Relay board
- Displays Net Power in digits and gauge presentation (surplus: green CCW and consumption: brown CW)
- Switches the relay ON when Net Power reaches more than or equal -500 kW surplus (value of your choice)
- Allows for 4 fixed clock switch moments (and duration) independent of Net Power level, visible in the display (red blocks)
- Displays time intervals when the smart switch was activated (green ribbon)
- Has a builtin simple webserver for local access to show status and fixed clock switch moments and duration
- Clock switch moments and duration can be edited remotely by pointing a browser to the indicated local IP address

# 2 Sonoff ESP8266
## 2.1 Electronic Components </br>
### Sonoff Basic R2
<img src="https://github.com/Berg0162/s3-switch/blob/main/images/094905.jpg" width="416" height="554" ALIGN="left" alt="S3-Switch"></br>
Specifications:<br>

- Voltage range: 90-250V AC (50/60Hz)
- Max current: 10A
- Max. Power: 2200W
- WiFi chip: ESP8266 or ESP8285
- Dimensions: 91\*43\*25mm (l\*b\*h)
- Wireless standard: 802.11 b/g/n
- Security mechanism: WPA-PSK/WPA2-PSK
- Working temperature: 0ºC-40ºC

See the following page for more information about this product: [Sonoff Basic R2 - WiFi Switch - ESP8266/ESP8285](https://sonoff.tech/product/diy-smart-switches/basicr2/)
<br clear="left">
### CP2102 USB UART Board<br>

The CP2102 USB UART Board (type A) is an accessory board that features the single-chip USB to UART bridge CP2102 onboard.
The board used in the project is from manufacturer: [Waveshare](https://www.waveshare.com/CP2102-USB-UART-Board-type-A.htm)

<img src="https://github.com/Berg0162/s3-switch/blob/main/images/FTDI.jpg" width="560" height="560" ALIGN="left" alt="S3-Switch">
CP2102 features:<br>

- Single-Chip USB to UART Data Transfer
- No external resistors required, no external crystal required
- On-chip power-on reset circuit and voltage regulator
- Integrated 1024-Byte EEPROM

Virtual COM Port Device Drivers<br>
- Windows 8/7/Vista/Server 2003/XP/2000
- MAC OS-X/OS-9
- Linux 2.40 or higher
- USBXpress Direct Driver Support
- Windows 7/Vista/Server 2003/XP/2000
- Windows CE
<br clear="left">

Features<br>
- Supports Mac, Linux, Android, WinCE, Windows 7/8/8.1/10/11...
- Voltage output support: 5V or 3.3V
- Integrated USB protection device: SP0503
- 3 LEDs: TXD LED, RXD LED, POWER LED
- Pins accessible on pinheaders: TXD, RXD, RTS, CTS

## 2.2 Boot your Sonoff in Flashing Mode<br>
The Internet is crowded with instructions on how to flash your code to a Sonoff device. One of the most detailed instructions you can find is 
on [Random Nerd Tutorials](https://randomnerdtutorials.com/how-to-flash-a-custom-firmware-to-sonoff/). Study their tutorial to get acquainted with the technique!

<img src="https://github.com/Berg0162/s3-switch/blob/main/images/095604.jpg" width="356" height="473" ALIGN="left" alt="S3-Switch">
<img src="https://github.com/Berg0162/s3-switch/blob/main/images/095604_detail.jpg" width="526" height="423" ALIGN="left" alt="S3-Switch">
