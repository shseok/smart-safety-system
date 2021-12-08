

detect : danger_detect.o pwm.o gpio.o 
	gcc -o detect danger_detect.o pwm.o gpio.o -lpthread

gpio.o : gpio.c
	gcc -c gpio.c

pwm.o : pwm.c
	gcc -c pwm.c

danger_detect.o : danger_detect.c
	gcc -c danger_detect.c

