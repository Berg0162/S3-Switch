# S3-Switch &nbsp;&nbsp;Smart-Solar-Surplus-Switch
In Europe many households are equiped with modern meters (a.k.a. smart meters) for billing the energy consumption without human interference. These smart meters send readings to the energy supplier automatically but they also have a P1 port that gives you access to all measured energy data.
The present project uses a commercial device to read constantly this P1 port: [Homewizard P1 Meter](https://www.homewizard.com/p1-meter/). This P1 Meter is compatible with most modern meters in Europe.</br>

It becomes even more interesting when your solar panels are involved in the energy equasion. The Homewizard P1 meter is able to monitor and record the energy management of the household at any moment in time. That includes the solar energy sent from the inverter back into the power grid with revenue grade accuracy. The total energy consumption is the basis of the utility bills from the energy supplier(s) every month. One wants a maximum benefit of the solar energy for good reasons!</br>

The net power is the difference between the power the household is importing from and the power the inverter exports to the utility grid at any moment in time. When the net power is negative the export (solar production) exceeds the import of power and you might want to switch on extra electric appliances (like a battery charger, kitchen boiler or something else) to have maximum benefit of the solar surplus power. A smart switch with exactly that capabibilty would come in very handy! No doubt commercial parties (like [Homewizard](https://www.homewizard.com/energy-plus/)) offer this service but always at a recurring pricing model and a security risk...</br>

The present project created a simple, reliable, cost-effective, secure and fully controlled by the owner: <b>Smart-Solar-Surplus-Switch</b>.</br>

# Homewizard WiFi P1 meter API </br>
Homewizard should be praised for their openness with respect to the well documentated access to the P1 Meter!
[Get started with the Homewizard API](https://homewizard-energy-api.readthedocs.io/index.html)</br>

# Components </br>
[LilyGo ESP32S3 T-display](https://github.com/Xinyuan-LilyGO/T-Display-S3)
[5V Relay 1-Channel High-active or Low-active](https://www.tinytronics.nl/shop/en/switches/relays/5v-relay-1-channel-high-active-or-low-active)</br>
[Hi-Link PCB Power supply](https://www.tinytronics.nl/shop/en/power/power-supplies/5v/hi-link-pcb-power-supply-5vdc-1a-hlk-5m05)

# Circuitry </br>

<img src="https://github.com/Berg0162/s3-switch/blob/main/images/093440.jpg" width="315" height="453" ALIGN="left" alt="S3-Switch">

<img src="https://github.com/Berg0162/s3-switch/blob/main/images/113236.jpg" width="620" height="440" ALIGN="left" alt="S3-Switch">

<img src="https://github.com/Berg0162/s3-switch/blob/main/images/093314.jpg" width="492" height="536" ALIGN="left" alt="S3-Switch">

