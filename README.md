#  Electric scooter motion control adaptation
A small hardware and software modification to legalize electric  Segway-Ninebot ES/MAX/Xiaomi M365-series scooters in The Netherlands.

# Models
- Xiaomi Mi Essential (not tested)
- Xiaomi Mi 1S EU (not tested)
- Xiaomi Mi M365 Pro (not tested)
- Xiaomi Mi (M365) Pro 2 (tested)
- Segway-Ninebot Max and ES series (BETA)

# Disclaimer
THIS SCRIPT, INSTRUCTIONS, INFORMATION AND OTHER SERVICES ARE PROVIDED BY THE DEVELOPER ON AN "AS IS" AND "AS AVAILABLE" BASIS, UNLESS OTHERWISE SPECIFIED IN WRITING. THE DEVELOPER DOES NOT MAKE ANY REPRESENTATIONS OR WARRANTIES OF ANY KIND, EXPRESS OR IMPLIED, AS TO THIS SCRIPT, INSTRUCTIONS, INFORMATION AND OTHER SERVICES. YOU EXPRESSLY AGREE THAT YOUR USE OF THIS SCRIPT IS AT YOUR OWN RISK. 

TO THE FULL EXTEND PERMISSIBLE BY LAW, THE DEVELOPER DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED. TO THE FULL EXTEND PERMISSIBLE BY LAW, THE DEVELOPER WILL NOT BE LIABLE FOR ANY DAMAGES OF ANY KIND ARISING FROM THE USE OF THIS SCRIPT, INSTRUCTIONS, INFORMATION AND OTHER SERVICES, INCLUDING, BUT NOT LIMITED TO DIRECT, INDIRECT, INCIDENTAL, PUNITIVE AND CONSEQUENTIAL DAMAGES, UNLESS OTHERWISE SPECIFIED IN WRITING.

# How it works
An Arduino Nano will be used to read out the serial-bus of the scooter. The speedometer will be monitored if there are any kicks with your feed. When there is a kick, the throttle will be opened for 1-5 seconds (quadratically decreasing in level). The scooter will be accepting a new kick about 300 milliseconds after throttling. The recommended driving style for good kick detection and low power consumption is: kick-boost(5s)-wait(5s)-kick-boost(5s)-wait(5s)-kick-boost(5s)-etcetera. The range of your scooter is not wildly affected by this modification.

## Formula
The (simplified) formula used for calculating the throttle level (in full percentages of throttle) is: 
### [y=s-s*s^(0.5x-0.5t)](https://www.desmos.com/calculator/1qknrc1jrk)
* y = throttle
* x = time elapsed
* t = duration of boost
* s = speed (normally multiplied by 5, but actual value depends on configuration)

