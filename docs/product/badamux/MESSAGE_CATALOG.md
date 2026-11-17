# badaMux 메시지 카탈로그·필드 사전

버전 `0.1.0-draft` · 2026-11-17 · [동작 명세](COMMUNICATION_SPEC.md) · [인수 계획](ACCEPTANCE_TEST_PLAN.md)

## 1. 읽는 방법

- E: 고정 XML에서 선언 확인. 장비 실행 지원을 뜻하지 않는다.
- S: 표준 MAVLink 재사용 후보. 실제 PX4 버전·stream·capability 확인 필요.
- P: 신규 제품 논리 메시지 제안. 실제 wire 이름/ID/enum 값/CRC는 미할당.
- G=GCS, A=PX4, B=badaMux, C=선내 자율운항 컴퓨터.
- `uint8/16/32/64`, `int8/16/32/64`, `float32`, `bytes[N]`는 제안 데이터 타입이다.
  symbolic enum/bitmask 값은 별도 XML 리뷰에서 고정한다. C 구조체 padding을 wire layout으로 사용하지 않는다.
- 메시지 하나의 수신, 실행 접수, 실제 장비 적용, 물리 목표 도달을 구분한다.
- 신규 schema의 field set은 필수/선택·payload 분할·metadata를 검토할 논리 계약이다.
  전체 context를 모든 표준/기존 메시지에 뒤에 붙인다는 뜻이 아니다.

## 2. E: 기존 bada.xml 선언

