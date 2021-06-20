# Xiaomi-Scooter-Motion-Control
A small hardware and software modification to legalise the electric scooters in The Netherlands.

# Supported scooter models
- XIAOMI Mi Electric Scooter Essential
- XIAOMI Mi Electric Scooter 1S EU
- XIAOMI Mi Electric Scooter M365 Pro
- XIAOMI Mi Electric Scooter Pro 2

# Disclaimer
THIS SCRIPT, INSTRUCTIONS, INFORMATION AND OTHER SERVICES ARE PROVIDED BY THE DEVELOPER ON AN "AS IS" AND "AS AVAILLABLE" BASIS, UNLESS OTHERWISE SPECIFIED IN WRITING. THE DEVELOPER DOES NOT MAKE ANY REPRESENTATIONS OR WARRANTIES OF ANY KIND, EXPRESS OR IMPLIED, AS TO THIS SCRIPT, INSTRUCTIONS, INFORMATION AND OTHER SERVICES. YOU EXPRESSLY AGREE THAT YOUR USE OF THIS SCRIPT IS AT YOUR OWN RISK. 

TO THE FULL EXTEND PERMISSABLE BY LAW, THE DEVELOPER DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED. TO THE FULL EXTEND PERMISSABLE BY LAW, THE DEVELOPER WILL NOT BE LIABLE FOR ANY DAMAGES OF ANY KIND ARISING FROM THE USE OF THIS SCRIPT, INSTRUCTIONS, INFORMATION AND OTHER SERVICES, INCLUDING, BUT NOT LIMITED TO DIRECT, INDIRECT, INCIDENTAL, PUNITIVE AND CONSEQUENTIAL DAMAGES, UNLESS OTHERWISE SPECIFIED IN WRITING.

# How it works
An Arduino Nano will be used to read out the serial-bus of the Scooter. The speedometer will be monitored if there are any kicks with your feed. When there is a kick, the throttle will be opened for 5 seconds (quadracically decreasing). After this time, the scooter will be accepting a new kick.

# Modifications
## Hardware
We reccomend to purchase the following part at [the closest __local__ electronics store](https://www.google.com/maps/search/elektronica+arduino/).

### Shopping list
* Arduino Nano (without headers)
* 1k resistor (use an metal film type)
* 0.47uF capacitor (use an elco)
* JST-ZH male-plug (or cut it from the trottle)
* 10cm male-female prototyping cable (various colors)
* USB A to Mini USB cable for programming and debugging
* Optional: [A sticker for the rear bumper](https://www.legaalsteppen.nl/)

### Wiring it up
![Wiring Scheme](Arduino_Wiring_Scheme_v1.0.png?raw=true "Wiring Scheme")

## Software
### Upload script to Arduino
Using the latest Arduino IDE, upload the .ino file to your programming board. Remove the stock throttle and place the Arduino Nano under the original dashboard.
### Flash custom firmware to Dashboard
Using one of the following apps, flash your dashboard to custom firmware that meets the local regulations:
* Android (paid): [Xiaoflasher](https://play.google.com/store/apps/details?id=eScooter.m365Info)
* Android (free): [ScooterHacking Utility](https://play.google.com/store/apps/details?id=sh.cfw.utility)
* iOS (free, create firmware manually): [Scooter Companion](https://testflight.apple.com/join/RaFiBTgi) 

Reccommended params:
* Max speed: 25/20/15 (S/D/ECO, for LEGAL purposes)
* Draw: 18/16/14 (S/D/ECO, for LEGAL purposes)
* Brake light flash freqency: 0 (for LEGAL purposes)
* No KERS (also called COAST MODE+ANTI CLONK, to disable braking at 0% throttle)
* No overspeed alarm (theoretically possible at steep slopes)

## Done!

# Legal aspects
This modification is created for electric scooters in The Netherlands to comply with REGULATION (EU) No 168/2013 OF THE EUROPEAN PARLIAMENT AND OF THE COUNCIL on the approval and market surveillance of two- or three-wheel vehicles and quadricycles. In this regulation 'pedal cycles with with pedal assistance which are equipped with an auxiliary electric motor having a maximum continuous rated power of less than or equal to 250 W, where the output of the motor is cut off when the cyclist stops pedalling and is otherwise progressively reduced and finally cut off before the vehicle speed reaches 25 km/h' are excepted from type approval and the associated laws.

The Dutch Ministry of Infrastructure and Water Management has confirmed that a two-wheel vehicle, which is intended to be muscle-powered and clearly belongs on the bicycle path, should be seen as a pedal cycle. By the nature of the assistance these electic scooters are categorized as 'pedal cycles with with pedal assistance' and do not need type-approval. These vehicles are allowed to drive up to 25 km/h on public roads.

The code below is intended to comply with above regulations and should be used in combination with modified firmware for the electric scooter. This firmware must contain at least the following adaptations:
* Electric power reduced to 250 W (18A)
* Maximum speed 25 km/h
* No flashing tail light (to comply with NL RVV 1990 article 35a)

Sources:
* Regulation (EU) No 168/2013 of the European Parliament and of the Council of 15 January 2013 on the approval and market surveillance of two- or three-wheel vehicles and quadricycles Text with EEA relevance
* Artikel 35a Reglement verkeersregels en verkeerstekens 1990 (RVV 1990)

# Confirmation of legality
"(...) However, we take the view that a two-wheeled vehicle, which is propelled by its own muscle power and which clearly belongs on the bicycle path, should fall into the category of bicycle. (...) Due to the nature of the support, these scooters therefore also fall into the category of 'bicycle with pedal assistance' and do not have to be admitted separately as special mopeds. You are allowed to drive on public roads at a maximum speed of 25 km/h."

THE MINISTER OF INFRASTRUCTURE AND WATER MANAGEMENT,

On their behalf,

Head of the Road Safety and Road Transport Department

drs. M.N.E.J.G. Philippens
