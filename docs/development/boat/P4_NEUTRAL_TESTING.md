# Boat 가상 조이스틱 중립 매핑·SITL 검증

- 작성일: 2026-11-25
- 브랜치: `feat/boat-virtual-joystick-neutral`
- 기준: `feature/boat`의 `4413fd84f`
- 범위: PX4 Boat 가상 입력의 MANUAL_CONTROL throttle 인코딩

## 1. 수정 이유와 내부 경로

Boat의 가상 스틱은 후진/중립/전진을 `-1 / 0 / +1`로 제공한다.
기존 공통 송신기는 이 값에 1000을 곱하므로 중립을 `MANUAL_CONTROL.z = 0`으로 보냈다.
검증 대상 PX4 수신기는 `z / 500 - 1`로 throttle을 복원하므로 이 값은 최대 후진 요청이 된다.

```text
Boat QML 스틱 (-1 ~ +1)
       |
       v
Vehicle::virtualTabletJoystickValue
  활성 Boat / 연결 / 유한값 / 범위 검증
       |
       v
firmwarePlugin()->virtualJoystickThrottle
  PX4 + Boat: (throttle + 1) / 2
  다른 펌웨어 기본값: 입력 유지
       |
       v
기존 sendJoystickDataThreadSafe: throttle * 1000
       |
       v
MANUAL_CONTROL.z  ---> PX4: z / 500 - 1
```

| 스틱 | 새 PX4 Boat wire z | PX4 내부 요청 |
| --- | --- | --- |
| -1 | 0 | -1 |
| -0.4 | 300 | -0.4 |
| 0 | 500 | 0 |
| +0.4 | 700 | +0.4 |
| +1 | 1000 | +1 |

변환은 FirmwarePlugin hook으로 분리하고 PX4 Boat에만 적용한다.
ArduPilot Boat, Rover·항공기·Sub의 가상 입력과 물리 조이스틱 공통 송신 경로는 기존 값을 유지한다.
QML 축 반전·배치·자동 복귀 설정과 `supportsNegativeThrust`는 변경하지 않는다.
역추진 인코딩과 실제 추진계의 역추진 지원 여부는 별도 조건이다.

## 2. 자동 회귀 범위

- `BoatNavigationTest`: PX4/ArduPilot × Boat/Rover/Multirotor/FixedWing/VTOL/Sub,
  각 5개 throttle 지점의 실제 MANUAL_CONTROL 패킷 검사(60개 조합).
- 같은 행렬에서 x/y/r·대상 ID 보존과 물리 조이스틱 송신값 0.5 → z 500 유지 검사.
- 기존 비활성/초기 연결/단절 및 NaN/무한대/범위 밖 Boat 입력 거부 유지.
- `BoatVirtualJoystickTest`: 실제 QML의 배치·자동 복귀·기체 전환·세션 취소 회귀.
- 기존 Joystick/미션/기체·QML 및 전체 Unit 회귀 결과는 아래 실행 기록에 기재한다.

## 3. 실제 UDP SITL 테스트 실행

`BoatVirtualJoystickSITLTest`는 MockLink 대신 실제 QGC Vehicle과 UDPLink를 사용한다.
명시적 simulator 확인값이 없으면 skip하며 일반 Unit 실행에는 포함되지 않는다.
사람이 시뮬레이터 주소·기체 ID·다른 제어기 입력 부재를 확인한 환경에서만 실행한다.

예시(포트/IP는 환경에 맞게 지정):

```bash
export QGC_BOAT_SITL_ARMING=I_CONFIRM_SIMULATION
export QGC_BOAT_SITL_HOST=192.168.0.18
export QGC_BOAT_SITL_LOCAL_PORT=14555
export QGC_BOAT_SITL_REMOTE_PORT=18590
export QGC_BOAT_SITL_SYSTEM_ID=1
export QGC_BOAT_SITL_REPORT="$PWD/build/boat-p3/boat-neutral-validation/live-qgc.json"
ctest --test-dir build/boat-p3 -R '^BoatVirtualJoystickSITLTest$' --output-on-failure
```

PX4는 위 local port로 송신하고 remote port에서 QGC 입력을 받아야 한다.
테스트는 단일 PX4 Boat·예상 ID·초기 disarmed를 확인하고 Manual 모드를 선택한다.
실제 `LinkInterface::bytesSent` 패킷에서 중립 z 500을 확인한 뒤 정상 arm 요청을 보낸다.
force arm이나 파라미터 우회는 사용하지 않는다.

순서는 prearm 중립 2초 → armed 중립 4초 → 전진 +0.4 6초 → 중립 10초 →
후진 -0.4 18초 → 중립 4초다. 시간은 PX4의 시뮬레이션 시계를 따른다.
전진 방향 이동과 후진 마지막 3초의 반대 방향 이동을 위치·Heading으로 검사한다.
각 구간의 wire z, 위치 변화, SOG, 시각을 JSON에 남긴다.
송신 알림 큐에 남은 직전 구간 값은 새 값의 첫 수신 전까지만 허용한다.
새 값이 나타난 뒤에는 구간 끝까지 동일한 값이어야 하며, 새 값이 한 번도 없으면 실패한다.
cleanup에서 중립을 요청하고 정상 disarm 및 이전 모드 복원을 확인한다.
프로세스 강제 종료 시 cleanup은 보장되지 않으므로 simulator의 disarm을 별도로 확인해야 한다.

Gazebo의 실제 추진 모델 출력은 QGC의 요청값과 구분해 별도로 수집한다.
`/model/aura/drive/state`의 throttle·추력과 `/model/aura/odometry`의 위치를 비교한다.
중립은 추진 명령이 0이라는 의미다. 관성으로 움직이는 선체의 즉시 정지를 뜻하지 않는다.

