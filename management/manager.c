/* motion, carmera, button  (client socket)
* 1. 모션이 감지시 카메라 촬영하여 화면 보여줌
* 2. 버튼을 통해 시스템 통제
* 위험 감지 파이에서 감지를 받고 처리함. 오작동일 경우 안내한테? 어떻게 주지
*/
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h> // Handle Signal
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdbool.h>

#define IN 0
#define OUT 1
#define LOW 0
#define HIGH 1
#define PIN 20       // Button
#define POUT2 21     // Button
#define POUT 17      // LED
#define MOTION_PIN 7 // MOTION
#define VALUE_MAX 40
#define DIRECTION_MAX 45
#define BUFFER_MAX 3

char cmd[] = "sudo raspistill -o /home/pi/Pictures/test.jpg"; // 사진
char cmd_other[] = "sudo raspivid -o vid.h264";               // 동영상

char tmp_buffer1[100];
char tmp_buffer2[100];

static bool is_change = false;
static bool is_on = false;
char answer[2] = "1"; // 버튼 대답

int serv_sock, sock, clnt_sock = -1;
struct sockaddr_in serv_addr, clnt_addr, cli_addr;
socklen_t clnt_addr_size;
int str_len;

int pid;
int camera_work = 0;

char r_msg[4];
char w_msg[1];

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
        close(fd);
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
        close(fd);
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
        close(fd);
        return (-1);
    }

    close(fd);
    return (0);
}

void (*breakCapture)(int);

void signalingHandler(int signo) //Disable GPIO pins
{
    printf("Ctrl-C 키를 눌루셨죠!!\n");
    printf("또 누르시면 종료됩니다.\n");
    GPIOUnexport(MOTION_PIN);
    GPIOUnexport(POUT);
    GPIOUnexport(POUT2);
    GPIOUnexport(PIN);
    exit(1);
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

void *send_thd()
{
    int state = 1;
    int prev_state = 1;
    while (1)
    {
        if (-1 == GPIOWrite(POUT2, 1)) // for button
            return;
        state = GPIORead(PIN);
        if (state == 0 && prev_state == 1) // press
        {

            write(clnt_sock, "1", 1);
            printf("msg = %s\n", w_msg);
            camera_work = 0;
        }
        else
        {
            write(clnt_sock, "0", 1);
        }
        prev_state = state;
        usleep(500000);
    }
}

void *receive_thd()
{
    while (1)
    {
        str_len = read(sock, r_msg, sizeof(r_msg)); // 길이 4의 값을 매순간 읽음(화재/가스 : "00", "01", "10", "11" "111");
        usleep(500000);
    }
}

void *camera_thd()
{
    system(cmd);
}

int main(int argc, char *argv[]) // 주소, 포트1, 포트2
{
    int repeat = 100;
    int state = 1;
    char prev_state[100];
    int light = 0;
    int motion;

    pthread_t p_thread[3];
    char warning111[4] = "111";
    char warning11[3] = "11";
    char malfunction1[5] = "01";
    char malfunction2[5] = "10";

    int camera_work = 0;

    int thr_id;

    char *tmp_buffer = "kill -9";
    pid_t pid; /* process id */

    // ------------------------Handle Signal----------------------
    setsid();
    umask(0);

    breakCapture = signal(SIGINT, signalingHandler);

    // ------------------------LED----------------------
    if (-1 == GPIOExport(POUT)) //Enable GPIO pins
    {
        usleep(300000);
        return (1);
    }

    if (-1 == GPIODirection(POUT, OUT)) //Set GPIO directions
        return (2);

    // ------------------------Button----------------------
    if (-1 == GPIOExport(POUT2) || -1 == GPIOExport(PIN)) // usleep 추가
        return (1);

    if (-1 == GPIODirection(POUT2, OUT) || -1 == GPIODirection(PIN, IN))
        return (2);

    // ------------------------Socket----------------------

    if (argc != 4)
    {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    sock = socket(PF_INET, SOCK_STREAM, 0); // client
    if (sock == -1)
        error_handling("socket() error");

    memset(&cli_addr, 0, sizeof(cli_addr)); // client
    cli_addr.sin_family = AF_INET;
    cli_addr.sin_addr.s_addr = inet_addr(argv[1]);
    cli_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr *)&cli_addr, sizeof(cli_addr)) == -1)
        error_handling("connect() error");
    // -----------------------server--------------------
    serv_sock = socket(PF_INET, SOCK_STREAM, 0); // server
    if (serv_sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[3]));

    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) // bind
        error_handling("bind() error");

    if (listen(serv_sock, 5) == -1) // listen
        error_handling("listen() error");
    sleep(1);

    if (clnt_sock < 0)
    {
        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_addr_size); // accept
        if (clnt_sock == -1)
            error_handling("accept() error");
    }

    thr_id = pthread_create(&p_thread[0], NULL, send_thd, NULL);
    if (thr_id < 0)
    {
        perror("thread create error : ");
        exit(0);
    }
    thr_id = pthread_create(&p_thread[1], NULL, receive_thd, NULL);
    if (thr_id < 0)
    {
        perror("thread create error : ");
        exit(0);
    }

    //------------------------main----------------------
    while (1)
    {
        is_on = 0;
        strcpy(prev_state, r_msg);

        if (strncmp(prev_state, r_msg, 3) == 0)
        {
            is_change = true;
        }
        if (str_len == -1)
            error_handling("read() error");

        printf("Receive message from Server : %s\n", r_msg);

        if (strncmp("111", r_msg, 3) == 0 || strncmp("011", r_msg, 3) == 0 || strncmp("101", r_msg, 3) == 0) // 111
        {
            light = 1;
            is_on = true;
        }

        if (is_on)
        {
            camera_work = 1;

            pthread_create(&p_thread[2], NULL, camera_thd, NULL);
            char **command = (char **)malloc(sizeof(char *) * 10);
            command[0] = "xdg-open";
            command[1] = "/home/pi/Pictures/test.jpg";
            command[2] = NULL;

            sleep(8);
            pid = fork();
            if (pid == 0)
            {
                if (execvp(command[0], command) == -1)
                {
                    printf("Error in Camera!\n");
                }
            }
        }

        /*if (GPIORead(PIN) == 0) // 오작동시 누름
        {
            char **button_argv = (char **)malloc(sizeof(char *) * 10);
            button_argv[0] = "pskill";
            button_argv[1] = "-9";
            button_argv[2] = "-f";
            button_argv[3] = "test.jpg";
            button_argv[4] = NULL;
            camera_work = 0;
            int temp_pid = fork();
            if (temp_pid == 0)
            {
                if (execvp(button_argv[0], button_argv) == -1)
                {
                    printf("Error in terminating Streaming!\n");
                }
            }
            int state;
            wait(&state);

    }*/
        printf("is_on = %d\n", is_on);
        printf("camara_work = %d\n", camera_work);

        printf("GPIORead : %d from pin %d\n", GPIORead(PIN), PIN);

        GPIOWrite(POUT, light); // 위험감지시 불 켜짐 (오작동시 켜지도록 해야하나)
        usleep(500000);         // 0.1마다 repeat이 0이 될 때까지 계속 읽음
    }

    // thread_stop();
    int status;
    pthread_join(p_thread[0], (void **)&status);
    pthread_join(p_thread[1], (void **)&status);
    pthread_join(p_thread[2], (void **)&status);

    close(clnt_sock);
    close(serv_sock);
    close(sock);
    return (0);
}
