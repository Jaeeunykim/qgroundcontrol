# badaMux 통신 명세 인수·검증 계획

버전 `0.1.0-draft` · 2026-11-17 · [동작 명세](COMMUNICATION_SPEC.md) · [메시지 카탈로그](MESSAGE_CATALOG.md)

이 문서의 시험은 모두 계획이며 이번 문서 작성에서 실행하지 않았다.
기존 BadaNavi 패킷/UI 시험이나 오래된 BadaMux 코드의 존재를 현재 제품의 통합·실선 합격으로 이전하지 않는다.

## 1. 승인 전 결정표

결정마다 담당자·합의일·근거 버전·값·허용 범위·검증 ID를 기록한다. 미확정을 펌웨어 기본값으로 조용히 대체하지 않는다.

| ID | 결정할 내용 | 기본 제안/현재 상태 | 완료 증거 |
| --- | --- | --- | --- |
| D01 | 제품 보드·firmware/PX4/GCS/XML/generator 커밋 | PX4 경유 구조 확인. 실제 배포 버전 미확정 | version manifest와 각 노드 정보 응답 |
| D02 | UART/네트워크·baud·flow control·CAN/EtherCAT 구성 | 실제 장비·배선 미확정. 과거 문서 baud를 자동 채택하지 않음 | 배치/포트 표·동시 부하 캡처 |
| D03 | system/component/device/axis 식별과 라우팅 | 선박별 유일성, 정상 능동 방송 금지 | 다중 선박·링크 시험 |
| D04 | 필수/선택 capability·legacy 지원 subset | M0/M1/M2 최소 범위와 P 메시지 채택 미확정 | E/S/P 구현 지원 행렬 |
| D05 | RC/현장/원격/자율운항 우선권·scope·lease | 두 계층 lease 제안, 실제 우선권 미확정 | 전환·만료·경쟁 명령 시험 |
| D06 | 엔진·전동 추진·기어·축·센서 실물 구성 | 초기 단일 러더, 다축은 capability. 실물 매핑 미확정 | ECU/드라이브 모델·장치 사전 |
| D07 | 출력 0·공회전·시동·정지·중립·QUICK_STOP·해제 | 별도 operation 유지, 실제 장치별 action 미확정 | 장치별 요청/실측 결과 |
| D08 | 각도·RPM·온도·좌표·부호·frame·원천 age·valid | 논리 단위 제안. legacy last_update와 온도 위치 미확정 | 입력→PX4→GCS 변환 표·교차 시험 |
| D09 | 주기·TTL·heartbeat/lease·시계 오차·지연 예산 | 동작 명세의 LAB 수치는 제안. production 수치 미확정 | 지연/손실/부하/표류·장비 응답 측정 |
| D10 | 출력·각도·RPM·변화율·기어 interlock·도달 판정 | 실제 장비·운항 profile 필요 | 한계/고장/기어 전환 시험 |
| D11 | 미션·고도 wire 필드·속도·pause/return/완료 | PX4 실제 명령/모드·도착 후 동작 미확정 | plan 왕복·실행 상태·실선 결과 |
| D12 | 선내 컴퓨터·offboard·planner·센서·horizon | M3 선택. 실제 기능/프로토콜 미확정 | PX4 지원 증거·시나리오/HIL |
| D13 | 인증·서명·key lifecycle·정비 예외·replay | CRC와 별도, 비밀 저장 방식 미확정 | 인증/재부팅/재전송 시험 |
| D14 | 요청 ID·dedupe window/journal·재시도·취소 | command/setpoint/mission 분리. 저장/메모리 상한 미확정 | 중복/응답 유실/reset 시험 |
| D15 | 설정·보정·영속 저장·업데이트·rollback | 유지보수 절차·제품 무결성 정책 미확정 | 전원 중단·readback·복구 시험 |
| D16 | GCS/PX4/Bada/센서/planner별 loss action·복구 | 원인·운항 모드별 분리. 실제 정책 미확정 | 독립 장애 주입 행렬 |
| D17 | 물리 E-stop·부분 정지·현장 복구·승인 역할 | 하드웨어 정의/장치 결과 필요. 원격 reset 권한 미확정 | 실회로·장치별 측정·latch 시험 |
| D18 | 운영 범위·환경·장시간·출시 범위·인수 책임 | 최대 속도/기상/수역·반복 횟수·기간 미확정 | 승인 profile·시험 보고·known limitations |