기준은 [bada.xml f16192a](https://github.com/BadaProject/mavlink/blob/f16192a8737e30f994e80b23d71ed1e33e2e47d7/message_definitions/v1.0/bada.xml)다.
아래 이름·ID·타입은 이 스냅샷에서 확인했다. 화살표는 제품 목표 배치이며 XML 자체가 송신자를 강제하지 않는다.

| E ID | wire 메시지·숫자 ID | 목표 방향 | 용도·제한 |
| --- | --- | --- | --- |
| E01 | `BADA_ENGINE_CONTROL` 50001 | A→B | 출력 요청. target/session/TTL/ACK 없음, RPM/기어/엔진 시동 명령 아님 |
| E02 | `BADAMUX_SERVO_SETPOINT` 42000 | A→B | 축별 normalized 목표. 일반 목표마다 성공 ACK를 가정하지 않음 |
| E03 | `BADAMUX_SERVO_COMMAND` 42001 | A→B | ENABLE/QUICK_STOP 등 상태 전이. E08과 대응 |
| E04 | `BADAMUX_SERVO_CONFIG` 42002 | A→B | 보정·한계·profile 설정. runtime apply/영속 저장·권한 확인 필요 |
| E05 | `BADAMUX_SERVO_STATUS` 42003 | B→A | 실제/목표·CiA402·각도·고장. valid_fields와 last_update 의미 확인 |
| E06 | `BADAMUX_ETHERCAT_STATUS` 42004 | B→A | 버스·WKC·cycle·축 수. 장비별 정상과 별도 |
| E07 | `BADAMUX_SERVO_EVENT` 42005 | B→A | 사건·거부·오류. 개별 요청 성공 ACK가 아님 |
| E08 | `BADAMUX_SERVO_ACK` 42006 | B→A | command_id + request_msg_id + axis + 출처로 대응 |
| E09 | `BADAMUX_SERVO_CONFIG_REQUEST` 42007 | A→B | 활성/저장/default 설정 조회 |
| E10 | `BADAMUX_SERVO_CONFIG_STATUS` 42008 | B→A | 설정·revision 부재 한계·ACTIVE/저장 상태 확인 |
| E11 | `BADA_MANEUVERING_STATUS` 50004 | A→G 또는 합의된 B→A | 통합 RPM·온도·러더·오류. 원천 시각/valid/인스턴스 없음 |
| E12 | `BADA_SERVO_CONTROL` 50002 | legacy | unsigned 각도 배열. 음수 타각이나 E02와의 동일 의미를 가정하지 않음 |
| E13 | `BADAMUX_STATUS` 50003 | legacy | 기존 통합 상태. 다축 대응·단위/상태 의미 재확인 |

### E01 엔진 출력

| 필드 | XML 타입 | 의미·검사 |
| --- | --- | --- |
| throttle | uint16_t | 0~1000. UI 0~100%를 ×10 반올림. 범위 밖·NaN 입력 거부. 0의 장비 동작은 D07에서 확정 |

기존 BadaNavi의 합의는 출력 비율이다. 과거 BadaMux 검토 기록의 `MAV_CMD_USER_1` RPM 방식과 자동 호환되지 않는다.
현재 생산 펌웨어의 수신 경로를 확인하고 한 가지 승인된 운용 경로로 정리한다.

### E02 서보 목표

| 필드 | XML 타입 | 의미·검사 |
| --- | --- | --- |
| time_boot_ms | uint32_t | 발신자 boot time ms. 현재 선언만으로 오래된 명령 거부가 구현됐다고 판단하지 않음 |
| command_id | uint32_t | 앱 레벨 추적 ID. boot/session/lease가 없어 새 제품 요구와 차이 존재 |
| profile_velocity / profile_accel / profile_decel | 각각 uint32_t | CiA402 user units의 속도·가감속. 0은 기존 설정 유지이며 정지 아님 |
| target_normalized | float | [-1,1]. −1은 min_angle, 0은 center, +1은 max_angle |
| flags | uint16_t | NEW_SETPOINT=1, CHANGE_IMMEDIATELY=4, QUEUE_SETPOINT=8 |
| axis | uint8_t | LEFT=0, RIGHT=1, ALL=255. 실제 단일 축 대응은 설정. ALL 목표 송신은 승인하지 않음 |
| mode | uint8_t | KEEP=0, PP=1, CSP=8 선언. 실제 수신 모드 지원은 capability로 확인 |

활성 설정 조회 전 고정 ±30°/±35°를 적용하지 않는다. 정상 목표에 QUEUE를 기본 사용하지 않는다.
새 제품 제어 context를 강제할 adapter/수신 구현과 peer 협상이 없으면 legacy E02로 M1/M2 요구 충족을 선언하지 않는다.

### E03 동작·E08 응답·E07 이벤트

| 메시지 | 전체 필드: 타입 |
| --- | --- |
| E03 | time_boot_ms:uint32, command_id:uint32, flags:uint16, axis:uint8, command:uint8, mode:uint8, reason:uint8 |
| E08 | time_boot_ms:uint32, command_id:uint32, request_msg_id:uint32, result_param2:int32, axis:uint8, result:uint8, progress:uint8 |
| E07 | time_boot_ms:uint32, command_id:uint32, detail:int32, event_type:uint16, statusword:uint16, error_code:uint16, al_status:uint16, axis:uint8, severity:uint8, reason:uint8, error_register:uint8 |

E03 command 값은 QUICK_STOP=0, DISABLE_VOLTAGE=1, FAULT_RESET=2, ENABLE_OPERATION=3,
CLEAR_SETPOINT=4, HALT=5, RESUME=6이다. wire enum 추가·재배정 없이 의미를 보존한다.
E03 flags는 REQUIRE_OP_ENABLED=1, ALLOW_REINIT=2, CLEAR_PENDING_SETPOINT=4, REENABLE_AFTER_RESET=8이다.
FAULT_RESET와 REENABLE_AFTER_RESET는 운용상 별도 허용이 필요하며 자동 이동으로 이어지지 않게 한다.

E08 result는 ACCEPTED=0, TEMPORARILY_REJECTED=1, DENIED=2, UNSUPPORTED=3, FAILED=4, IN_PROGRESS=5다.
일반 `COMMAND_ACK`와 서로 다른 protocol이다. E08 ACCEPTED를 실제 타각 도달로 재정의하지 않는다.
진행률은 0~100이고 0은 미확인/해당 없음일 수 있다. 최종 결과·실제 상태는 별도로 판정한다.

### E04·E09·E10 설정

| 메시지 | 전체 필드: 타입 |
| --- | --- |
| E09 | time_boot_ms:uint32, command_id:uint32, request_flags:uint16, axis:uint8 |
| E04 | time_boot_ms:uint32, command_id:uint32, profile_velocity:uint32, profile_accel:uint32, profile_decel:uint32, quick_stop_decel:uint32, center_counts:int32, min_position_counts:int32, max_position_counts:int32, counts_per_degree:float, min_angle_deg:float, max_angle_deg:float, flags:uint16, axis:uint8, software_limit_mode:uint8 |
| E10 | time_boot_ms:uint32, command_id:uint32, profile_velocity:uint32, profile_accel:uint32, profile_decel:uint32, quick_stop_decel:uint32, center_counts:int32, min_position_counts:int32, max_position_counts:int32, counts_per_degree:float, min_angle_deg:float, max_angle_deg:float, status_flags:uint32, config_flags:uint16, axis:uint8, software_limit_mode:uint8 |

E09 request_flags는 ACTIVE=1, PERSISTED=2, DEFAULT=4이며 0은 ACTIVE 조회다.
E10 status_flags는 ACTIVE=1, PERSISTED=2, DEFAULT_USED=4, DIRTY=8, STORAGE_READY=16이다.
scale/한계·profile·storage의 실제 지원과 적용 시점은 장비별 확인이 필요하다.
기존 스냅샷에는 session·base_revision·트랜잭션·독립 epoch가 없어 새 설정 계약과 차이가 있다.

### E05·E06 실제 상태

| 메시지 | 전체 필드: 타입 |
| --- | --- |
| E05 | time_boot_ms:uint32, flags:uint32, valid_fields:uint32, last_update_ms:uint32, target_position_counts:int32, actual_position_counts:int32, actual_velocity:int32, target_angle_deg:float, actual_angle_deg:float, controlword:uint16, statusword:uint16, error_code:uint16, al_status:uint16, axis:uint8, mode_requested:int8, mode_display:int8, error_register:uint8 |
| E06 | time_boot_ms:uint32, flags:uint32, wkc_fault_count:uint32, max_cycle_overrun_us:uint32, last_reinit_ms:uint32, expected_wkc:int16, last_wkc:int16, cycle_ms:uint16, slave_count:uint8, active_axis_count:uint8 |

E05의 `last_update_ms`는 XML 설명상 timestamp 또는 age의 장비 정책 의존성이 있다. D08에서 하나의 의미로 확정해야 한다.
actual_angle이 러더 축 센서 실측인지 motor encoder 환산인지 문서화한다. CiA402 target reached 비트 하나를 선박 전체 완료로 쓰지 않는다.
flags/valid_fields/error_register의 미정의 비트는 보존하고 정상으로 지우지 않는다.

### E11·E12·E13 통합/legacy 상태

| 메시지 | 전체 필드: 타입 |
| --- | --- |
| E11 | engine_fault_flags:uint32, rudder_fault_flags:uint32, engine_rpm:float, engine_temperature:float, rudder_angle_deg:float |
| E12 | count:uint8, angles:uint16[4] |
| E13 | activation_flags:uint8, engine_temp:uint16, engine_rpm:uint16, engine_status:uint16, servo_angles:uint16[4], servo_speeds:int16[4] |

E11의 RPM은 실제값, 온도는 °C, 러더는 deg이며 음수 좌현·양수 우현이다.
온도 측정 위치는 XML만으로 충분하지 않다. 별도 device profile로 고정하거나 신규 표현을 선택한다.
engine_fault_flags는 COMMUNICATION=1, NOT_RUNNING=2, RPM=4, TEMPERATURE=8, EMERGENCY_STOP=16, CONTROLLER=32다.
rudder_fault_flags는 COMMUNICATION=1, FEEDBACK=2, TRACKING=4, PORT_LIMIT=8, STARBOARD_LIMIT=16, ACTUATOR=32다.
NOT_RUNNING과 정지 의도 같은 운영 상태의 표시 정책을 별도로 정하되 기존 bit 의미를 바꾸지 않는다.
E11에는 boot·sample 시각·valid·장치 인스턴스가 없어 최신/다축 상태 계약을 단독 충족하지 못한다.
E12/E13은 최신 제어 모델의 fallback으로 자동 선택하지 않는다.

## 3. S: 표준 MAVLink 활용 행렬

| S ID | 기능 | 표준 후보 | 경계·결과 |
| --- | --- | --- | --- |
| S01 | 노드·모드·식별 | HEARTBEAT, AUTOPILOT_VERSION, 지원되는 모드 보고 | G↔A, A↔B의 실제 발신 정보. lease/장비 정상으로 해석하지 않음 |
| S02 | 위치·GNSS | GLOBAL_POSITION_INT, GPS_RAW_INT, GPS2_RAW 등 | 센서/추정 출처·정확도·age·frame 확인 |
| S03 | 자세·방위·속도 | ATTITUDE/ATTITUDE_QUATERNION, VFR_HUD, 위치 속도 필드 | Heading/COG/SOG/STW 분리. 선택 wire 단위 고정 |
| S04 | 전원·엔진·ESC | BATTERY_STATUS, SYS_STATUS, EFI_STATUS, ESC_STATUS | 장치 index·물리량·무효 규칙·발행 지원 확인 |
| S05 | 수동 입력 | MANUAL_CONTROL 및 실제 PX4가 지원하는 RC 경로 | G/RC→A. B 장비 목표와 별개 |
| S06 | 단발 운항 명령 | COMMAND_INT/LONG, COMMAND_ACK | G/C→A. 실제 PX4 지원 모드/명령과 응답 정책 사용 |
| S07 | 미션 | MISSION_COUNT/REQUEST_INT/ITEM_INT/ACK/CURRENT/ITEM_REACHED 등 | G↔A. 업로드 성공과 실행 시작/도달을 구분 |
| S08 | 운항구역·복귀점 | 지원되는 MISSION_TYPE_FENCE/RALLY, HOME_POSITION | 실제 PX4 지원·제외구역·귀환 의미 검증 |
| S09 | 주기·시간 | MAV_CMD_SET_MESSAGE_INTERVAL, MESSAGE_INTERVAL, TIMESYNC | 요청 수락과 실제 수신률/시계 품질 확인 |
| S10 | 설정·파일·로그 | PARAM/PARAM_EXT, MAVLink FTP, 지원 로그 protocol | 장비 설정 세션·revision·원자성 부족은 별도 보완 |
| S11 | 경고·장애물·선박 | EVENT/STATUSTEXT, OBSTACLE_DISTANCE, AIS_VESSEL 등 | 실제 센서 범위·품질·출처. 단순 메시지 수신이 회피 기능을 제공하지 않음 |
| S12 | offboard | 해당 PX4 빌드가 지원하는 setpoint/uORB/ROS 2 계약 | C→A, 기능별 frame·liveness·loss action을 시험. 일반 예제 지원을 Boat에 가정하지 않음 |

표준 필드에 제품 session·hash·기어 코드를 임의로 숨겨 넣지 않는다. 표준으로 충분한 기능은 P 메시지를 중복 도입하지 않는다.

## 4. P: 공통 논리 context와 상태 사전

### C-REQ 단발 요청

| 필드 | 제안 타입 | 필수 의미 |
| --- | --- | --- |
| target_system / target_component | uint8 / uint8 | 능동 제어는 지정 대상. 실제 instance는 별도 |
| sender_boot_id / session_id | uint64 / uint64 | 새 부팅/협상 세션 식별. 인증 수단 자체는 아님 |
| lease_id / control_epoch | uint64 / uint32 | 능동 요청에 필요. 읽기 전용 요청은 해당 없음으로 정의 |
| request_id / operation | uint32 / enum16 | 같은 세션에서 재사용 금지, 대상 operation과 함께 대응 |
| instance_id | uint16 | 노드 내 장비·축·채널 식별. ALL은 명시적으로 허용된 operation만 |
| sent_time_us / max_age_ms | uint64 / uint32 | 발신 단조 시각과 허용 age. 시계 품질 검증 필요 |
| expected_revision | uint32 | 설정/계획/프로필 변경 시 비교. 미사용 의미 별도 고정 |

### C-STREAM 고속 목표

C-REQ의 대상·boot/session·lease/epoch·instance·시각/age에 `sequence:uint32`, `control_cycle:uint32`를 사용한다.
단발 request_id와 혼용하지 않는다. 같은 cycle의 묶음 목표는 수신 검증을 함께 하고 적용 상태를 장비별로 보고한다.
네트워크 frame header의 8비트 seq만으로 명령 순서·재부팅·replay·중복을 판별하지 않는다.

### C-STATE 상태·품질

| 필드 | 제안 타입 | 필수 의미 |
| --- | --- | --- |
| source_boot_id / instance_id | uint64 / uint16 | 상태 생성 노드와 실제 장비 인스턴스 |
| sample_id / state_revision | uint32 / uint32 | 센서 sample과 논리 상태 revision 구분 |
| source_sample_time_us / publish_time_us | uint64 / uint64 | 같은 발신 시계 기준이거나 명시적 source clock ID |
| source_age_ms | uint32 | 원천 갱신 age. 재발행으로 초기화 금지 |
| valid_fields / stale_fields | bitmask64 / bitmask64 | 해당 schema의 필드 bitmap. 비트 할당은 XML 리뷰 |
| measurement_kind / quality | enum8 / enum8 | measured/estimated/commanded, 품질/참조 방식 |
| active_faults | bitmask64 또는 별도 event 참조 | 알 수 없는 bit도 보존. 다장비 고장을 하나로 덮어쓰지 않음 |

한 메시지에 서로 다른 sample age의 필드를 묶으면 필드별 metadata나 별도 그룹을 사용한다.
표준 message에 없는 metadata는 표준 field의 의미를 바꾸지 않고 별도 제품 snapshot 등으로 대응한다.

### C-RESULT 요청 결과

`request_id:uint32`, `operation:enum16`, `instance_id:uint16`, 요청자의 boot/session,
`result:enum8`, `reason:enum16`, `progress:uint8`, `state_revision:uint32`, `applied_sequence:uint32`,
장치별 결과를 논리적으로 식별한다. symbolic 결과는 ACCEPTED/IN_PROGRESS/SUCCEEDED/REJECTED/FAILED/CANCELED/PARTIAL/RESULT_UNKNOWN이다.
거부 사유 후보는 VERSION_MISMATCH, UNAUTHORIZED, WRONG_TARGET, NO_LEASE, LEASE_EXPIRED, STALE_COMMAND,
OUT_OF_ORDER, NOT_READY, UNSUPPORTED, INVALID_VALUE, CONFIG_MISMATCH, INTERLOCK, SENSOR_STALE,
ESTOP_ACTIVE, BUS_FAULT, BUSY, STORAGE_FAILURE, REQUEST_CONFLICT다. 숫자 값은 미할당이다.

## 5. P 메시지 목록: 연결·권한·상태

아래 요청/응답은 모두 제안이다. 요청 context C-REQ, 상태 C-STATE, 결과 C-RESULT의 적용 범위를 따르며,
wire packet 분할·주소·권한·최대 payload를 XML 작업에서 확정한다.

| P ID | 논리 이름 | 방향·주기 | 추가 필드: 제안 타입 | 전제·응답/완료 |
| --- | --- | --- | --- | --- |
| P01 | P_DEVICE_INFO_REQUEST/STATUS | G→A, A→B / 요청 시 | device_uid:bytes[16], fw_hash:bytes[20], xml_hash:bytes[32], protocol_major/minor:uint16, hw_revision:uint16, config_revision:uint32 | 읽기 전용. 노드별 version/capability 수신, 운항 제어 자동 허용 아님 |
| P02 | P_CAPABILITY_PAGE | A/B→요청자 / 요청·변경 | page:uint16, page_count:uint16, capabilities:bitmask64, max_instances:uint16, supported_ops 목록 | 페이지 누락·버전/boot 변경 시 전체 재조회. 지원 enum·상한 포함 |
| P03 | P_DEVICE_INVENTORY | B→A→G / 요청·변경 | device_id:uint16, class:enum8, bus:enum8, bus_address:uint32, physical_role:enum16, measurement_kind:enum8 | 역할과 ECU/축 실주소 대응. 장치 수는 capability에 제한 |
| P04 | P_LINK_HEALTH | A/B→G/A / 1Hz 제안 | peer_id:uint16, rx_age_ms:uint32, crc/drop/overrun/replay_count:uint32, queue_depth:uint16, rtt_ms:uint32 | 원인 endpoint 구분, peer heartbeat age와 센서 age 별도 |
| P05 | P_AUTHORITY_ACQUIRE | G/C→A 또는 A/정비→B / 단발 | requested_role:enum8, scope:bitmask32, requested_lease_ms:uint32, expected_epoch:uint32 | 인증·상태·우선권 검사. C-RESULT와 P08 수신 후 소유 확인 |
| P06 | P_AUTHORITY_RENEW/RELEASE | 현재 소유자→수신자 / 갱신·단발 | lease_id:uint64, epoch:uint32, lease_ms:uint32, release_reason:enum16 | 타 소유자 갱신 불가. release 시 대기 목표 제거·장비 정책 적용 |
| P07 | P_AUTHORITY_TRANSFER/REVOKE | 승인된 중재자→수신자 / 단발 | new_owner:노드 식별, scope:bitmask32, expected_epoch:uint32, reason:enum16 | 이전 epoch 폐기, 새 입력 준비/실패 처리. 소유자 둘이 동시 적용되지 않음 |
| P08 | P_AUTHORITY_STATUS | A/B→관련 노드 / 즉시+5Hz 제안 | owner:노드 식별, role:enum8, scope:bitmask32, lease_remaining_ms:uint32, epoch:uint32, state:enum8, reason:enum16 | 수신자 로컬 lease 판정이 권위 원천. 상태 메시지 지연으로 권한 연장 불가 |
| P09 | P_NODE_STATUS | B→A→G / 1~5Hz 제안 | node_state:enum8, ready/blocked_reasons:bitmask64, reset_cause:enum16, uptime_us:uint64, active_config:uint32 | BOOT/READY/DEGRADED/FAULT 구분. ready bit 하나로 장비 전체 보증하지 않음 |
| P10 | P_RESULT_QUERY/STATUS | 요청자↔처리자 / 단발 | original_request_id:uint32, original_session:uint64, operation:enum16, result:C-RESULT | 응답 유실·재연결 결과 조회. journal 상실이면 RESULT_UNKNOWN |

노드 식별은 transport 주소와 device_uid에 대응하는 typed 구조다. 고정 길이 장치 이름 문자열을 권한 키로 사용하지 않는다.

## 6. P 메시지 목록: 추진·조향·부가장치

| P ID | 논리 이름 | 방향·주기 | 추가 필드: 제안 타입 | 전제·응답/완료 |
| --- | --- | --- | --- | --- |
| P11 | P_ACTUATOR_TARGET | A→B / 20Hz 제안 | valid_targets:bitmask32, propulsion_mode:enum8, requested_output:float32, requested_rpm:float32, requested_gear:enum8, rudder_deg:float32, target_config_revision:uint32 | C-STREAM, 단일 권한·profile 일치. 서로 배타적인 출력/RPM 모드 검사. 실제 장치별 적용 seq 보고 |
| P12 | P_PROPULSION_COMMAND | A→B / 단발 | action:enum8, engine_id:uint16, start_timeout_ms:uint32 | START/STOP/IDLE/DISABLE 등 별도. 재시도·원격 시동 interlock. C-RESULT+실제 상태 |
| P13 | P_GEAR_COMMAND | A→B / 단발 | gear:enum8, expected_current_gear:enum8, max_transition_ms:uint32 | RPM/속도·중립 경유·lease 확인, 실제 기어 피드백으로 완료. 없으면 확인 불가 |
| P14 | P_RUDDER_COMMAND | A→B / 단발 | action:enum8, axis:uint16, expected_mode:enum8 | ENABLE/HALT/QUICK_STOP/DISABLE/RESET/CENTER 구분. CENTER는 이동이며 활성 조건 필요 |
| P15 | P_ACTUATOR_STATUS | B→A→G / 10Hz 제안 | target/applied/actual_output:float32, target/actual_rpm:float32, target/actual_gear:enum8, target/actual_angle_deg:float32, device_state:enum8, applied_sequence:uint32, limiting_reason:enum16 | C-STATE, commanded/estimated/measured 구분. 필요 시 엔진/축별 메시지 분할 |
| P16 | P_ENGINE_SENSOR_STATUS | B→A→G / 5Hz 제안 | rpm:float32, coolant/head/exhaust_temp_c:float32, oil_pressure_pa:float32, fuel_rate_lph:float32, fuel_level_l:float32, engine_hours:float32, ecu_fault_ref:uint32 | 필드별 valid·age. 실제 설치한 센서만 지원, 온도 의미 변경 금지 |
| P17 | P_ELECTRIC_PROPULSION_STATUS | B→A→G / 5~10Hz 제안 | rpm_signed:float32, bus_voltage_v:float32, current_a:float32, motor/esc_temp_c:float32, output_limit:float32, fault_ref:uint32 | 전동 추진 capability 전용. 방향·전력/회생 의미 합의 |
| P18 | P_STEERING_DIAGNOSTICS | B→A→G / 1Hz·요청 | axis:uint16, counts:int32, angle_deg:float32, velocity_deg_s:float32, tracking_error_deg:float32, statusword/controlword/error_code:uint16, wkc:int16 | 실측/환산 구분, 원시 CoE 진단과 운항 상태를 혼합하지 않음 |
| P19 | P_BUS_STATUS | B→A / 1Hz·변경 | bus_id:uint16, bus_type:enum8, state:enum8, error_count/drop_count:uint32, last_source_age_ms:uint32, cycle/jitter_us:uint32 | CAN/EtherCAT/UART별 장애·복구, 전체 정상/부분 장애 구분 |
| P20 | P_IO_COMMAND/STATUS | A↔B / 단발·변경 | channel_id:uint16, function:enum16, requested/actual_state:enum8, feedback_available:uint8, interlock_reason:enum16 | 역할 allowlist·폴라리티·상호배제·boot default. 출력 latch만 있으면 actual 확인으로 표시하지 않음 |
| P21 | P_PAYLOAD_COMMAND/STATUS | G/C→A→B / 단발·작업 | payload_id:uint16, job_id:uint32, action:enum16, setpoint:float32, sample_counter:uint32, result_ref:uint32 | 펌프/측정/시료채취 등 선택 기능. 단위·operation별 schema는 payload profile로 추가 |

### P11 목표 단위와 원자성

내연기관 output 모드는 [0,1]이며 E01로 변환할 때 ×1000이다. 전동 signed 모드는 별도 enum과 capability로 구분한다.
RPM 모드·기어·rudder를 사용하지 않는 필드는 valid_targets에서 제외하고 수신자가 이전 값 유지인지 무효화인지 profile로 명시한다.
한 묶음의 필수 필드가 유효하지 않으면 전체 거부를 기본 제안으로 한다. 부분 허용이 필요한 profile은 장치별 결과를 보고한다.
수신 ACK나 `applied_sequence`는 motor/선박의 물리적 도달을 의미하지 않는다.

## 7. P 메시지 목록: 항법·미션·자율운항

| P ID | 논리 이름 | 방향·주기 | 추가 필드: 제안 타입 | 전제·응답/완료 |
| --- | --- | --- | --- | --- |
| P22 | P_NAV_QUALITY_STATUS | A→G/C / 5Hz 제안 | nav_source:enum8, heading_ref:enum8, position/heading/velocity_valid:bitmask32, horizontal_error_m:float32, heading_error_deg:float32, source_age_ms:uint32, estimator_state:enum8 | S02/S03을 보완할 필요가 있을 때만. 항법/센서 원천을 구분 |
| P23 | P_VESSEL_LIMITS_STATUS | A/B→G/C / 요청·변경 | config_revision:uint32, min/max_speed_mps:float32, max_yaw_rate_deg_s:float32, min_turn_radius_m:float32, accel/decel_mps2:float32, draft_m:float32 | 고정/추정·속도 의존 한계·신뢰도 명시. 계획기가 장비 한계를 우회하지 않음 |
| P24 | P_NAV_INTENT | G/C→A / 단발 | intent:enum8, lat/lon_degE7:int32, heading_deg:float32, speed_mps:float32, acceptance_radius_m:float32, frame:enum16, completion_policy:enum8 | Go To/heading/speed/hold/return은 S06 우선. 필요한 제품 부가 의미만 확장 |
| P25 | P_PLAN_MANIFEST | G/C↔A / 전송 전·조회 | plan_id:uint64, revision:uint32, content_hash:bytes[32], item_count:uint32, map/fence_revision:uint32, limits_revision:uint32, completion_policy:enum8 | S07의 opaque ID·transfer와 대응. hash 일치가 경로 안전성/권한을 보증하지 않음 |
| P26 | P_PLAN_VALIDATE_REQUEST/STATUS | G/C↔A / 단발 | plan_id:uint64, revision:uint32, validation_result:enum8, first_error_item:uint32, reason:enum16, constraint_revision:uint32 | 좌표·지원 명령·구역·선회/속도 검증. 불완전 계획 활성화 금지 |
| P27 | P_MISSION_CONTROL | G/C→A / 단발 | action:enum8, plan_id:uint64, revision:uint32, start/resume_item:uint32, pause_policy:enum8, completion_policy:enum8 | START/PAUSE/RESUME/ABORT/RETURN. S06/S07로 충분한 경우 중복 메시지 도입 안 함 |
| P28 | P_MISSION_EXECUTION_STATUS | A→G/C / 변화+1~5Hz 제안 | plan/revision, state:enum8, item:uint32, segment_id:uint32, distance_m/cross_track_m/eta_s:float32, reason:enum16, active_owner 참조 | 실행/도달/작업 완료·추진 상태 분리. 표준 진행 정보 우선 활용 |
| P29 | P_GEOFENCE_ENERGY_STATUS | A→G/C / 1Hz·변경 | fence_revision:uint32, inside:enum8, breach_type:enum16, margin_m:float32, energy_remaining_wh/fuel_l/reserve_estimate:float32, estimate_quality:enum8 | fence와 energy schema를 필요 시 분리. unknown·센서 stale·귀환 추정 한계 표시 |
| P30 | P_AUTONOMY_STATUS | C→A→G / 5Hz 제안 | planner_boot:uint64, mode/state:enum8, nav/scene_revision:uint32, valid_horizon_ms:uint32, last_plan_revision:uint32, degraded_reason:enum16 | C 권한·liveness와 PX4가 실제 채택한 trajectory 상태 구분 |
| P31 | P_TRAJECTORY_PROPOSAL/STATUS | C↔A / 협상된 주기 | trajectory_id:uint64, revision:uint32, frame:enum16, start_time_us:uint64, horizon_ms:uint32, point_count:uint16, points:분할된 typed 목록 | 한 frame에 큰 배열 전송 금지. 불완전/만료/horizon 밖·한계 위반 거부, 안전한 switch point 명시 |
| P32 | P_OBSERVATION_BATCH | 센서/C→A 또는 C/G / 협상 | sensor_id:uint16, scene_revision:uint32, frame:enum16, observation_time_us:uint64, count:uint16, tracks:분할 목록 | 안전 제어 링크의 raw 영상/radar 대용 아님. 대상마다 age·품질·출처·측정/추정 구분 |
| P33 | P_AVOIDANCE_DECISION | C→A→G / 변화+주기 | decision_id:uint64, risk_state:enum8, related_track_ids:목록, maneuver:enum8, trajectory_ref:uint64, expiry_ms:uint32, reason:enum16 | 인지/계획 실패·stale를 정상 회피로 표시하지 않음. 제안과 PX4 채택을 구분 |
| P34 | P_DOCKING_CONTROL/STATUS | G/C↔A / 단발·5Hz 제안 | berth_id:uint64, approach_plan:uint64, relative_frame:enum16, relative_pose:typed 값, pose_quality:enum8, stage:enum8, abort_reason:enum16 | 별도 센서/저속 제어·abort·최종 체결/정지 피드백 인수 필요 |
| P35 | P_SURVEY_WORK_STATUS | A/C→G / 1~5Hz·조회 | job_id:uint64, segment_id:uint32, work_state:enum8, coverage_ref:uint64, sample_id:uint32, sample_time_us:uint64, sensor_position_ref:uint32, missing_segment_count:uint32 | 작업/이동 구분, 시각·센서 offset·누락 구간·재개점 정의 |
| P36 | P_FLEET_TASK | G/C↔A / 선택적 단발 | fleet/task_id:uint64, vessel_uid:bytes[16], assignment_revision:uint32, conflict_policy:enum8, plan_ref:uint64 | 다선박 기능 전용. 각 선박 lease·identity 유지, 방송 actuator 목표 금지 |

trajectory point 논리 필드는 frame에 따른 위치·속도·가속도·heading/yaw rate·유효 시간이다.
최대 점 수·패킷 수·CRC/hash·순서·완성 조건·수명과 PX4 offboard 변환은 D12에서 고정한다.
observation track은 ID·분류·위치·속도·크기·공분산/품질·source_kind·age를 가진다.
AIS는 선박 보고 출처이며 센서 실측·동일 객체 연결의 확정 보증으로 사용하지 않는다.

## 8. P 메시지 목록: 보호·설정·정비·진단

| P ID | 논리 이름 | 방향·주기 | 추가 필드: 제안 타입 | 전제·응답/완료 |
| --- | --- | --- | --- | --- |
| P37 | P_SAFETY_ACTION | 승인 주체→A/B / 단발 | action:enum8, scope:bitmask32, reason:enum16, expected_latch_revision:uint32 | SOFT_STOP/INHIBIT/RESET_REQUEST 구분. 물리 E-stop 해제·장비 재활성 자동 실행 금지 |
| P38 | P_SAFETY_STATUS | B/A→G / 즉시+5Hz 제안 | latch_revision:uint32, active_conditions:bitmask64, commanded_action:enum8, device_results:분할 목록, reset_allowed:uint8, blocking_reason:enum16 | per-device stopped/failed/unknown, 물리 회로 입력·출력·해제 상태 분리 |
| P39 | P_FAULT_EVENT/SNAPSHOT | B/A/C→상위 / 즉시·조회 | event_id:uint64, event_seq:uint32, fault_code:uint32, source_instance:uint16, severity:enum8, active:uint8, first/last_time_us:uint64, occurrences:uint32, related_request:uint32 | latch/active/history 구분, seq gap 재조회, 알 수 없는 code 보존 |
| P40 | P_CONFIG_READ/STAGE/COMMIT/STATUS | 정비/G→A→B / 단발 | transaction_id:uint64, base/new_revision:uint32, parameter_set:typed 목록, hash:bytes[32], persist:uint8, result:C-RESULT | 기능적으로 분리된 메시지 후보군. 전필드 검증·readback·영속 결과·전원상실 복구 |
| P41 | P_CALIBRATION_CONTROL/STATUS | 정비→A/B / 단발·진행 | procedure_id:uint16, axis:uint16, calibration_revision:uint32, stage:enum8, progress:uint8, abort_reason:enum16 | 유지보수 lease, 장비 절차·이동 한계·독립 중단. 완료 후 임의 re-enable 안 함 |
| P42 | P_UPDATE_CONTROL/STATUS | 정비→A/B / 단발·진행 | image_hash:bytes[32], version:typed 식별, image_size:uint32, stage:enum8, recovery_state:enum8 | 전송은 기존 FTP 등 우선. 이미지 승인·bootloader rollback·운항 중 차단 별도 구현 |
| P43 | P_LOG_INDEX/TRANSFER_STATUS | G/정비↔A/B / 요청 | log_id:uint64, start/end_time_us:uint64, size:uint32, hash:bytes[32], transfer_offset:uint32, drop_count:uint32 | 제어보다 낮은 우선순위, 구간·파일·무결성·중단 재개. 비밀값 제외 |
| P44 | P_DIAGNOSTIC_SNAPSHOT | A/B/C→요청자 / 1Hz·요청 | cpu_load_pct:float32, free_memory:uint32, control_cycle_max_us:uint32, loop_miss_count:uint32, watchdog_reason:enum16, firmware/config 참조 | 비동기 진단이 실시간 장비 루프를 차단하지 않음 |
| P45 | P_STREAM_PROFILE_REQUEST/STATUS | 권한 주체↔A/B / 단발 | profile_id:uint16, stream_group:uint16, requested/effective_rate_hz:float32, bandwidth_limit_bps:uint32, rejected_reason:enum16 | S09 우선, 승인한 실제 rate와 drop·필수 최소 rate 보고 |
| P46 | P_OPERATION_PROFILE_STATUS | A/B→G/C / 조회·변경 | profile_id:uint16, revision:uint32, applicable_capabilities:bitmask64, timeout_set_ref:uint32, vessel_limits_ref:uint32, approved:uint8 | 프로필 승인·장비·lease·운항 모드와 연결. 변경은 명시적 새 revision |
| P47 | P_ENVIRONMENT_SENSOR_STATUS | B/C→A→G / 1~5Hz·요청 | sensor_id:uint16, quantity:enum16, reference:enum16, value:float32, unit:enum16, sensor_offset_ref:uint32, uncertainty:float32 | 바람/조류/수심/수온 등 선택 센서. C-STATE·measured/estimated, 진풍/겉보기·기준면 구분 |
| P48 | P_HULL_AUX_STATUS | B→A→G / 변화+1Hz 제안 | sensor_id:uint16, function:enum16, state:enum8, level:float32, pump_commanded/feedback:enum8, fault_ref:uint32 | 누수·빌지·펌프·해치·화재 감지 등 설치 장치만. 미수신을 닫힘/정상/배수 완료로 처리하지 않음 |

## 9. 명령별 응답·시간 초과·재시도 표

| 분류 | 첫 응답·실제 완료 | 재시도/취소 | 만료 시 |
| --- | --- | --- | --- |
| 읽기·식별 | 요청 ID 대응 상태 응답 | 동일 요청 bounded retry 가능 | 읽기 실패. 능동 기능 자동 허용 안 함 |
| lease | 승인 결과+수신자 authority status | 같은 요청 duplicate 결과, 거부 후 무한 재획득 금지 | 권한 없음/상실. 이전 목표 폐기 |
| 고속 목표 | 매번 ACK 없음, applied seq·실제값·거부 이벤트 | 최신 목표만 유지, 오래된 목표 재전송 금지 | 로컬 TTL·장비별 loss action |
| ENABLE/HALT/정지 | 처리 ACK + 실제 장치 상태 | 동일 요청 ID 중복 보호. 결과 unknown이면 조회 | 물리 완료 미확인 경고·장치별 실패 |
| START/기어/보정 | 접수·진행·최종 결과와 실측 | 비멱등. 같은 ID journal, reboot 후 자동 재실행 금지 | RESULT_UNKNOWN/FAILED, operation별 보호 |
| 설정/업데이트 | stage/검증·commit/저장·readback 결과 | transaction별 재조회·원자성 | 이전 유효 설정/복구 이미지 유지 |
| 미션 전송 | 표준 MISSION 요청/ACK | 표준 retransmit와 실제 PX4 구현 사용 | 불완전 계획 활성화 금지 |
| 운항 의도 | 표준 ACK/제품 결과 + 실제 모드·진행 | 해당 PX4 명령 정책, 안전한 중복 처리 | 결과 미확인·상태 재조회. 임의 성공 표시 금지 |

각 operation의 `T_ack`, `T_operation`, `retry_limit`, `W_dedupe`, 완료 오차·지속 시간은 승인 profile로 고정한다.
모든 동작에 같은 3초 timeout이나 동일 재시도 횟수를 강제하지 않는다.

## 10. 필수 지원 행렬과 패킷 예시

| 기능 | G | A | B | C | 필수 여부 |
| --- | --- | --- | --- | --- | --- |
| 읽기 전용 발견·버전·장비·원천 상태 | 표시 | 중계/변환 | 생성 | 선택 소비 | M0 필수 |
| authority·장비 TTL·정지·결과 | 요청/표시 | 중재/생성 | 강제/실행 | 해당 권한 | M1 필수 |
| 항법·기본 WP·중단/재개/복귀 | 편집/요청 | 실행/보고 | 목표 실행 | 없어도 됨 | M2 필수 |
| 인식·회피·trajectory | 표시 | 채택/검증 | 장비 한계 강제 | 생성 | M3 선택 |
| 접안·다선박·특수 payload | 기능별 | 기능별 | 설치 장비만 | 기능별 | 선택 |

아래는 로그/검토용 JSON 표현이며 실제 MAVLink wire 프레임이나 이미 존재하는 API가 아니다.
64비트 ID는 JSON 수치 정밀도 손실을 피하도록 문자열로 표시한다.

```json
{
  "logical_message": "P_ACTUATOR_TARGET",
  "status": "PROPOSED_NOT_ALLOCATED",
  "session_id": "301",
  "lease_id": "401",
  "control_epoch": 7,
  "sequence": 120,
  "control_cycle": 120,
  "max_age_ms": 300,
  "target_profile": "LAB_SINGLE_RUDDER_OUTPUT",
  "targets": {
    "propulsion_mode": "OUTPUT_FRACTION",
    "requested_output": 0.2,
    "requested_rudder_deg": 5.0
  }
}
```

이 예시는 실제 송신에 필요한 C-STREAM 대상·발신 boot·시각·인증 context를 전부 표시하지 않은 논리 예다.
20%·5°·300ms는 실제 선박 운용 허용값이 아니다. production profile과 협상·버전·장비 readiness가 먼저 필요하다.

## 11. 구현 승인 전 체크

- [ ] E01~E13마다 XML 선언/발신 구현/수신 구현/실측 시험을 서로 다른 칸으로 기록.
- [ ] P01~P48 중 표준 S로 충분한 기능을 제거하거나 매핑하고, 실제 채택할 최소 subset 확정.
- [ ] 신규 메시지 ID·enum·bitmap·payload 순서·최대 길이·확장/미지원 정책 리뷰.
- [ ] 각 메시지의 boot/session/권한/대상·TTL·결과·field validity 및 source age 연결.
- [ ] command/mission/setpoint/event를 서로 다른 응답·재시도 규칙으로 구현.
- [ ] [인수 계획](ACCEPTANCE_TEST_PLAN.md)의 패킷·장애·실선 증거를 해당 E/S/P ID와 연결.
