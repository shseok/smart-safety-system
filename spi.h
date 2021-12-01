
#ifndef __SPI_H__
#define __SPI_H__


#define VALUE_MAX   256
#define BUFFER_MAX 3
#define DIRECTION_MAX 45

#define ARRAY_SIZE(array) sizeof(array) / sizeof(array[0])

const char *DEVICE = "/dev/spidev0.0";
uint8_t MODE = SPI_MODE_0;
uint8_t BITS = 8;
uint32_t CLOCK = 1000000;
uint8_t DELAY = 5;

int prepare(int fd);
uint8_t control_bits_differential(uint8_t channel);
uint8_t control_bits(uint8_t channel);
int readadc(int fd, uint8_t channel);

#endif
