#!/bin/bash
sudo python resetbutton.py &

# wait for A button press (gpio 25) before starting mirror so that
#  we can log in if we need to

# pigs needs pigpiod running..
sudo pigpiod
pigs modes 25 r

while true; do
	if [[ $(pigs r 25) == "0" ]]; then break; fi
	sleep 1
done

# ..but mirror doesn't work if it's running :eyeroll:
sudo killall pigpiod

# finally, loop mirror so it restart on crash/button reset
while true; do sudo /home/pi/mirror; done
