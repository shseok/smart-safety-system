
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
#include<string.h>
#include<pthread.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include"gpio.h"
#include"pwm.h"

#define IN  0
#define OUT 1

#define LOW 0
#define HIGH    1

#define FD 27
#define MD 5

#define RED 17
#define GREEN 22

char danger[4];

int fire = 1;
float gas; //for saving gas ppm.
int motion = 0; //for saving motion detecting
int fi = 0; // for fire index;
int gi = 0; // for gas index;

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
void *fire_detect();
void *gas_detect();
void *motion_detect();



int main(int argc, int argv[]){
    pthread_t fid, gid, mid; //fid is for detecting fire thread, gid fs for detecting gas thread.

    pthread_create(&fid, NULL, fire_detect, NULL);
    pthread_create(&gid, NULL, gas_detect, NULL);
    pthread_create(&mid, NULL, motion_detect, NULL);
    
    int serv1_sock, serv2_sock, allert_sock = -1, control_sock = -1; //serv1 for allert Rpi, serv2 for controller RPi
    struct sockaddr_in serv1_addr, serv2_addr, allert_addr, control_addr;
    socklen_t allert_addr_size, control_addr_size;
    char msg[3]; // danger data to send other Rpi.
    char recv[2]; // receive data from controller Rpi.

    if(argc != 3){
        printf("Usage : %s <port>\n", argv[0]);
    }

    serv1_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv1_sock == -1){
        fprintf(stderr,"Error in socket() for serv1_sock\n");
    }

    memset(&serv1_addr, 0, sizeof(serv1_addr));
    serv1_addr.sin_family = AF_INET;
    serv1_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv1_addr.sin_port = htons(atoi(argv[1]));

    serv2_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv2_sock == -1){
        fprintf(stderr, "Error in socket() for serv2_sock\n");
    }

    memset(&serv2_addr, 0, sizeof(serv2_addr));
    serv2_addr.sin_family = AF_INET;
    serv2_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv2_addr.sin_port = htons(atoi(argv[2]));


    if(bind(serv1_sock, (struct sockaddr*) &serv1_addr, sizeof(serv1_addr))==-1){
        fprintf(stderr, "Error in bind() for serv1");
        exit(0);
    }

    if(bind(serv2_sock, (struct sockaddr*) &serv2_addr, sizeof(serv2_addr))==-1){
        fprintf(stderr, "Error in bind() for serv2");
        exit(0);
    }

    if(listen(serv1_sock, 5) == -1 || listen(serv2_sock, 5)){
        fprintf(stderr, "listen error!\n");
        exit(0);
    }
    //accept other Rpi's request
    if(allert_sock < 0){         
            allert_addr_size = sizeof(allert_addr);
            allert_sock = accept(serv1_sock, (struct sockaddr*)&allert_addr, &allert_addr_size);
            if(allert_sock == -1){
                fprintf(stderr, "Error in allert_sock!\n");
            } 
    }


    if(control_sock < 0){         
            control_addr_size = sizeof(control_addr);
            control_sock = accept(serv2_sock, (struct sockaddr*)&control_addr, &control_addr_size);
            if(control_sock == -1){
                fprintf(stderr, "Error in control_sock!\n");
            } 
    }
    
    danger[3] = '\0';

    while(1){
        if(danger[0] == '1' && danger[1] == '1'){
            printf("fire and gas!\n");
            printf("ppm : %.2f\n\n", gas);
        }
        else if(danger[0] == '0' && danger[1] == '1'){
            printf("gas!\n");
            printf("ppm : %.2f\n\n", gas);
        }
        else if(danger[0] == '1' && danger[1] == '0'){
            printf("fire!\n");
        }
        else{
            printf("safe now!\n");
        }

        // send data through socket programming.
        write(allert_sock, danger, 2);
        write(control_sock, danger, sizeof(danger));
        usleep(500000);
    }
    
    int status;
    pthread_join(fid, (void**)&status);
    pthread_join(gid, (void**)&status);
    pthread_join(mid, (void**)&status);
}


// other functions to maintain the detector Rpi.

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

void *fire_detect(){
    if(-1 == GPIOExport(FD) || -1 == GPIOExport(RED)){
        printf("error in fire export\n");
        exit(0);
    }

    usleep(100000);

    if(-1 == GPIODirection(FD, IN) || -1 == GPIODirection(RED, OUT)){
        printf("error in fire direction\n");
        exit(0);
    }
    fi = 0;
    while(1){
        fire = GPIORead(FD);
        if(!fire && fi < 10){
           fi++;
        }
        else if(!fire && fi >= 10){
            GPIOWrite(RED, HIGH);
            danger[0] = '1';
            //send danger message to other RPi
        }
        else if(fire && fi > 0){
            fi--;
        }
        else{
            GPIOWrite(RED, LOW);
            danger[0] = '0';
        }
        usleep(100000);
    }
    return;
}

void *gas_detect(){
    
    if(GPIOExport(GREEN) == -1){
        printf("gas export error\n");
        exit(0);
    }
    
    if(GPIODirection(GREEN, OUT) == -1){
        printf("gas direction error\n");
        exit(0);
    }
    
    
    int fd = open(DEVICE, O_RDWR);
    if(fd <= 0 ){
        printf("SPI file open error!\n");
        exit(0);
    }

    if(prepare(fd) == -1){
        printf("SPI prepare error!\n");
        exit(0);
    }
    gi = 0;
    while(1){
        gas = (float)readadc(fd, 0);
        gas = (gas / 1024) * 9800 + 200;
        //printf("now GAS PPM : %.2f\n", gas);
        
        if(gas >= 5000 && gi < 10){
            gi++;
        }
        else if(gas >= 5000 && gi >= 10){
            GPIOWrite(GREEN, HIGH);
            danger[1] = '1';
            //send danger message to other RPi
        }
        else if(GREEN < 5000 && gi > 0){
            gi--;
        }
        else{
            GPIOWrite(GREEN, LOW);
            danger[1] = '0';
        }
        usleep(100000);
    }
    return;
}

void *motion_detect(){
    if(-1 == GPIOExport(MD)){
        printf("error in motion export\n");
        exit(0);
    }

    usleep(100000);

    if(-1 == GPIODirection(MD, IN)){
        printf("error in motion direction\n");
        exit(0);
    }
    while(1){
        motion = GPIORead(FD);
        if(!motion){ //nothing detected!!!
			danger[2] = '0';
		}
		else{ //people detected
            danger[2] = '1';
		}
        usleep(100000);
    }
    return;
}
