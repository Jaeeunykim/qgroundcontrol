# badaMux 무인선 통신·동작 명세

버전 `0.1.0-draft` · 2026-11-17 · [문서 안내](README.md) · [메시지 카탈로그](MESSAGE_CATALOG.md) · [인수 계획](ACCEPTANCE_TEST_PLAN.md)

## 1. 적용 범위와 용어

이 문서는 제품의 `GCS ↔ PX4 ↔ badaMux ↔ 장비` 통신과 선택적 선내 자율운항 컴퓨터의 경계를 정의한다.
CAN/J1939·NMEA2000·EtherCAT PDO/SDO의 개별 장치 레지스터/PGN 값은 해당 장치의 고정 버전 문서로 연결한다.
확인하지 않은 버스 코드·기어 매핑·각도 한계·성능 수치를 사실로 만들지 않는다.

| 용어 | 의미 |
| --- | --- |
| 운항 의도 | 시작·중단·복귀·목표 속도·Go To처럼 PX4가 처리할 상위 요청 |
| 장비 목표 | PX4가 badaMux에 내리는 출력·타각·기어 등 저수준 명령 |
| 실제 상태 | ECU/센서/드라이브의 피드백. 요청값·출력 레지스터와 구분 |
| 연결 생존 | 특정 endpoint의 통신 존재. 제어권·목표 최신성·센서 정상과 별개 |
| 소유권/lease | 특정 범위에 대해 특정 주체만 제어하도록 하는 제한 시간의 권한 |
| 세션 | 부팅·인증·소유권 획득 시 분리되는 명령 수명. 재연결로 이전 세션을 복구하지 않음 |
| 기본/선택 기능 | M0~M2 기본 운항과 장비별 선택 기능. 미지원은 정상 0과 다름 |
| 기존/제안 | 기존 XML 선언 E, 표준 MAVLink S, 신규 논리 메시지 P. 실행 지원 여부는 별도 |

## 2. 책임·제어 경계

| 노드 | SHALL 책임 | 맡지 않는 기본 책임 |
| --- | --- | --- |
| GCS | 사용자 의도·계획 전송, 상태·결과·제어 주체 표시, 입력 상실·세션 해제 전달 | 실시간 장비 루프·단절 중 정지 보장 |
| PX4 | 항법·모드·미션·기어/추진/조향 목표 생성·상위 제어 중재·badaMux 경로 | 장비 실제값이 없는 상태에서 완료 추정 |
| badaMux | 주소/세션/범위/상태/만료 검증, 단일 실행 주체, 장비 제한·인터록·watchdog, 실제 상태·거부/오류 보고 | 기본 경로계획·AIS 판단·GCS 미션 파일 관리 |
| 선내 자율운항 컴퓨터 | 선택적 인식·경로계획·제약 검증, PX4와 명시적 제어 계약 | badaMux에 우회 출력하거나 물리 정지 회로 대체 |
| 물리 조종기·비상회로 | 합의된 우선권·로컬 개입·하드웨어 정지 상태 제공 | GCS 메시지 수신에만 의존하는 보호 |

### R-01 단일 명령 경로

정상 운항은 GCS/자율운항 컴퓨터 → PX4 → badaMux 경로를 사용한다.
유지보수 직접 제어는 별도 capability·물리/운용 조건·권한을 요구하고 AUTO와 동시 실행하지 않는다.
라우터는 forwarding과 메시지 변환/재발행을 구분한다. 단순 forwarding은 원본을 변경하지 않는다.
장비마다 실제로 여러 소유자를 허용하지 않으며 PX4와 badaMux에서 모두 중재 조건을 검사한다.

### R-02 capability 프로필

장치는 프로토콜/펌웨어/설정 버전, 하드웨어 식별, 실제 장치 목록과 다음 지원 여부를 보고해야 한다.

- 내연기관/전동 추진, 엔진 출력/목표 RPM 지원, ECU 실제 RPM·온도·연료 피드백.
- 기어 없음/전진·중립·후진, 실제 기어 피드백·인터록 지원.
- 단일/다축 러더, 각도 실측/엔코더 환산, PP/CSP 등 실제 지원 모드.
- 조종기·원격 수동·자동항로·Go To·위치 유지·복귀·offboard 지원.
- 세션·lease·명령 TTL·중복 억제·부분 정지 결과·원천 freshness·서명 검증 지원.
- 선택 센서·작업 장비·지오펜스·계획기·장애물·접안 기능.

capability가 없음/알 수 없음이면 해당 능동 기능은 미지원으로 취급한다.
프로토콜 지원과 현재 사용 가능 상태는 별도다. 장비가 있어도 고장/미보정/제어권 없음이면 조작 불가다.

## 3. 전송·식별·호환

### R-03 transport

