# toy_project_system

### Sensor Driver

리눅스 I2C 드라이버 (BMP280)

What is I2C?

- on-board 및 외부 장치를 프로세서에 연결하기 위해 사용되는 매우 일반적인 저속 버스
- 2개의 전선만 사용된다: SDA for the data, SCL for the clock.
- 마스터/슬래이브 버스: 마스터가 트랜잭션을 초기화할 수 있고, 슬래이브는 파이스가 초기화한 트랜잭션에 응답할 수 있다.
- 리눅스 시스템에서 프로세서에 내장된 I2C 컨트롤러는 일반적으로 버스를 제어하는 마스터이다
- 각 슬래이브 디바이스는 고유한 I2C 주소로 식별된다
- 마스터에 의해 초기화된 각 트랜잭션은 이 주소를 포함하며, 관련된 슬래이브가 이 특정 트랜잭션에 응답해야 한다.

GND : Raspi Pin 34 (Ground)

VCC : Raspi Pin 17 (3V3 Power)

SDA : Raspi Pin 3 (GPIO 2, SDA)

SCL : Raspi Pin 5 (GPIO 3, SCL)