# Modifications
## Hardware
We recommend to purchase the following part at [the closest __local__ electronics store](https://www.google.com/maps/search/elektronica+arduino/).

### Shopping list
* Arduino Nano (without headers is recommended)
* 1k resistor (metal film type is recommended)
* 0.47uF capacitor (electrolytic type is recommended)
* JST-ZH 1.5mm male plug (or cut it from the throttle)
* 10cm male-female prototyping cable (black, red, green and yellow)
* USB A to Mini USB cable
* A small cable-tie
* Optional: [A sticker for the rear bumper](https://www.legaalsteppen.nl/)
* Recommended: various sizes of heat shrink sleeves and hot melt glue

### Tools needed
* Small Phillips screw driver
* Medium Phillips screw driver
* 3mm Allen wrench
* Wire cutter

### Disassembly ([photo's can be found here](https://www.scootersdirect.co.uk/blogs/news/xiaomi-m365-pro-scooter-teardown-inside))
ENSURE THE SCOOTER IS POWERED OFF, DO NOT POWER IT ON UNLESS REFITTED
1. Remove the right hand handlebar grip by sliding it of the steering bar.
2. Remove the top dash cover by prying it up, it is hold together by double sided adhesive.
3. Remove the lower/main dashboard cover by prying it up, it is hold together by double sided adhesive.
4. Remove the screw on top and below the headlight and slide the headlight out of its socket.
5. Remove the dashboard from the steering column by unscrewing the three small screws holding it in place.  
6. Cut the cable-tie from the rubber thimble, unplug the three small connectors inside and unplug the main dashboard connector. 
7. Remove the throttle by removing the rubber protector, loosening the allen srew, feeding the cable trough the headlight socked and sliding the throttle off.

### Wiring
![Wiring Scheme](Arduino_Wiring_Scheme_v1.0.png?raw=true "Wiring Scheme")

Finish up wrapping the board in heat shrink tube and cover all remaining open connections in hot melt glue. 

### Reassembly
Follow steps 1-6 of the disassembly in reverse order. Make sure you mount the board tightly in the steering frame to prevent it from rattling. Program the board and flash your scooter using the steps below.

# Known issues
- __Accelerating in corners__: The wheel width on the tire wall is smaller than the tire center. Because the scooter speed is measured by the amount of wheel rotations, cornering with your scooter will result in an incorrect (increasing) speed measurement. This is a design flaw of the scooter and cannot be solved in this script. It is recommended to hold the brake while cornering sharply to prevent these accelerations.
- __Accelerating on downhill slopes and after speed bumps__: This is as expected as this will result in an increased speed measurement that cannot be differentiated from a kick.

## Software
### Upload script to Arduino
Using the latest Arduino IDE, upload the .ino file to your Arduino Nano programming board. 

### Limit speed, motor power (only required for >250W motors) and disable brake light flash
#### Flash custom firmware
Using one of the following apps, flash your dashboard to custom firmware that meets the local regulations:
* Android (paid): [Xiaoflasher](https://play.google.com/store/apps/details?id=eScooter.m365Info)
* Android (free): [ScooterHacking Utility](https://play.google.com/store/apps/details?id=sh.cfw.utility)
* iOS (free, create firmware manually): [Scooter Companion](https://testflight.apple.com/join/RaFiBTgi) 

Recommended firmware params:
* Max speed: 25kmh/20kmh/15kmh (S/D/ECO, max 25 km/h for legal purposes)
* Draw: 18A/16A/14A (S/D/ECO, with 350W motor use max 18A for legal purposes) (set )
* Disable motor below 5 km/h
* Brake light flash frequency: 0 (no flashing for legal purposes)
* No KERS (also called COAST MODE+ANTI CLONK, to disable braking at 0% throttle)
* No overspeed alarm (theoretically possible at steep slopes)

## Done!

## Support
We have a Telegram group for legal electric scooter enthusiasts, join us on:
https://t.me/joinchat/IuIjHecjckhK1h-a

# Legal aspects in The Netherlands
## Relevant regulations and required modifications
This modification is created for electric scooters in The Netherlands to comply with REGULATION (EU) No 168/2013 OF THE EUROPEAN PARLIAMENT AND OF THE COUNCIL on the approval and market surveillance of two- or three-wheel vehicles and quadricycles. In this regulation 'pedal cycles with with pedal assistance which are equipped with an auxiliary electric motor having a maximum continuous rated power of less than or equal to 250 W, where the output of the motor is cut off when the cyclist stops pedaling and is otherwise progressively reduced and finally cut off before the vehicle speed reaches 25 km/h' are excepted from type approval and the associated laws.

The script is intended to comply with above regulations and should be used in combination with modified software and hardware for the electric scooter. A road legal scooter must contain at least the following adaptations:
* No throttle lever
* Electric power reduced to 250 W (18A)
* Maximum speed 25 km/h
* No flashing tail light (to comply with NL RVV 1990 article 35a)

Sources:
* [Regulation (EU) No 168/2013 of the European Parliament and of the Council of 15 January 2013 on the approval and market surveillance of two- or three-wheel vehicles and quadricycles Text with EEA relevance](https://eur-lex.europa.eu/legal-content/EN/TXT/PDF/?uri=CELEX:32013R0168)
* [Artikel 35a Reglement verkeersregels en verkeerstekens 1990 (RVV 1990)](https://wetten.overheid.nl/jci1.3:c:BWBR0004825&hoofdstuk=II&paragraaf=13&artikel=35a)

## Insurance and/or liability
Because this scooter does not need type approval and/or registration, you do not need a compulsory third party liability insurance for vehicles. You are insured under the conditions of a Public Liability Insurance if any damage is caused by your electric scooter to third parties. We recommend to get this insurance when driving on public roads.

## Confirmation of legality
"(...) However, we take the view that a two-wheeled vehicle, which is propelled by muscle power and which clearly belongs on the bicycle path, should fall into the category of bicycle. (...) Due to the nature of the support, these scooters therefore also fall into the category of 'bicycle with pedal assistance' and do not have to be admitted separately as special mopeds. You are allowed to drive on public roads at a maximum speed of 25 km/h."

THE MINISTER OF INFRASTRUCTURE AND WATER MANAGEMENT,

On their behalf,

Head of the Road Safety and Road Transport Department

drs. M.N.E.J.G. Philippens
