# edu_IOCP
IOCP 실습

Overlapped IO
- 리눅스 모델은 데이터의 입력이 완료되는 시점에 읽는 것이 아니다.
- Overlappped IO는 데이터의 입력이 완료되는 시점에 이벤트를 발생시킨다.
- 여러번의 데이터 복사가 일어나지 않는다!

Socket
- 기본적인 소켓은 중첩특성을 가진다.
- BSD 소켓은 중첩 특성을 명시해야 한다.
- 중첩 소켓의 사용은 lpOverlapped 매개변수를 통한다.