PX4↔badaMux는 MAVLink 2를 기준으로 검토한다. 실제 UART/네트워크·baud·포트·flow control·배선은 D02에서 고정한다.
GCS 링크, 선내 링크, ECU/드라이브 버스의 대역폭·수명·오류는 따로 측정한다.
프레임 분할/병합·잡음·잘못된 CRC·알 수 없는 incompatibility flag·버퍼 초과를 처리한다.
파서 오류 프레임을 제어 상태로 사용하지 않으며 송수신 스레드가 장비 보호 루프를 막지 않게 한다.

### R-04 주소와 인스턴스

- 선박 `system_id`, 노드 `component_id`, 장비/축 인스턴스, 제품 고유 식별을 배치표로 관리한다.
- 동일 선박 system 내에서 component를 중복 할당하지 않는다. system/component는 인증 수단이 아니다.
- badaMux를 autopilot/다른 선박으로 잘못 발견시키지 않도록 HEARTBEAT의 type·autopilot·출처를 합의한다.
- 능동 명령은 명시적 대상과 단일 소유 범위를 요구한다. 정상 운항에서 방송 명령을 실행하지 않는다.
- 기존 target 필드 없는 E 메시지는 선박 전용 격리 경로의 legacy 제약을 가진다.
  공유 라우터에서 모든 선박으로 방송해 실행시키는 방식은 허용하지 않는다.
- 다중 링크 복제는 같은 요청의 중복으로 인식한다. 링크가 바뀌어도 소유권을 추가로 획득한 것으로 취급하지 않는다.

### R-05 버전·schema

제품 프로토콜 major/minor, XML commit/hash, firmware commit, hardware revision, 설정 revision을 기록한다.
호환되지 않는 major는 능동 제어를 차단하고 진단만 허용한다. minor 확장도 capability를 확인한다.
기존 메시지 ID의 타입·단위·부호·enum 의미를 변경하지 않는다. 신규 ID/enum은 XML 소유자와 충돌 검사 후 할당한다.
MAVLink XML의 `<version>3</version>`을 MAVLink wire protocol 3으로 해석하지 않는다.
생성기 버전과 include closure를 고정하고 생성 C/Kotlin/C++ 파일을 수동 수정하지 않는다.

legacy 수신기가 session/lease/TTL 확장을 무시하면 안전 계약을 충족한 것으로 보지 않는다.
호환 adapter나 새 endpoint에서 강제할 수 없는 legacy 능동 제어는 자동으로 허용하지 않는다.
legacy 공개 진단 지원과 운영 제어 승인 프로필을 구분한다.

## 4. 공통 데이터 규칙

### R-06 물리량·좌표

| 항목 | 제품의 논리 기준 | 주의 |
| --- | --- | --- |
| 시간 | 단조 시각 us, 주기/TTL ms, UTC는 별도 optional | boot/session과 시계 품질 포함 |
| 위치 | WGS84 위경도 deg, wire 정수 필드는 명시적 degE7 | `(0,0)`은 자동 무효가 아님 |
| 자세·방위 | Heading/COG는 진북 기준 [0,360) deg, yaw rate deg/s | 자기북은 reference 표시·변환 근거 필요 |
| 기체 좌표 | x 선수, y 우현, z 아래. 사용 frame을 항상 명시 | NED/ENU/body 값을 암묵 혼용하지 않음 |
| 속도 | m/s, 가속도 m/s², 거리 m | SOG/STW·목표/실제·전후진 부호 구분 |
| 러더 각도 | 중심 0 deg, 음수 좌현·양수 우현을 제품 프로필로 검증 | 배선/기구 반전과 측정 위치 기록 |
| 추진 출력 | 내연기관 요청은 [0,1] 또는 %의 명시적 변환 | 기존 50001 wire는 uint16 0~1000이며 음수 후진 불가 |
| 전동 추력 | 지원 프로필에서만 signed [-1,1] 또는 물리량 | 실제 추력/출력·측정값으로 오인하지 않음 |
| RPM·온도·압력 | rpm, °C, Pa의 논리 단위 | 표준 wire 단위가 다르면 지정 변환. 냉각수/헤드/배기가스 구분 |
| 전기·연료 | V, A, W, Wh, 연료 L 또는 kg를 명시 | wire 단위·밀도 변환 근거·센서별 의미 유지 |
| 수심·해도 | 센서 수심 m, 해도 기준면·GNSS 고도는 별도 | GNSS 고도를 수심 필드로 재사용하지 않음 |

표준 MAVLink의 필드·sentinel·frame 정의를 이 표로 덮어쓰지 않는다. 논리 단위와 wire 단위를 카탈로그에 함께 기록한다.

### R-07 유효성과 원천 freshness

모든 상태 그룹은 장비 인스턴스, sample/boot 식별, 측정 또는 원천 갱신 시각/age, valid 필드,
측정/추정 출처, 활성 fault를 식별할 수 있어야 한다. 묶음 내 필드가 다르게 갱신되면 필드별 age를 제공한다.
상태는 `UNSUPPORTED / NOT_RECEIVED / VALID / INVALID / STALE` 의미를 구분한다.
0 RPM·0 deg·0°C·0 V 중 유효한 실제 0과 미수신/고장 표현을 혼동하지 않는다.
PX4가 마지막 값으로 새 패킷을 발행해도 원천 age를 0으로 초기화하지 않는다.
timestamp 없는 기존 메시지는 보장 한계를 기록하고, 다른 표준 메시지의 미사용 필드에 age를 숨겨 넣지 않는다.

