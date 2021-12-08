
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <wiringPi.h>
#include <softTone.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <softPwm.h>
//motor
#define SERVO 1
#define MOTOR_PIN 1
//speker
#define BUFFER_MAX 30
#define DIRECTION_MAX 45
//#define VALUE_MAX 40
#define IN 0
#define OUT 1
#define PWM 0

#define LOW 0
#define HIGH 1
#define VALUE_MAX 256

#define POUT 23
#define LED_FIRE_POUT 17
#define LED_GAS_POUT 27
#define LED_BOTH_POUT 22
#define SPEAKER_PIN 28

int str_len_btn, str_len_sensor;
int sensor_worked_speaker = 0;
char msg[2], msg2[1];
int read_btn = 0, prev_motor_state = 0; //0이 잠겨있음 1이 열려있음
int sensor_worked = 0;
int global_sock, global_sock2;

static int GPIOExport(int pin)
{

    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (-1 == fd)
    {
        fprintf(stderr, "Failed to open export for writing!\n");
        return (-1);
    }

    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);
    close(fd);
    return (0);
}

static int GPIOUnexport(int pin)
{
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (-1 == fd)
    {
        fprintf(stderr, "Failed to open unexport for writing!\n");
        return (-1);
    }
    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);
    close(fd);
    return (0);
}

static int GPIODirection(int pin, int dir)
{
    static const char s_directions_str[] = "in\0out";

    char path[DIRECTION_MAX] = "/sys/class/gpio/gpio%d/direction";
    int fd;

    snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);

    fd = open(path, O_WRONLY);
    if (-1 == fd)
    {
        fprintf(stderr, "Failed to open gpio direction for writing!\n");
        return (-1);
    }

    if (-1 == write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3))
    {
        fprintf(stderr, "Failed to set direction!\n");
        return (-1);
    }

    close(fd);
    return (0);
}

static int GPIORead(int pin)
{
    char path[VALUE_MAX];
    char value_str[3];
    int fd;

    snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_RDONLY);
    if (-1 == fd)
    {
        fprintf(stderr, "Failed to open gpio value for reading!\n");
        return (-1);
    }

    if (-1 == read(fd, value_str, 3))
    {
        fprintf(stderr, "Failed to read value!\n");
        return (-1);
    }

    close(fd);

    return (atoi(value_str));
}

static int GPIOWrite(int pin, int value)
{
    static const char s_values_str[] = "01";
    char path[VALUE_MAX];
    int fd;

    snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_WRONLY);
    if (-1 == fd)
    {
        fprintf(stderr, "Failed to open gpio value for writing!\n");
        return (-1);
    }

    if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1))
    {
        fprintf(stderr, "Failed to write value!\n");
        return (-1);

        close(fd);
        return (0);
    }
}
void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
void open_door()
{
    printf("---open door!!(mortor on)---\n");
    softPwmWrite(MOTOR_PIN, 13); // 문열림 현재위치에서 180도로 회전
    delay(300);                 //600ms 동안 softPwmWrite()가 지속
    prev_motor_state = 1;
}
void close_door()
{
    printf("---Lock door!!(motor off)---\n");
    softPwmWrite(MOTOR_PIN, 3); //문닫힘 현재위치에서 180도로 회전
    delay(300);
    prev_motor_state = 0;
}

void *button_thd()
{
    //계속 버튼 입력을 read 함.
    while (1)
    {
        str_len_btn = read(global_sock2, msg2, sizeof(msg2));
        usleep(500000);
        printf("button state: %s\n", msg2);
        if (str_len_btn == -1)
            error_handling("read() error");

        if (strncmp("1", msg2, 1) == 0 && strcmp("00", msg) == 0 && sensor_worked == 1)
        {
            printf("-----------button -> off-------------\n");
            sensor_worked_speaker = 0;
            read_btn = 1;
            sensor_worked = 0;
        }
    }
}

