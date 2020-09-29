sysfs_gpio
==========

A realtime HAL module for accessing GPIO pins using Sysfs. The is comparable to hal_bb_gpio, but
accesses the GPIO state using the /sys/class/gpio interface rather than directly mapping to 
/dev/mem. This likely means that it is slower that hal_bb_gpio. Sysfs is also deprecated, so
long term this may not be a reliable way to access the pins, but it does seem to be widely 
supported, currently.

## Usage

    sudo FLAVOR=rt-preempt instcomp --install sysfs_gpio.c

    realtime start

    halcmd newthread servo-thread fp 1000000

    # Beaglebone AI
    # Use comma-separated list of 8xx and 9xx values to create HAL pins for those pins
    echo 106 > /sys/class/gpio/export # pin 819
    echo 242 > /sys/class/gpio/export # pin 817
    echo out > /sys/class/gpio/gpio242/direction

    halcmd loadrt sysfs_gpio board=BBAI input_pins=819 output_pins=817
    halcmd addf sysfs_gpio.read servo-thread
    halcmd addf sysfs_gpio.write servo-thread
    halcmd start

    halcmd getp sysfs_gpio.p819-in # get the value of pin 819
    halcmd setp sysfs_gpio.p817-out 1 # set the value of pin 817 to high
   

    # Beaglebone Black
    # Use comma-separated list of 8xx and 9xx values to create HAL pins for those pins
    echo 22 > /sys/class/gpio/export # pin 819
    echo 27 > /sys/class/gpio/export # pin 817
    echo out > /sys/class/gpio/gpio27/direction
    halcmd loadrt sysfs_gpio board=BBB input_pins=819 output_pins=817
    halcmd addf sysfs_gpio.read servo-thread
    halcmd addf sysfs_gpio.write servo-thread
    halcmd start
    halcmd getp sysfs_gpio.p819-in # get the value of pin 819
    halcmd setp sysfs_gpio.p817-out 1 # set the value of pin 817 to high

    # Other board (untested, but potentially Raspberry Pi)
    # rather than 8xx and 9xx, the GPIO numbers can be used directly
    echo 27 > /sys/class/gpio/export # pin 13 on Raspberry Pi
    echo 23 > /sys/class/gpio/export # pin 16 on Raspberry Pi
    echo out > /sys/class/gpio/gpio23/direction
    halcmd loadrt sysfs_gpio board=OTHER input_pins=27 output_pins=23
    halcmd addf sysfs_gpio.read servo-thread
    halcmd addf sysfs_gpio.write servo-thread
    halcmd start
    halcmd getp sysfs_gpio.p27-in # get the value of GPIO 27
    halcmd setp sysfs_gpio.p23-out 1 # set the value of GPIO 23 to high
