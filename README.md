# S3-Switch &nbsp;&nbsp;Smart-Solar-Surplus-Switch
In Europe many households are equiped with modern meters (a.k.a. smart meters) for billing the energy consumption without human interference. These smart meters send readings to the energy supplier automatically but they also have a P1 port that gives you access to all measured energy data.
The present project uses a commercial device to read constantly this P1 port: [Homewizard P1 Meter](https://www.homewizard.com/p1-meter/). This P1 Meter is compatible with most modern meters in Europe.</br>

It becomes even more interesting when your solar panels are involved in the energy equasion. The Homewizard P1 meter is able to monitor and record the energy management of the household at any moment in time. That includes the solar energy sent from the inverter back into the power grid with revenue grade accuracy. The total energy consumption is the basis of the utility bills from the energy supplier(s) every month. One wants a maximum benefit of the solar energy for good reasons!</br>

The net power is the difference between the power the household is importing from and the power the inverter exports to the utility grid at any moment in time. When the net power is negative the export (solar production) exceeds the import of power and you might want to switch on extra electric appliances (like a battery charger, kitchen boiler or something else) to have maximum benefit of the solar surplus power. A smart switch with exactly that capabibilty would come in very handy! No doubt commercial parties (like [Homewizard](https://www.homewizard.com/energy-plus/)) offer this service but always at a recurring pricing model and a security risk...</br>

## Homewizard WiFi P1 meter API </br>
Homewizard should be praised for their openness with respect to the well documentated access to the P1 Meter!</br>
[Get started with the Homewizard API](https://homewizard-energy-api.readthedocs.io/index.html)</br>

The present project created 2 instances of a <b>Smart-Solar-Surplus-Switch</b> that share most of the software but differ in the selected electronic components:
## 1 LilyGo ESP32S3 T-display
This board and ESP32S3 processor was selected for its excellent specifications and crisp display. Aside of gaining experience with the [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) library, creating a nice visual user interface was a dominant incentive. A lot of inspiration and practical knowledge was obtained by studying on Youtube: [the Volos Projects](https://www.youtube.com/c/VolosProjects). This ended up in a good working and visually attractive S3-Switch but definitively not the most cost-effective or simple solution to build! The solution works in the living space (as a "green" charging point) rather than hidden under kitchen sink!
## 2 Sonoff Basic R2 (ESP8266)
The Sonoff was selected for its very low cost, ready for-the-purpose-package, is meant to be hacked and to upload/flash custom firmware. It ended up as the reliable S3-Switch workhorse that is tucked away aside the fuse box...<br>
## Arduino IDE
The present code is developed on Arduino IDE 2.0. Notice that you will need for each of these processors (<b>ESP32S3</b> and/or <b>ESP8266</b>) an Arduino IDE that is tailored for the specific type:<br>

[Installation instructions for the ESP32S3 T-display](https://github.com/Xinyuan-LilyGO/T-Display-S3)<br>

[Installation instructions for the ESP8266](https://arduino-esp8266.readthedocs.io/en/latest/installing.html)<br>

# 1 ESP32S3-T-display
## 1.1 Electronic Components </br>
### LilyGo ESP32S3 T-display<br>
<img src="https://github.com/Berg0162/s3-switch/blob/main/images/T-DISPLAY-S3.jpg" align="left" width="500" height="500" alt="S3-Switch">
LilyGo T-Display-S3 is an ESP32-S3 development board. It is equipped with a color 1.9" LCD screen (170*320) and two programmable buttons. Communication with the display is using an I8080 interface. Its overall size has the same layout as the T-Display. The ESP32S3 allows for USB communication and can be programmed in the Arduino Integrated Development Environment (IDE).<br>

See for specifications and installation: [LilyGo ESP32S3 T-display](https://github.com/Xinyuan-LilyGO/T-Display-S3)
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
The board used in the project is from manufacturer: [Waveshare](https://www.waveshare.com/CP2102-USB-UART-Board-type-A.htm)<br>
This board is only used to flash/upload new firmware to the Sonoff. It can afterwards be disconnected and will only serve again when you need to update the firmware.

<img src="https://github.com/Berg0162/s3-switch/blob/main/images/FTDI.jpg" width="259" height="471" ALIGN="left" alt="S3-Switch">
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
The Internet is crowded with instructions on how to flash your code to a Sonoff device. [Search now.](https://www.google.com/search?q=flash+sonoff) One of the most detailed instructions you can find is 
on [Random Nerd Tutorials](https://randomnerdtutorials.com/how-to-flash-a-custom-firmware-to-sonoff/). Study their tutorial to get acquainted with the technique!

<img src="https://github.com/Berg0162/s3-switch/blob/main/images/095604.jpg" width="356" height="473" ALIGN="left" alt="S3-Switch">
<img src="https://github.com/Berg0162/s3-switch/blob/main/images/095604_detail.jpg" width="426" height="323" ALIGN="left" alt="S3-Switch">
<br clear="left">

## 2.3 Entering the SSID and Pasword of your local WiFi network
The <b>Sonoff S3-Switch</b> supports Access Point mode (at startup) that allows you to use the S3-Switch to create a temporary WiFi network to connect. This is similar to WiFi connection sharing available on phones (a.k.a. hotspot). As with phones, the operation of a WiFi router is simulated: this is known as a Soft AP (for “software” WiFi access point). With Access Point mode one creates a private WiFi local area network wholly isolated from others.
At startup the S3-Switch checks for a <b>valid</b> SSID and Passphrase to connect to your local WiFi network. If this check fails the S3-Switch starts Access Point mode to allow you to enter the credentials of the WiFi router of your choice and allows the ESP32 to connect to the local WiFi network. Most connected objects use this principle to connect to the home WiFi.
After the valid SSID and Passphrase is checked this information is stored persistently and the S3-Switch will use these data, the next time it is powered or reset. Entering the SSID info is a one-time user action! 
If the S3-Switch is in AP mode:
- On your phone/tablet connect to the open WiFi network with name: <b>ESP32-AP</b> (password: 12345678) 
- Point your browser to the fixed IP Address: <b>192.168.4.1</b>
- Your browser will show a form
- Enter the SSID and Password. Press Submit button..
- Receipt will be confirmed!
If all goes well, the S3-Switch now (always) connects to the local WiFi network of your choice!

## 2.4 Sonoff S3-Switch remote access using the browser<br>
The Sonoff S3-Switch can remotely be accessed by pointing the browser (on your desktop, tablet or smartphone) to a fixed host IP address: <b>192.168.2.200</b> or to: <b>esp8266.local</b>. The simple builtin web server will respond with a start page, that helps you select the different options.<br>

<img src="https://github.com/Berg0162/s3-switch/blob/main/images/Sonoff_01.jpg" width="350" height="380" ALIGN="left" alt="S3-Switch">
<img src="https://github.com/Berg0162/s3-switch/blob/main/images/Sonoff_02.jpg" width="350" height="380" ALIGN="left" alt="S3-Switch">
<img src="https://github.com/Berg0162/s3-switch/blob/main/images/Sonoff_03.jpg" width="350" height="380" ALIGN="left" alt="S3-Switch">
<img src="https://github.com/Berg0162/s3-switch/blob/main/images/Sonoff_04.jpg" width="350" height="380" ALIGN="left" alt="S3-Switch">