void *danger_thd() //계속 센서 4가지 상황을 read 함.
{

    
    while (1)
    {
        str_len_sensor = read(global_sock, msg, sizeof(msg)); //위험감지파이 신호 read
        printf("received : %s\n", msg);
        usleep(500000);
        if (str_len_sensor == -1)
            error_handling("read() error");

            //--------------------LED 조작---------------
        if (strcmp("10", msg) == 0 || strcmp("01", msg) == 0 || strcmp("11", msg) == 0)
        {
            sensor_worked = 1;

            if (strcmp("10", msg) == 0)
            { //불꽃 감지
                printf("fire!!\n");
                if (-1 == GPIOWrite(LED_FIRE_POUT, 1))
                {
                    error_handling("write() error");
                }
            }
            else if (strcmp("01", msg) == 0)
            { //가스 감지
                printf("gas!!\n");
                if (-1 == GPIOWrite(LED_GAS_POUT, 1))
                {
                    error_handling("write() error");
                }
            }
            else if (strcmp("11", msg) == 0)
            { //둘다 감지
                printf("fire&gas!!\n");
                if (-1 == GPIOWrite(LED_BOTH_POUT, 1))
                {
                    error_handling("write() error");
                }
            }
        }
    }
}
void *act_thd()
{
    int i=0;
    softPwmWrite(MOTOR_PIN, 3);
    while (1)
    {
        
        //--------------------SPEAKER 조작---------------
        //--------------------MOTOR 조작---------------

        //motor on
        // if (prev_motor_state == 0 && sensor_worked == 1)
        // { //문이 0이라면(잠겨있다면)
        //     open_door();
        // }


        // if ((strcmp("10", msg) == 0 || strcmp("01", msg) == 0 || strcmp("11", msg) == 0))
        // {
        //     sensor_worked_speaker = 1;
        // }

        if (sensor_worked==1)
        {
            if(prev_motor_state == 0)  open_door();
            while (1)
            {
                i++;
                softToneWrite(SPEAKER_PIN, i % 2 == 0 ? 523 : 933);
                delay(100);

                if (read_btn == 1)
                {
                    close_door();
                   
                    softToneWrite(SPEAKER_PIN, 0);
                    if (-1 == GPIOWrite(LED_FIRE_POUT, 0) || -1 == GPIOWrite(LED_GAS_POUT, 0) || -1 == GPIOWrite(LED_BOTH_POUT, 0))
                    {
                        error_handling("write() error");
                    }
                    read_btn = 0;
                    break;
                }
            }
        }
    }
}
int main(int argc, char *argv[])
{

    pthread_t p_thread, p_thread2, p_thread3;
    int status;
    //int sock, sock2;
    struct sockaddr_in serv_addr, serv_addr2;
    //led
    if (-1 == GPIOExport(LED_FIRE_POUT) || -1 == GPIOExport(LED_GAS_POUT))
    {
        return 1;
    }
    if (-1 == GPIODirection(LED_FIRE_POUT, OUT) || -1 == GPIODirection(LED_GAS_POUT, OUT))
    {
        return 2;
    }
    //speaker
    if (wiringPiSetup() == -1)
    {
        return 1;
    }
    //----위험감지파이 socket
    global_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (global_sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));
    if (connect(global_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect()1_error");
    //----관리자파이 socket
    global_sock2 = socket(PF_INET, SOCK_STREAM, 0);
    if (global_sock2 == -1)
        error_handling("socket() error");
    memset(&serv_addr2, 0, sizeof(serv_addr2));
    serv_addr2.sin_family = AF_INET;
    serv_addr2.sin_addr.s_addr = inet_addr(argv[3]);
    serv_addr2.sin_port = htons(atoi(argv[4]));

    if (connect(global_sock2, (struct sockaddr *)&serv_addr2, sizeof(serv_addr2)) == -1)
        error_handling("connect()2_error");

    softToneCreate(SPEAKER_PIN);
    softPwmCreate(MOTOR_PIN, 0, 200);
    
    //setting
    softToneWrite(SPEAKER_PIN, 0);
   
    //pthread
    pthread_create(&p_thread, NULL, danger_thd, NULL);
    pthread_create(&p_thread2, NULL, act_thd, NULL);
    pthread_create(&p_thread3, NULL, button_thd, NULL);
    pthread_join(p_thread, (void **)&status);

    return 0;
}