### R-08 시계·지연

원천 시계와 수신 시계의 offset·drift·불확실도를 추정하거나 동등한 age 검증 경로를 제공한다.
서로 다른 장치 boot timestamp를 직접 빼지 않는다. TIMESYNC 지원 여부와 유효기간은 프로필에 기록한다.
능동 명령은 최대 허용 age를 검증할 수 없으면 거부한다. 수신 순간부터 TTL을 시작하는 것만으로 전송 중 지연을 검출했다고 보지 않는다.
재부팅·시계 점프·counter wrap은 새 세션/epoch로 구분한다. UTC 동기화 상실과 로컬 watchdog은 분리한다.

## 5. 명령 세션·응답·재시도

### R-09 제안 명령 context

신규 제품 능동 메시지는 대상·송신자 신뢰 context, 발신 boot/session, 제어 lease/epoch,
요청 ID 또는 setpoint sequence, 전송 시각·최대 age/TTL, 장치/축 scope를 식별해야 한다.
필드 타입은 카탈로그 C-CMD/C-STREAM에서 정의하며 실제 payload 배치/ID는 별도 결정한다.
128/64비트 식별자를 `COMMAND_LONG`의 float param으로 옮겨 담지 않는다.
표준 MAVLink 명령에는 해당 프로토콜을 사용하고 표준 payload에 없는 제품 필드를 임의 추가하지 않는다.
최초 식별/시간 조회와 lease 획득 요청은 아직 lease를 가질 수 없으므로 기존 lease 요구의 예외다.
이때도 발신자 인증·대상·요청 freshness·권한·획득 조건을 검사하고, 획득 승인 전 장비 목표는 실행하지 않는다.

### R-10 요청 수명과 완료

```text
CREATED → VALIDATED → SENT → ACCEPTED/IN_PROGRESS → SUCCEEDED 또는 FAILED/CANCELED
                       ↘ REJECTED
                       ↘ 응답 시간 초과 → RESULT_UNKNOWN → 상태 조회/조작자 확인
```

`ACCEPTED`는 접수 의미다. 실행 완료는 장치 피드백·허용 오차·지속 시간·최대 대기를 충족해야 한다.
기존 `BADAMUX_SERVO_ACK.ACCEPTED`는 기존 동작 명령의 처리 결과이며 물리 타각 도달을 증명하지 않는다.
새 제품 논리 상태 이름을 기존 ACK enum에 숫자로 추가한 것으로 해석하지 않는다.
`IN_PROGRESS`에서 최종 결과를 기다리고 진행 알림만으로 대기 시간을 무제한 연장하지 않는다.
일부 장치만 성공한 요청은 `PARTIAL` 상세 결과로 보고하며 전체 성공으로 표시하지 않는다.

### R-11 중복·재시도·취소

- 단발 요청의 대응 키는 `(authenticated sender, sender_boot, session, request_id, operation, target instance)`다.
- 같은 키·같은 payload 재전송은 저장한 결과를 반환하고 장비를 두 번 실행하지 않는다.
  같은 키·다른 payload는 충돌로 거부한다.
- 새 request ID는 새 의도다. 응답 유실을 새 요청 ID로 무조건 재시도하지 않는다.
- 중복 보관 기간은 최대 재시도 구간 + 최대 패킷 지연 + 여유보다 길게 정하고 메모리 상한을 둔다.
- 재부팅으로 결과 journal을 잃은 비멱등 동작은 자동 재실행하지 않고 결과 미확인으로 복구한다.
- 표준 command/mission 재전송은 표준과 해당 PX4 구현을 따른다. 엔진 시동·기어 전환·보정·설정 저장 등 제품 비멱등 동작은 별도의 중복 보호가 필요하다.
- 취소 요청도 원래 작업의 완료와 경쟁할 수 있다. `취소 접수`와 `이미 완료`를 구분하고 물리 복귀를 자동 약속하지 않는다.
- setpoint는 매번 ACK를 기다리지 않고 최신 유효 sequence만 처리한다. 역순/중복 패킷은 적용하지 않으며 중복으로 TTL을 갱신하지 않는다.

sequence는 동일 세션에서 uint32 modular 비교를 사용할 경우 half-range 규칙과 reset 조건을 고정한다.
최신값 mailbox를 사용하며 과거 목표를 무제한 FIFO에 쌓지 않는다.

## 6. 제어권·상태 머신

### R-12 두 계층의 lease

1. 상위 운항 소유권: 운용자/RC/선내 자율운항 컴퓨터 중 누가 PX4에 의도를 내릴 수 있는가.
2. 장비 소유권: PX4 또는 승인된 정비 주체 중 누가 badaMux 장비 범위를 제어하는가.