기본 M2 출시를 위해 선택 M3를 모두 구현할 필요는 없다. 미지원 capability는 숨기거나 명확하게 표시한다.
필수 M0~M2 항목의 미확정/실패는 해당 단계 출시를 막는다. 장비가 없는 선택 기능은 `미지원`으로 기록한다.

## 2. 시험 환경과 산출물

| 환경 | 검증할 것 | 대체할 수 없는 것 |
| --- | --- | --- |
| 정적 schema/생성 | ID·타입·CRC·include·payload·bitmask·언어 간 해석 | 실제 장비 기능 |
| 단위·SIL | parser·상태·idempotency·lease·시간·가짜 ECU/드라이브 | ECU·서보의 물리 동작 |
| PX4 SITL | 지원 모드·항로·명령·거부·계획/단절 시나리오 | 실제 badaMux 버스/추진·조향 |
| BadaMux 벤치 | 실제 board·CAN/EtherCAT·watchdog·한 축/ECU | 실선 조류·기상·운항 성능 |
| HIL/계류 | PX4↔badaMux↔장비 왕복·부하·정지·기어·failover | 자유 운항·복귀·회피·접안 인수 |
| 제한 수역 실선 | 항로·수동 개입·중단/재개·복귀·환경별 제약 | 미시험 환경·제품 기능의 포괄 보증 |

장비를 움직이는 시험은 승인된 장비 프로필·현장 담당·독립 정지 수단·격리/계류 조건을 갖춘 환경에서 수행한다.
시험 장치와 생산 선박 네트워크를 구분하고, 모의 시험에서 실제 broadcast actuator 명령을 전송하지 않는다.

각 시험 결과에는 다음을 포함한다.

- 시험 ID, E/S/P 메시지·R 요구사항, 단계 M0~M3, 실행자·날짜·장비·환경.
- GCS/PX4/badaMux/XML/generator/config/map/plan revision·capability.
- 입력 조건·fault injection·예상 상태·시간/오차 기준·실제 결과와 판정.
- 요청 ID/session/epoch/sequence와 송신·수신·적용·원천 sample·물리 응답 시각.
- 로그·패킷·영상/계측 증거의 위치·hash, 실패/미실행 사유·재시험·남은 제한.

## 3. M0: 발견·버전·상태·읽기 전용

| 시험 ID | 요구·메시지 | 입력·상황 | 통과 기준 |
| --- | --- | --- | --- |
| BM-T01 | R-03~05, E/S/P01~04 | 실제 XML에서 C/C++ 및 사용하는 앱 언어 생성·상호 decode | field/type/ID/CRC·확장 필드·payload가 일치. 선언과 실행 지원 별도 기록 |
| BM-T02 | R-03, 전체 | byte 분할/병합·noise·잘린 frame·CRC 오류·unknown flag/ID | parser 복구·계수 기록, 잘못된 데이터로 제어/정상 상태 갱신 없음 |
| BM-T03 | R-04~05, P01~03 | 다른 major/minor·XML·재부팅·duplicate 주소·지원 안 되는 장비 | 호환/진단/제어 차단 정책 일치, 타 장치로 오인하지 않음 |
| BM-T04 | R-06~08, E05/E11/P15~18/P47~48 | 유효 0·NaN·sentinel·음수·한계·헤드/냉각수·비대칭 각도·선택 선체/환경 센서 | 물리량/참조 기준/부호 정확, 미지원/무효를 정상 0·누수 없음·배수 완료로 표시하지 않음 |
| BM-T05 | R-07, P15~19 | PX4 heartbeat/stream은 유지하고 ECU/러더 원천만 중단 | 원천 age 증가·field stale와 보호/표시 일치. 재발행으로 최신 처리하지 않음 |
| BM-T06 | R-08, C-STATE | clock offset/drift·미동기·wrap·재부팅 | 타 장치 boot time 직접 차감 없음, 불확실도/epoch 정책대로 invalid/stale |
| BM-T07 | R-04/29, P04/P45 | 선박 2대·장비 여러 index·중복/반이중 링크·rate 요청 거부 | 데이터 격리, 실제 수신률·drop/queue 확인, broadcast 제어 없음 |

