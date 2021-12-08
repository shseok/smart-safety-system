#ifndef __PWM_H__
#define __PWM_H__


#define VALUE_MAX   256
#define BUFFER_MAX 3
#define DIRECTION_MAX 45

int PWMExport(int pwmnum);
int PWMEnable(int pwmnum);
int PWMWritePeriod(int pwmnum, int value);
int PWMWriteDutyCycle(int pwmnum, int value);

#endif