`HEARTBEAT`, ARM, 정상 모드, GCS 연결만으로 lease가 생기지 않는다.
획득은 대상 scope·요청 역할·요청 수명·선박 상태를 검사한다. 승인된 소유자·lease ID·epoch·남은 시간을 보고한다.
갱신은 현재 소유자만 가능하다. 갱신은 연결 유지 의도이지 오래된 장비 목표의 수명 연장이 아니다.
양도 시 이전 epoch의 목표·대기 작업을 폐기하고 새 소유자 입력이 준비된 뒤 전환한다.
재접속은 모니터링부터 시작하며 이전 lease나 목표를 자동 복원하지 않는다.

표준 QGC가 P_AUTHORITY 메시지를 이미 송신한다고 가정하지 않는다. 표준 운항 명령을 제품 PX4가 승인하는 adapter를
채택한다면, 인증된 발신자·기존 RC/모드 절차·현장 조건을 소유권 모델에 대응시키고 그 정책을 문서화한다.
제품 전용 lease 상태/동작 UI가 필요한 기능은 별도 제품 앱/확장 범위이며 공식 QGC의 필수 기능으로 이전하지 않는다.

### R-13 우선권

기본 우선순위 제안은 물리 비상정지 → 로컬 보호/인터록 → 승인된 현장 수동 개입 → PX4 운항 제어 → 원격 정비다.
실제 RC/현장 스위치·강제 인계 정책은 D05에 확정한다. 단순 연결 순서나 마지막 패킷 수신으로 우선권을 바꾸지 않는다.
scope별 동시 소유 가능 여부를 명시하고 추진·기어·조향의 위험한 분할 소유를 금지한다.
안전 정지 요청도 인증된 안전 역할/물리 입력으로 제한한다. 아무 익명 방송 명령을 정지 권한으로 인정하지 않는다.

### R-14 서로 독립적인 상태 축

| 상태 축 | 제안 의미 |
| --- | --- |
| 노드 | BOOTING / DISCOVERED / CONFIGURING / READY / DEGRADED / FAULT / MAINTENANCE |
| 장비 | DISABLED / ENABLING / ENABLED / MOVING / HALTED / QUICK_STOP / FAULT |
| 제어권 | UNOWNED / REQUESTED / OWNED / TRANSFERRING / EXPIRED / REVOKED |
| 운항 | IDLE / MANUAL / ASSISTED / AUTO_MISSION / HOLD / RETURN / FAILSAFE |
| 계획 | EMPTY / STAGED / VALIDATED / ACTIVE / PAUSED / COMPLETED / ABORTED |
| 데이터 | NOT_RECEIVED / VALID / INVALID / STALE / UNSUPPORTED |

운항 HOLD의 구체 동작은 PX4/제품 프로필로 고정한다. 표의 symbolic 상태는 PX4 custom_mode나 wire enum 숫자가 아니다.
하나의 `ready` boolean으로 장비 활성·항법 정상·원격 권한·물리 정지를 모두 표현하지 않는다.

## 7. 기본 장비 기능

### R-15 엔진·추진

- `BADA_ENGINE_CONTROL.throttle`은 uint16 0~1000의 기존 출력 요청이다. 목표 RPM이 아니다.
- 목표 RPM 기능은 지원 ECU·제어 루프·단위·한계가 합의된 별도 capability에서만 제공한다.
- 시동/정지/공회전/출력 0/추진 해제/클러치 중립은 별도 동작이다. 출력 0의 실제 의미는 D07에서 고정한다.
- 엔진별 최소/최대·ramp·온도/압력·RPM 보호·동작 interlock·작동 중 방향 전환 조건을 프로필로 조회한다.
- 실제 RPM·스로틀 위치·기어·온도·유압·연료·ECU 고장을 원천 freshness와 함께 보고한다.
- 전동 추진은 signed 회전/역추진 capability, ESC 고장·전류·온도·한계·좌우 추진기를 정의한다.

### R-16 기어·클러치

F/N/R은 logical 이름이고 ECU wire 값은 장비별 고정 매핑이다. 요청 기어와 실제 피드백을 구분한다.
전진↔후진 전환의 중립 경유·RPM/출력/속도 한계·대기·최대 시간·센서 미지원 정책을 정의한다.
전환 중 출력 명령이 들어와도 interlock을 우회하지 않는다. 실패/불일치 시 장치별 대응과 보고가 필요하다.
ACK는 실제 기어 체결 확인이 아니다. 피드백이 없으면 `확인 불가`를 명시한다.

### R-17 조향·러더