## 4. M1: 제어권·수동·장비·정지

| 시험 ID | 요구·메시지 | 입력·상황 | 통과 기준 |
| --- | --- | --- | --- |
| BM-T08 | R-09/12~14, P05~08 | 두 조종자 동시 acquire·scope 겹침·RC/정비/자동 경쟁 | 승인된 하나의 소유자만 적용, 거부 이유·epoch 일치 |
| BM-T09 | R-08/11~12, P06/P11 | lease renew는 유지하되 setpoint 중단·역순·중복·지연 | setpoint TTL 독립 동작, duplicate가 유효시간 연장하지 않음 |
| BM-T10 | R-11~13, P07/P11 | 양도/회수 직전·직후 이전 epoch 패킷 주입 | 옛 소유자 출력 적용 없음, 새 입력 준비와 전환 결과 일치 |
| BM-T11 | R-09~11, P10/P12~14 | ACK 유실·IN_PROGRESS·같은 ID 다른 payload·동작 중 취소 | 동일 요청 중복 실행 없음, 최종/unknown/이미 완료 구분 |
| BM-T12 | R-11/25, P12/P13 | 비멱등 작업 실행 직후 수신기 reset·journal 상실 | 자동 재시동/재기어/재보정 없음, 결과 조회·현장 확인 경로 |
| BM-T13 | R-15/18, E01/P11/P12/P16 | 출력 0/중간/상한·범위 밖·ramp·목표 RPM 비지원 | 합의한 단위·거부·적용값·실제 RPM 일치, RPM과 % 혼동 없음 |
| BM-T14 | R-16, P13/P15 | F→N→R, 높은 RPM·출력 유지·기어 센서 상실·불일치 | interlock 우회 없음, 실제 체결/실패/확인 불가와 timeout 구분 |
| BM-T15 | R-17, E02~10/P14/P18 | 활성 설정 조회→enable→작은 ±각도→중앙·한계 | 비대칭 min/max와 부호 일치, command ACK와 실제 타각 도달 구분 |
| BM-T16 | R-17/25, E05~07/P18/P19 | WKC·following error·limit·feedback 상실·reset | 해당 축/버스 오류 보고, 자동 목표 queue/re-enable/이동 없음 |
| BM-T17 | R-18, P11/P15 | 묶음 중 한 장치 invalid/timeout, 다른 장치 정상 | 전체 거부/부분 허용 profile 준수, 물리 동시 적용으로 잘못 보고하지 않음 |
| BM-T18 | R-18~19, P20/P21 | GPIO 극성·금지 조합·피드백 없음·점검 이탈 | 출력과 actual 구분, 장비 역할/권한·interlock·deactivation 적용 |
| BM-T19 | R-19, S05/P11 | 조이스틱 중심·역전·범위·deadman·장치 분리·앱 background | 중립/입력 상실·lease/TTL 정책, 이전 입력 재사용·우회 송신 없음 |
| BM-T20 | R-25~26, P37/P38 | 출력 0/기어 중립/서보 quick stop/엔진 stop 각각 | 동작별 실제 효과·시간·상태가 profile과 일치 |
| BM-T21 | R-26, P37/P38 | 일부 장치 무응답·부분 정지·물리 E-stop·reset | 전체 성공 오표시 없음, latch·현장 복구·자동 재활성 금지 확인 |

## 5. M2: 항로·자율운항 기본 기능

