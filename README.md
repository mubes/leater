leater
======

PID Oven (and other things) Controller for LPC810 series Microcontrollers

These files implement a reasonably complete PID controller for ovens and
various other places. The thing originated from a bit of a combination effect
when my neighbour wanted a heater to keep his laser warm (hence the name Laser
hEATER, or LEATER) and I needed an oven controller to reflow PCBs for my
CNC milling machine project.

Leater uses a LPC812 microcontroller, with the pinout as shown (and easily
modified) in config.h;

              1--PIO0_17        PIO0_14--20 SPI_SEL
    GREEN_LED 2--PIO0_13        PIO0_0---19 UART_RX (ACMP_IN)
              3--PIO0_12        PIO0_6---18 SPI_CLK
        RESET 4--PIO0_5         PIO0_7---17 (ADC_FEEDBACK)
      UART_TX 5--PIO0_4         Vss------16 Gnd
          TCK 6--PIO0_3         Vdd------15 3v3 Out
          TMS 7--PIO0_2         PIO0_8---14 XTALIN
              8--PIO0_11        PIO0_9---13 XTALOUT
              9--PIO0_10        PIO0_1---12 HEATER
             10--PIO0_16        PIO0_15--11 SPI_MISO

The pins are fixed function either because of the chip itself or, in some 
cases, because of the specific board I've been working with.  The beauty 
of the LPC8xx chips is that things are pretty reconfigurable, so you should 
be able to change things around very easily just by changing the defines 
in config.h....it should work fine on a LPCXpresso 812 board, for example.
Indeed, the code demands on the chip are pretty minimal - you don't even
really need a crystal!

Leater can use either a thermocouple or a thermistor arrangement. By default
its set up for a maxim 6675 K-type thermocouple interface but the defines
in config.h allow changeover to thermistor very easily....additional sensors
can also be easily added.

For more information and how to use please go to http://www.marples.net/leater.

The interface is, by default, a 115Kbaud serial connection.  You can easily
program into a LPC812 using flashmagic, lpc21isp or even lpcxpresso depending
on what platform you're using.

This project is open source, and active collaboration is encouraged.

DAVE