기본 범위는 단일 축이고 capability로 다축을 확장한다. axis 0/1의 선박 실제 장치 대응을 배치표로 고정한다.
목표 deg를 기존 normalized 값으로 변환할 때 음수는 `angle / abs(min_angle_deg)`, 양수는 `angle / max_angle_deg`를 사용한다.
설정은 `min < 0 < max`, 유한수·유효 보정·활성 revision을 검사하고 범위 밖 값은 기본적으로 거부한다.
clamp를 허용하는 제어 프로필은 적용값·제한 이유를 보고한다. 조용히 제한하고 원래 목표를 적용한 것처럼 보고하지 않는다.
실제 각도, encoder count, 속도, following error, limit·CiA402·EtherCAT 상태와 측정/추정 여부를 보고한다.
중앙 복귀는 이동 명령이다. QUICK_STOP/HALT/DISABLE와 구분하고 정지 요청을 angle 0으로 대체하지 않는다.
정상 상태에서 자동 re-enable, fault reset 후 자동 이동, 준비 전 목표 queue는 기본 비활성으로 한다.

### R-18 묶음 목표와 출력 장치

추진·기어·조향의 일관된 적용이 필요한 운항은 하나의 제어 epoch/cycle의 묶음 목표를 검증한다.
선박 전체 검증 실패 시 일부만 적용할지 전체 거부할지를 operation별로 고정한다.
여러 CAN/EtherCAT 장비에 논리 묶음을 보낸 것이 물리 동시 적용을 보장하는 것은 아니다. 장치별 적용 sequence·결과를 보고한다.
GPIO/펌프/등화/작업 장비는 채널 기능·극성·power-on default·상호배제·읽기 피드백·권한 scope를 정의한다.
출력 latch 값과 실제 장비 동작 확인을 구분한다.

## 8. 수동·보조·자율운항

### R-19 수동 조종

GCS/RC 입력은 PX4가 해석한 뒤 badaMux의 장비 목표로 변환한다. 정상 운항에서 입력 UI가 ECU/러더 메시지를 우회 송신하지 않는다.
조향/추진 축·반전·deadband·중립·slew·유효 범위·역추진·deadman 지원을 프로필로 고정한다.
가상/물리 입력 상실, 앱 background, focus loss, 화면 회전, RC override, 기체 변경의 처리와 타이머를 정의한다.
장비 점검은 별도 maintenance lease에서 작은 목표와 실제 피드백을 사용한다. 데모는 실제 출력 경로와 격리한다.

### R-20 보조 운항·Go To·위치 유지

방위 유지·속도 유지·Go To·위치 유지·복귀는 지원되는 PX4 모드/명령을 사용한다.
목표 frame·위경도·고도 wire 값·속도·도착 반경·최대 이동 거리·종료 후 동작을 명시한다.
보트 UI에서 고도를 숨겨도 PX4가 요구하는 메시지 필드를 임의로 제거하거나 0으로 치환하지 않는다.
위치 유지 기능은 허용 반경·항법 품질·추진 능력·최대 표류/노력·실패 대응이 있어야 지원한다.
중립은 위치 유지가 아니며, RTL은 접안 완료나 안전한 항로의 보증이 아니다.

### R-21 항로·작업 계획

GCS↔PX4 계획 전송은 기존 MAVLink Mission Protocol을 우선 사용한다. badaMux에 WP 파일/미션 엔진을 중복 구현하지 않는다.
계획 검증은 좌표·명령·속도·도착 반경·운항구역·설정/지도 revision·선회/감속 가능성을 포함한다.
완전히 검증·수락된 계획만 활성화하며 실패한 업로드가 실행 중 계획을 부분 교체하지 않게 한다.
표준 opaque mission ID를 제품 hash/revision과 혼동하지 않고 별도 제품 manifest가 필요하면 명시적으로 협상한다.
실행 중 경로 갱신은 지원 PX4의 교체 semantics와 safe switch point를 검증한 경우에만 허용한다.

### R-22 미션 수명

시작은 계획 일치·필수 항법·전원·장비·제어권·운항구역·비상 상태를 확인한다.
일시정지는 경로 진행을 멈추는 의도이며, 후속 중립/위치 유지/기타 동작은 제품 프로필로 결정한다.
재개는 마지막 목표를 그대로 되살리는 동작이 아니다. 현재 위치·계획 revision·다음 항목·경로 안전성을 재검사한다.
완료는 항로 완료, 목표 도달, 작업 완료, 추진 정지, 회수/접안을 별도 상태로 보고한다.
종료/abort와 재개 가능한 pause를 구분하며, 자동 재시작은 별도 승인된 정책 없이는 금지한다.

### R-23 복귀·운항구역·에너지

Home/복귀점·진입 항로·도착 반경·도착 후 동작을 정의한다. 직접 RTL과 검증된 경로 귀환을 구분한다.
폴리곤/제외 구역/경계 기준·지도 revision·경로 선분·선회 구간을 검증한다. 육지/금지구역을 가로지르는 복귀를 안전 복귀로 표시하지 않는다.
배터리/연료 부족 시 reserve·귀환 가능 추정·전원 센서 품질·추정 불확실도·오래된 데이터의 정책을 고정한다.
추정값은 보장값이 아니며 항로 변경 시 다시 계산한다.

### R-24 선택적 고급 자율운항