| 시험 ID | 요구·메시지 | 입력·상황 | 통과 기준 |
| --- | --- | --- | --- |
| BM-T22 | R-20/22, S06/P24/P27 | Go To·방위/속도 유지·위치 유지 지원/미지원·고도 없음 | 실제 PX4 계약대로 실행/거부. 미지원 기능을 UI나 ACK로 성공 처리하지 않음 |
| BM-T23 | R-21, S07/P25/P26 | 항로 업/다운·중간 단절·역순·오류 항목·동시 전송·빈 계획 | 완성 전 활성 계획 교체 없음, revision/항목/속도/파일 왕복 일치 |
| BM-T24 | R-21/23, P23/P26 | 금지구역·육지 횡단·선회 여유·과속·잘못된 frame·한계 변경 | 유효 제약 위반 거부, 변경된 constraint revision 재검증 |
| BM-T25 | R-22, S06/S07/P27/P28 | 시작→진행→pause→재개→완료, 같은 요청 중복 | 실제 모드·항목·작업·추진 상태 일치. pause/abort/완료를 구분 |
| BM-T26 | R-12/19/22, P07/P27 | AUTO 중 현장/RC 수동 개입 후 자동 재개 | 우선권/epoch 적용, 현재 위치·계획 재검증 없이 옛 목표 복원 안 함 |
| BM-T27 | R-23, S08/P24/P29 | RTL 직접 경로 위험·Home 변경·복귀 중 단절 | 승인 return policy만 실행, 접안/정지/안전 경로 오표시 없음 |
| BM-T28 | R-23/25, P22/P29 | GNSS·heading·energy 오류/stale·reserve 부족 | 기능별 제한/대응, 위치 없는 무조건 RTL 없음 |
| BM-T29 | R-25, P04/P08/P11 | GCS 단절, A↔B 단절, source bus 단절을 독립 주입 | MANUAL/AUTO/정비 모드별 승인 loss action이 각 수신 노드에서 실행 |
| BM-T30 | R-08/25/28~29 | 고속 이동 profile·손실/jitter·stream flood·로그 전송 | 최악 지연·TTL/보호 예산 충족, 오래된 출력 queue 누적 없음 |

## 6. M3: 선택적 고급 자율운항

| 시험 ID | 요구·메시지 | 입력·상황 | 통과 기준 |
| --- | --- | --- | --- |
| BM-T31 | R-24, S12/P30/P31 | planner liveness 정상/중단·trajectory 불완전/만료·frame 오류 | PX4 지원 계약과 loss action 일치, 불완전/오래된 경로 미채택 |
| BM-T32 | R-24, P22/P23/P30~33 | 움직이는 장애물·센서 상실·AIS/센서 불일치·stale track | uncertainty/age·출처 구분, 검증된 경로만 채택, 판단 실패가 정상 회피로 보이지 않음 |
| BM-T33 | R-21/24, P31/P33 | 운항 중 재계획·지도/운항구역 revision 변경·제약 위반 | safe switch point·제약 검증·주항로 복귀 정책, 장비 한계 우회 없음 |
| BM-T34 | R-24, P21/P35 | 측량 작업 중단·샘플 유실·재개·sensor offset | 시각/위치 대응·작업/이동 구분·누락 구간·재개점이 실제와 일치 |
| BM-T35 | R-24, P34 | 접안 접근·상대 위치 품질 저하·abort·최종 체결 미확인 | 단계별 gate/abort·추진/체결 피드백·실패 복구 일치. RTL로 완료 대체 안 함 |
| BM-T36 | R-04/12/24, P36 | 다선박 task 충돌·할당 변경·duplicate system ID | 선박별 lease/계획 격리, 잘못된 선박의 출력 적용 없음 |

## 7. 공통 보안·정비·복구·관측

