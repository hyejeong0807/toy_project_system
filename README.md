# toy_project_system

#### 쓰레드 구현하기

#### 뮤텍스 추가

#### 멀티 스레드 동기화 & C와 C++ 연동

#### 시스템 V 메시지 큐 & 세마포어 & 공유 메모리

sensor thread (input process)에서 보내는 값을 공유 메모리에 저장한 후 monitor thread (system server process)에서 attach하여 읽어와 출력하는 작업

메시지 큐에 공유 메모리 키를 담아서 보낸다.