선내 컴퓨터를 사용하는 경우 PX4와 planner mode·liveness·목표 horizon·frame·setpoint 계약을 고정한다.
일반 PX4 offboard 예제의 비행기/멀티로터 조건이나 2Hz 기준을 무인선 장비 루프에 그대로 적용하지 않는다.
실제 PX4 빌드의 모드 지원·liveness 요구·loss action을 별도로 검증한다.

| 기능 | 필요한 정보와 완료 의미 |
| --- | --- |
| 경로계획/재계획 | plan/revision, 유효 horizon·frame·환경 revision, 속도/선회/감속 제약, 실패 이유, 전환 시점 |
| 장애물·AIS | 센서/track ID, 측정 시각·age·frame·위치/속도·크기·공분산/품질·출처. AIS와 센서 관측을 무조건 같은 확정 장애물로 취급하지 않음 |
| 회피 | 위험·선택 행동·허용 구역·주경로 복귀 조건, 계획기/인지 상실 시 대응. 통신 지원만으로 항법 규칙 준수나 충돌 방지 성능을 주장하지 않음 |
| 측량·작업 | 측선/작업 segment, 이동/작업 구간, sensor trigger·샘플 시각/위치·완료/누락 구간·재개점 |
| 접안 | 목표 선석·접근 경로·센서/상대좌표 품질·단계·abort 조건·최종 체결/추진 상태. RTL과 별도 capability |
| 협동/다중 선박 | 각 선박의 독립 lease·계획·네트워크 식별, 작업 할당 revision·충돌 중재. 기본 M2 범위 아님 |

바람·조류·수심·선체 누수·빌지/펌프 등 선택 센서는 측정/추정·참조 기준·설치 위치·age와 함께 보고한다.
실측 조류와 추정 조류, 겉보기 바람과 진풍을 구분하며 센서가 없는 값을 계산 가능하다고 가정하지 않는다.
누수·화재·해치·펌프 등의 상태를 모드나 배터리 정상으로 대체하지 않는다. 관련 장치 capability가 있을 때만 사용한다.

## 9. 보호·장애·복구

### R-25 원인별 대응

| 사건 | 탐지 주체 | 요구되는 동작 |
| --- | --- | --- |
| GCS 링크 상실 | PX4 | MANUAL/점검 입력은 만료. AUTO는 계속/중단/복귀 중 승인된 정책 적용. 무조건 장비 전원 차단하지 않음 |
| PX4↔badaMux heartbeat 상실 | badaMux | 신뢰한 제어 노드의 생존 상실 감지, lease/목표 해제와 장치별 loss action |
| heartbeat 정상·장비 목표 중단 | badaMux | 독립 setpoint TTL로 탐지. heartbeat가 오래된 출력을 유지시키지 않음 |
| 원천 CAN/ECU/피드백 상실 | badaMux/PX4 | 필드 무효/만료, 장치별 보호. 마지막 값을 최신으로 재발행하지 않음 |
| EtherCAT/WKC/축 고장 | badaMux | 영향 축·공통 버스·진행 작업 구분, 합의된 정지/제한, 실제 결과와 부분 실패 보고 |
| GNSS/방위/위치 추정 상실 | PX4 | 기능별 항법 조건에 따른 AUTO/HOLD/RETURN 제한. 무조건 RTL로 전환하지 않음 |
| planner/지각/외부 setpoint 상실 | PX4/선내 컴퓨터 | 합의된 onboard fallback, 만료된 trajectory 폐기 |
| reset·brownout·watchdog | 각 노드 | 새 boot/session, 출력 boot-safe, 원인 기록, 자동 운항/목표 복원 금지 |
| 물리 E-stop | 하드웨어/badaMux/PX4 | 하드웨어 정의 동작·latch·피드백, 원격 reset 제한, 자동 재활성 금지 |

loss action은 엔진 시동 상태·기어·러더·추진 방식별로 정의한다. 전원 차단이 항상 안전하다고 가정하지 않는다.
watchdog은 보호 스레드가 실행된다는 이유만으로 feed하지 않고 필수 제어/버스 작업의 생존 조건을 검사한다.

### R-26 정지 계약

임무 중단, 추진 출력 0, 기어 중립, 엔진 정지, 서보 QUICK_STOP, 위치 유지, 소프트웨어 안전 정지,
물리 비상정지를 별개 operation으로 다룬다. 안전 정지는 장비별 결과를 합산한 상태를 보고한다.
서보만 ACK 수락·엔진 피드백 없음이면 전체 정지 성공이 아니다.
무응답/일부 실패 상태에서 경고를 유지하고 원격 재시도/현장 개입 경로를 명시한다.
해제는 원인 제거·권한·현장 조건 확인 후에만 가능하며 해제 자체로 엔진/조향 목표를 복원하지 않는다.

### R-27 설정·보정·업데이트