| 시험 ID | 요구·메시지 | 입력·상황 | 통과 기준 |
| --- | --- | --- | --- |
| BM-T37 | R-04/30, 능동 메시지 | 인증 실패·출처 위조·unsigned/replay·키 변경·재부팅 | 정책대로 거부·계수 기록, CRC 성공만으로 제어 허용 안 함 |
| BM-T38 | R-27, P40/P46 | stale base revision·부분 설정 오류·저장 중 전원 단절 | 논리 원자성·영속 실패·readback·이전 유효 설정 복구 |
| BM-T39 | R-27, P41 | 보정 중 limit·취소·권한 상실·재부팅 | 제한된 절차·독립 중단·미완료 보정 무효·자동 재활성 없음 |
| BM-T40 | R-27, P42 | 운항 중 업데이트·잘못된 image·중간 단절·부팅 실패 | 차단/무결성/rollback·운항 금지 gate, 기존 설정/이미지 복구 |
| BM-T41 | R-29~30, P39/P43/P44 | event gap·로그 공간 부족·대량 진단·재조회 | snapshot 복구·drop 표시·제어 우선권, 비밀키/민감 token 로그 제외 |
| BM-T42 | R-25/28, 전체 | 실제 watchdog·brownout·필수 제어/버스 thread 정지 | 단순 feed loop가 고장을 감추지 않고 boot-safe·원인·새 session 확인 |
| BM-T43 | R-05, E/S/P | legacy peer·기능 일부 누락·major mismatch | 읽기/진단과 운영 제어 승인 분리, 무시되는 lease/TTL로 운항 허용 안 함 |
| BM-T44 | R-01~30, 선택 기능 | 승인 환경·속도·장시간·반복·온도/전원 profile | 사전 고정한 자원/반응/오류·운항 기준 충족. 미시험 범위 명시 |

모든 시험은 최소 한 개의 실패 사례도 포함한다. 문서 ID를 맞추기 위한 구현 복제형 단위 테스트를 만들지 않는다.
실제 사용하는 E/S/P subset과 장비 profile에 따라 시험을 parameterize하고 불필요한 중복을 합친다.

## 8. 단계별 출시 gate

| Gate | 필수 결과 | 승인 불가 사례 |
| --- | --- | --- |
| G0 문서/규약 | D01~D04·D08의 실제 버전·주소·필수 schema, E/S/P 지원 행렬 | XML 선언만 있고 수신 구현/타입/단위 미확인 |
| G1 읽기 전용 M0 | BM-T01~07 + snapshot·원천 age 증거 | 센서 단절을 정상 0/최신으로 표시 |
| G2 장비 제어 M1 | 관련 BM-T08~21·37~43, 장비별 한계·loss/stop·인수 담당 | lease/목표 TTL 부재, 부분 정지를 전체 성공 처리, 물리 시험 누락 |
| G3 기본 자율운항 M2 | BM-T22~30·44, M1 통과, 실제 PX4/실선 기본 운항 기록 | SITL 패킷만 통과·실선 이동/중단/복귀 미검증 |
| G4 선택 기능 M3 | 관련 BM-T31~36·44와 별도 센서/운영 범위 인수 | capability 없음·인지 실패·접안 미확인인데 기능 완료 표기 |

문서 승인, 개발 완료, 벤치 합격, HIL 합격, 현장 인수, 제품 출시 승인을 각각 분리한다.
출시는 구현 코드·설정/이미지·문서 버전·장비 profile·테스트 결과·known limitations를 함께 고정한다.
다른 선박·ECU·러더·PX4 버전으로 바뀌면 영향 분석과 관련 gate 재검증이 필요하다.

## 9. 인수 기록 템플릿

```text
시험 ID / 관련 R·E·S·P:
기능 단계 / 필수 또는 선택 / 실행 상태:
실행일·담당자·장소·시험 환경:
GCS / PX4 / badaMux / XML / 생성기 커밋:
장비 모델·인스턴스·배선·주소·센서 출처:
config / operation profile / map / plan revision:
주기·age·TTL·lease·시계 오차·최대 허용 지연:
도달 오차·지속 시간·operation timeout:
입력·fault injection·재현 단계:
예상 상태 / 실제 상태 / 장비별 물리 결과:
요청·응답·적용 sequence·sample·시각 추적:
로그·패킷·영상·계측 파일 및 hash:
통과 / 실패 / 차단 / 미실행 / 미지원:
제한·현장 조치·재시험 항목·승인자:
```

이 명세 패키지는 문서 형식·내부 링크·기존 XML 필드 대조만 수행할 수 있다.
실제 통신 구현·장비·운항 시험 결과는 실행한 뒤 위 템플릿으로 추가한다.
