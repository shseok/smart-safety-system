# System Programming Team3 Project

## Smart Safety System
본 프로젝트는 시스템 프로젝트 강의용이다

# 프로젝트 목표
라즈베리파이를 활용하여 가스, 화재를 감지한 후 이를 스피커와 LED를 통해 어떠한 위급 상황인지를 사용자들에게 알려주고 건물 내 비상문의 도어락을 모두 개방하는 **스마트 안전 시스템(SSS)**를 고안하였다.

# 응용시스템 구조
<img src="./img/system.png" width="550px" height="300px">

# 팀원 소개
- 아주대 소프트웨어학과 201720779 김강현
    - 감지파이 담당
- 아주대 소프트웨어학과 201820770 문민수
    - 안내파이 담당
- 아주대 소프트웨어학과 201823779 신현석
    - 관리자파이 담당
    
# 파이 의미
- 위험감지파이
    - 가스, 불꽃 센서를 통해 위험을 감지한다
- 위험안내파이
    - 스피커, 서보모터, LED를 통해 위험을 안내한다
- 관리자파이
    - 버튼을 통해 시스템을 통제 / 감지 위험 현장에서 사람이 있다고 판단할 경우 이를 카메라로 촬영하여 화면을 통해 보여준다
 
# 실행 방법
1. 각 파이에서 make를 해준다
2. 감지파이에서 아래 명령어를 실행한다
```
./detect {port for 안내파이} {port for 관리자파이}
```
3. 관리자파이에서 아래 명령어를 실행한다
```
./manager 192.168.0.5 {port for 감지파이} {port for 안내파이}
```
4. 안내파이에서 아래 명령어를 실행한다
```
./act 192.168.0.3 {port for 감지파이} 192.168.0.6 {port for 관리자파이}
```

# 기술스택
1. Multi Thread
2. Socket Programming
3. PWM
4. GPIO
5. SPI (with ADC)
6. WiringPi Library

# 변동사항
- 관리자 파이: 
    1. button export 이후 3초의 sleep을 주었다.
    2. 타이머의 기본 값은 5초인데, 옵션을 통해 타이머 시간을 바꿔주었다. 타이머 시간을 바꾸는 옵션은 t이다. 단위는 밀리초(Millisecond)를 사용하기때문에 -t 1000 옵션을 주어 타이머 시간을 1초로 설정해주었다. 사진이 화면에 출력되기 위해 sleep을 기존 8초에서 3초로 변경해주었다. 
    3. 위험감지시 카메라가 작동되므로 LED도 같이 작동시켜주어 카메라 작동의 유무를 더욱 잘 파악할 수 있도록 추가해주었다.
    4. 움직임 센서에 대한 기능을 위험 감지 파이로 전달하였다.

- 위험 감지 파이:
    1. 온습도 센서가 제대로 작동하지 않아 이를 대체하는 기능으로 움직임 감지 센서를 추가하였다.

# Demo
![demo](./img/systemDemo.gif)

# 고려해야할 사항
- 관리자 파이: 프로세스를 만들고 스트리밍 방식으로 활용하려 했으나 프로세스가 죽지 않는 문제점 (따라서, 현재는 사진을 일정 간격으로 확인함으로 대체)

# ref
http://www.3demp.com/community/boardDetails.php?cbID=233