읽기와 쓰기·runtime apply와 영속 저장을 분리한다. 쓰기는 base revision·검증·lease·장비 상태를 확인한다.
다필드 설정은 검증 후 원자적 논리 적용과 readback을 제공한다. 저장 실패/전원 손실 때 마지막 유효 설정으로 복구한다.
보정은 최대 이동·축·절차·중단/복귀·제한·실제 센서 확인이 있어야 하며 임의의 끝단 이동을 기본으로 하지 않는다.
펌웨어/설정 업데이트는 유지보수 상태·제품 무결성 검사·버전 호환·부팅 실패 복구를 요구한다.
통신 명세에 업데이트 지원을 기재한 것으로 OTA 구현/승인이 완료된 것으로 보지 않는다.

## 10. 주기·TTL·대역폭

### R-28 실험실 제안 프로필

아래 수치는 벤치/SIL 시험을 시작하기 위한 제안이며 배포용 기본값·장비 성능 보장이 아니다.
생산 프로필은 제동/표류·장비 응답·링크 지연/손실·bus 부하를 측정해 D09에서 확정한다.

| 그룹 | 제안 주기 | 별도 시간 기준 |
| --- | --- | --- |
| 노드 HEARTBEAT | 1Hz | 생존 상실 3,000ms 제안. setpoint 유효성 대신 사용 금지 |
| PX4→badaMux 묶음 목표 | 20Hz | 최대 명령 age/TTL 300ms 제안 |
| 장비 목표/실제 요약 | 10Hz | 원천 age 500ms 한도 제안, 필드별 검증 필요 |
| 엔진 저속 상태 | 5Hz | 온도 등 필드의 독립 sample age |
| 항법 상태 GCS 출력 | 5~10Hz | 센서/추정 품질과 원천 age 별도 |
| 안전/제어권 변화 | 즉시 이벤트 + 5Hz snapshot | 이벤트 유실 시 snapshot/재조회로 복구 |
| 진단·카운터 | 1Hz 또는 요청 | 운항 명령을 지연시키지 않는 낮은 우선순위 |
| lease 갱신 | 5Hz | lease 수명 1,500ms 제안. 목표 TTL과 독립 |
| 단발 ACK | 수신 처리 후 응답 | 첫 응답 1,000ms, operation 전체 한도는 동작별 확정 |

필수 파라미터는 `T_command_max_age`, `T_lease`, `T_heartbeat_loss`, `T_source_stale[group]`,
`T_ack`, `T_operation[op]`, `T_retry_horizon`, `W_dedupe`, `T_recovery_stable`, 지연/시계 불확실도 예산이다.
이 값들은 모두 장비·부하·속도 프로필과 버전으로 묶고 session 중 조용히 바꾸지 않는다.
명령 TTL은 하나의 정상 주기보다 길고 위험 분석의 최대 반응 시간보다 짧아야 하며 jitter·loss 허용을 함께 검증한다.

### R-29 대역폭·역압

MAVLink 2는 payload 최대 255바이트다. 기본 프레임 overhead 12바이트, 서명 시 13바이트가 더해진다.
payload 크기는 XML 필드 정렬·확장 필드·v2 truncation·생성 코드로 확인한다. 논리 context를 전부 더한 큰 payload를 무조건 하나에 넣지 않는다.
UART 8N1 예산은 `Σ(rate_hz × (payload_bytes + overhead_bytes) × 10)` bit/s로 계산한다.
예: 80바이트 payload를 서명해 20Hz로 보내면 21,000bit/s로, 115,200bit/s 한 방향 용량의 약 18.2%다.
이 수치는 한 stream 예시다. ACK·heartbeat·재전송·진단·다른 stream과 여유를 포함해야 한다.
전이중 UART는 방향별로, 반이중/공유 무선은 전체 점유율로 따로 계산한다.

보호·소유권/만료·정지·명령 응답을 우선하고 고속 목표는 최신값 위주로 보낸다.
과부하 때 영상/로그/진단 빈도를 줄이며 실패·drop·queue depth·최악 지연을 보고한다.
MAV_CMD_SET_MESSAGE_INTERVAL 수락만으로 실제 rate를 충족했다고 판단하지 않는다.

## 11. 인증·관측·승인

### R-30 인증·replay·감사

CRC는 오류 검출이고 발신자 인증이 아니다. MAVLink 서명 검증, endpoint 권한·lease·대상·세션 검사를 별도로 수행한다.
서명은 메시지 출처/무결성 검증이며 기밀성을 위한 암호화 전송과 구분한다.
signed 링크의 key provision·rotation·분실/재부팅·시계 상태, unsigned 정비 예외와 적용 범위는 D13에서 확정한다.
정상 운영에서 인증 실패·replay·예전 lease/boot 목표를 거부하고 횟수를 기록한다.
파일·설정 hash는 무결성 식별일 뿐 발신자 권한이나 악성 변경 방지의 전부가 아니다.

