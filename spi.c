
#include<stdint.h>
#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<getopt.h>
#include<fcntl.h>
#include<sys/ioctl.h>
#include<linux/types.h>
#include<linux/spi/spidev.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<inttypes.h>
#include"spi.h"

int prepare(int fd){
    if(ioctl(fd, SPI_IOC_WR_MODE, &MODE) == -1){
        perror("Can't set MODE");
        return -1;
    }

    if(ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &BITS) == -1){
        perror("Can't set number of BITS");
        return -1;
    }

    if(ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &CLOCK) == -1){
        perror("Can't set write CLOCK");
        return -1;
    }

    if(ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &CLOCK) == -1){
        perror("Can't set read CLOCK");
        return -1;
    }

    return 0;
}

uint8_t control_bits_differential(uint8_t channel){
    return (channel & 7) << 4;
}

uint8_t control_bits(uint8_t channel){
    return 0x8 | control_bits_differential(channel);
}

int readadc(int fd, uint8_t channel){
    uint8_t tx[] = {1, control_bits(channel), 0};
    uint8_t rx[3];

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = ARRAY_SIZE(tx),
        .delay_usecs = DELAY,
        .speed_hz = CLOCK,
        .bits_per_word = BITS,
    };

    if(ioctl(fd, SPI_IOC_MESSAGE(1), &tr) == 1){
        perror("IO Error");
        abort();
    }
    return ((rx[1] << 8) & 0x300) | (rx[2] & 0xFF);
}