## 4. 실행 기록

### 환경과 판정

- 송신: macOS arm64, Qt 6.11.1, Debug QGC의 실제 Vehicle/UDPLink 경로.
- 수신: Ubuntu 24.04.5 LTS의 기존 PX4 SITL + Gazebo 전곡항 calm 월드·aura 선박.
- PX4 기준 커밋: `fca3df865af36124a28c9d607e850f111dbaaea9` + 기존 사용자 Boat 수정본.
  수정본을 재설정하거나 덮어쓰지 않았으며 기존 실행 바이너리를 사용했다.
- AuraSim 기준 커밋: `7a648829815109e2a1ba34fa7ff3da862d5bbd65` + 기존 작업 상태.
- HEARTBEAT 실측: system 1 / component 1 / MAV_TYPE 11 / autopilot PX4.
- 최종 `BoatVirtualJoystickSITLTest`: **통과**, 약 51.8초.
- 종료 상태: **disarmed, Hold 복원**. Gazebo/PX4와 사용자의 GUI는 계속 실행하며 측정기만 종료했다.

실제 PX4 Actuator 메타데이터가 QML image provider를 등록하므로 테스트에도 QML 엔진을 제공한다.
호스트 기압 센서가 없는 데스크톱의 두 압력 센서 초기화 경고만 명시적으로 제외한다.
기체 prearm/제어 오류는 이 예외에 포함하지 않는다.

### 실제 요청과 독립 물리 측정

Gazebo 수치는 각 구간 첫 1초를 제외한 평균 추력과 해당 전체 구간의 선체 x 변화다.
선수는 Gazebo +x 방향이며 QGC Heading은 약 90도였다. QGC의 GNSS/추정 위치와
Gazebo의 물리 위치는 별도로 기록하며 두 값의 수치적 일치를 강제하지 않는다.

| 구간 | 안정 wire z | 평균 실제 추력 N | Gazebo 선체 x 변화 m | QGC 선수 방향 위치 변화 m |
| --- | --- | --- | --- | --- |
| armed 중립 4초 | 500 | 0.00 | -0.010 | -0.027 |
| 전진 +0.4 6초 | 700 | +620.88 | +2.104 | +2.206 |
| 전진 후 중립 10초 | 500 | 0.00 | +7.127 | +7.060 |
| 후진 -0.4 18초 | 300 | -371.03 | -6.397 | -6.546 |
| 후진 후 중립 4초 | 500 | 0.00 | -3.748 | -3.721 |

후진 마지막 3초의 Gazebo x 변화는 **-2.843m**였다.
중립 구간의 위치 변화는 관성이며, 0N 추진 중립과 정지 유지 모드를 혼동하지 않는다.
구간 전환 시 직전 송신 알림이 먼저 기록될 수 있지만 새 값이 확인된 이후에는 안정 값만 관측했다.
Gazebo 추력·방향 대조 assertion도 통과했다.

PX4 실행 바이너리 SHA-256:
`f0ad86c1449f96f22cedfe66c8206449aeb2cdc4bcfd59c1646429e7611eb49a`.
확인한 수신 코드 `mavlink_receiver.cpp` SHA-256:
`e429651149fe12c15f85c45afc0918a34f2e384d6d259687add855bc553c224e`.

### 빌드·회귀·lint

- `cmake --build build/boat-p3 --config Debug --parallel 6`: 통과.
- `ctest --test-dir build/boat-p3 --tests-from-file <절대경로>/regression-tests.txt
  --output-on-failure --parallel 4`: 전체 Unit 200개를 포함한 관련 **214개 모두 통과**(약 179.3초).
  Unit 전체와 MissionManager/Joystick/Boat 및 PlanView/기체 연결 회귀를 선택했다.
- 명시적 SITL 실행 확인값 없는 `BoatVirtualJoystickSITLTest`: **skip 확인**, 제어하지 않음.
- 변경한 production 구간의 clang-format 22.1.5와 테스트 파일 전체 포맷: 통과.
- Markdown, CMake, 공백/충돌/대용량 파일, 금지 assert/로그/고정 qWait/QTest ignore,
  QT_TRANSLATE_NOOP 개별 검사: 통과.
- 전체 pre-commit: npm `EALLOWGIT`로 markdownlint hook 환경 설치 단계에서 중단.
  별도 설치된 markdownlint로 변경 문서를 검사했다. 전체 lint 통과로 표시하지 않는다.
- `vehicle-null-check`의 기존 production 경고 7건은 기준 커밋 파일에서도 동일하게 재현했다.
  이번 변경 구간에서 새 경고는 없다. 전체 파일의 기존 형식·정적 분석 부채는 별도 범위다.

원시 결과는 Mac의 `build/boat-p3/boat-neutral-validation/`에 보관한다.
`live-acceptance.log`, `live-qgc.json`, `gazebo-final.jsonl`, `gazebo-final-summary.json`,
`regression-final.log`, `hooks-final.log`, `precommit.log`를 확인한다.
Ubuntu 측 독립 원시 기록은 AuraSim의 `build/qgc-mac-remote-20261125/neutral-gazebo.jsonl`이다.

## 5. 남은 인수 범위

이번 시험은 아래 특정 custom PX4와 전곡항 calm 시뮬레이터의 결과다.
모든 PX4 버전·선박·물리 조이스틱/RC·Android·입력 상실 failsafe의 인수를 대체하지 않는다.
P4/T12 전체와 P5 전체는 계속 미완료다. 공식 upstream 승인도 별도다.