로그에는 node/boot/session/request/sequence/operation/instance, 요청·적용·실제값, 유효성/age,
원인·진행/최종 결과·지연·버전·설정 revision을 연결한다. 비밀키/lease 비밀·개인 식별정보를 평문 로그에 넣지 않는다.
이벤트는 ID·발생/해제·심각도·원인·sample을 포함하고, 유실 검출·마지막 snapshot·페이지 재조회로 복구한다.
알 수 없는 fault bit/code를 지우거나 정상으로 해석하지 않는다.

## 12. 주요 운용 시퀀스

### 최초 연결과 수동 점검

1. GCS/PX4/badaMux 발견 → 주소·boot·프로토콜/XML·capability·장비 inventory 확인.
2. 활성 설정·센서/장비 상태·원천 age·안전 latch 확인. 인증/시간 품질·설정 불일치면 읽기 전용 유지.
3. 상위 제어권과 PX4→badaMux 장비 lease를 순서대로 확보. 직접 점검은 AUTO와 격리된 정비 역할 사용.
4. 지원 장비의 enable 요청 → 처리 결과와 실제 enabled 상태 확인.
5. 새 epoch/sequence의 작은 유효 목표를 전송하고 적용 sequence·실제값·제한/오류를 대조.
6. 점검 종료 → 장치별 종료/정지 결과 확인 → lease release. 화면을 다시 열어도 마지막 목표를 재실행하지 않음.

### WP 자동운항과 수동 개입

1. 표준 mission으로 완성 계획을 전송하고 plan ID/revision·속도·구역·장비 한계 검증 결과 확인.
2. PX4가 필수 항법·전원·장비·소유권을 확인한 뒤 AUTO 실행.
3. PX4는 연속 장비 목표를 badaMux에 보내고, badaMux는 TTL/lease/한계를 검사해 실제 상태를 보고.
4. RC/현장 개입 시 PX4가 기존 자동 목표를 취소하고 승인된 우선권으로 전환. 옛 epoch는 badaMux에서 거부.
5. 자동 재개는 별도 요청·현재 위치·계획·지원 상태 재검증을 거친다. 수동 레버가 남아 있는 상태의 혼합 제어 금지.

### heartbeat는 정상인데 장비 목표가 끊김

1. badaMux는 신뢰 PX4의 heartbeat와 별도로 최신 장비 setpoint의 age/TTL을 검사.
2. TTL 초과 → 목표 폐기·해당 장비 loss action·원인 이벤트/상태 보고.
3. PX4는 장비 ready/적용 상태 상실을 반영하고 운항 상태를 갱신.
4. 새 heartbeat/목표 하나로 이전 운항을 자동 복원하지 않고 원인·lease·enable/복구 조건을 재검사.

### 지상 링크 단절 후 재연결

1. PX4는 GCS 상실을 판단하고 MANUAL/정비/AUTO별 승인 정책 적용.
2. AUTO 계속이 승인된 경우에도 PX4↔badaMux 저수준 제어·항법·장비 보호는 계속 필요.
3. GCS 복귀 → 읽기 전용 상태/계획/owner/epoch/result snapshot 동기화.
4. 미완료 요청은 원래 ID로 결과를 조회. 소유권·계획 revision이 맞는지 확인한 뒤 새 사용자 의도를 허용.

### 선내 계획기 재계획

1. C는 최신 항법·관측·지도/구역·장비 제한 revision으로 새 trajectory를 제안.
2. PX4는 권한·frame·sample age·horizon·제약·switch point를 검사하고 채택/거부를 보고.
3. 채택 이후에도 C/PX4는 liveness·원천 센서·장비 상태를 확인하고 만료 시 onboard fallback 적용.
4. GCS에는 `제안됨/채택됨/실행 중/거부/중단`을 분리해 보고한다. 화면의 경로 그림만으로 실행을 확인하지 않는다.

## 13. 확정 절차

1. [카탈로그](MESSAGE_CATALOG.md)의 E/S/P별 지원 행렬과 실제 노드·버전을 기록한다.
2. [인수 계획](ACCEPTANCE_TEST_PLAN.md)의 D01~D18을 합의하고 선택 기능·장비 프로필을 고정한다.
3. ID·enum·payload 배치를 별도 XML 변경으로 리뷰하고 패킷 교차 시험을 만든다.
4. M0→M1→M2→선택 M3 순서로 시험 증거와 알려진 제한을 기록한다.
5. 문서 승인·펌웨어 구현·앱 표시·벤치·HIL·실선 인수를 각각 별도 판정한다.

표준 근거는 [MAVLink command](https://mavlink.io/en/services/command.html),
[mission](https://mavlink.io/en/services/mission.html), [라우팅](https://mavlink.io/en/guide/routing.html),
[서명](https://mavlink.io/en/guide/message_signing.html), [직렬화](https://mavlink.io/en/guide/serialization.html),
[PX4 stream](https://docs.px4.io/main/en/mavlink/streaming_messages),
[PX4 offboard 예제](https://docs.px4.io/main/en/ros2/offboard_control)를 참고한다.
이 문서의 신규 lease/TTL/상태·제품 기능은 표준에 이미 존재한다고 주장하지 않는 제품 설계 요구사항이다.
