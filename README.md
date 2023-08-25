# S3-Switch &nbsp;&nbsp;Smart-Solar-Surplus-Switch
In Europe many households are equiped with modern meters (a.k.a. smart meters) for billing the energy consumption without human interference. These smart meters send readings to the energy supplier automatically but they also have a P1 port that gives you access to all measured energy data.
The present project uses a commercial device to read constantly this P1 port: [Homewizard P1 Meter](https://www.homewizard.com/p1-meter/). This P1 Meter is compatible with most modern meters in Europe.</br>

It becomes even more interesting when your solar panels are involved in the energy equasion. The Homewizard P1 meter is able to monitor and record the energy management of the household at any moment in time. That includes the solar energy sent from the inverter back into the power grid with revenue grade accuracy. The total energy consumption is the basis of the utility bills from the energy supplier(s) every month. One wants a maximum benefit of the solar energy for good reasons!</br>

The net power is the difference between the power the household is importing from and the power the inverter exports to the utility grid at any moment in time. When the net power is negative the export (solar production) exceeds the import of power and you might want to switch on extra electric appliances (like a battery charger, kitchen boiler or something else) to have maximum benefit of the solar surplus power. A smart switch with exactly that capabibilty would come in very handy! No doubt commercial parties (like [Homewizard](https://www.homewizard.com/energy-plus/)) offer this service but always at a recurring pricing model and a security risk...</br>

The present project created a simple, reliable, cost-effective, secure and fully controlled by the owner: <b>Smart-Solar-Surplus-Switch</b>.</br>

# Homewizard WiFi P1 meter API </br>
Homewizard should be praised for their openness with respect to the well documentated access to the P1 Meter!</br>

[Get started with the Homewizard API](https://homewizard-energy-api.readthedocs.io/index.html)</br>

# Electronic Components </br>
[LilyGo ESP32S3 T-display](https://github.com/Xinyuan-LilyGO/T-Display-S3)</br>
<img src="https://github.com/Berg0162/s3-switch/blob/main/images/T-DISPLAY-S3.jpg" width="500" height="500" alt="S3-Switch"></br>
<b>5V Relay 1-Channel High-active or Low-active</b></br>
<img src="https://github.com/Berg0162/s3-switch/blob/main/images/relay high low active 1 channel-600x600h.jpg" align="left" width="200" height="200" alt="S3-Switch">
Specifications:
- Supply voltage: 5V DC
- Signal voltage: 3.3-5V
- Maximum voltage through relay: 250V AC or 110V DC
- Resistive load: 10A (125V AC), 7A (240V AC) or 7A (28V DC)
- Inductive load: 3A (120V AC) or 3A (28V DC)
- 
  </br>
  
Pinout:
- DC+: 5V supply voltage
- DC-: Ground/GND
- IN: Signal pin
- NO: Relay normally open
- COM: Relay common
- NC: Relay normally closed
<br clear="left">

<b>PCB Power supply</b></br>
<img src="https://github.com/Berg0162/s3-switch/blob/main/images/hi-link-pcb-power-supply-5vdc-1a-hlk-5m05-front-side-600x600.jpg" align="left" width="200" height="200" alt="S3-Switch"> 
Specifications:
- Input voltage (AC pins): 100 - 240V AC (recommended), 90 - 264V AC (maximum)
- Output voltage (+Vo and -Vo pins): 5V DC
- Maximum output current: 1000mA (continuous)
- Voltage control:  ±0.2%
- Load regulation:  ±0.5%
- Exit ripple: <70mV
<br clear="left">

# Circuitry and physical setup</br>
<img src="https://github.com/Berg0162/s3-switch/blob/main/images/Circuitry.jpg" width="820" height="461" ALIGN="left" alt="S3-Switch">
<br clear="left">

> [!WARNING] 
> The components uses a high alternating voltage which can be potentially dangerous!

<img src="https://github.com/Berg0162/s3-switch/blob/main/images/113236.jpg" width="620" height="440" ALIGN="left" alt="S3-Switch"></br>
<br clear="left">

<img src="https://github.com/Berg0162/s3-switch/blob/main/images/093314.jpg" width="492" height="536" ALIGN="left" alt="S3-Switch"></br>
<br clear="left">

# TBD
<img src="https://github.com/Berg0162/s3-switch/blob/main/images/093440.jpg" width="315" height="453" ALIGN="left" alt="S3-Switch"></br>
