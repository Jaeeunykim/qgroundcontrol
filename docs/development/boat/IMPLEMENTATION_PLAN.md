# QGC 공식 기여를 위한 Boat 지원 구현 계획

- 작성일: 2026-11-17
- 진행 상태 갱신: 2026-11-23
- 분석 기준: QGC `v5.1.5`, 커밋 `3a67d31f0c36bf3fe38ec52970d250a89d0aaf67`
- 작업 브랜치: `feature/boat`
- 상태: P1/P2/P3 로컬 코드·자동 검증 완료. P4 기본 표시 구현·자동 검증 완료. 실제 PX4·실선 인수와 공식 upstream 반영은 미완료.
- 첫 통합 대상: PX4/Pixhawk 기반 수상 무인선. PX4 버전·커스텀 커밋은 미확정.
- 관련 문서: [상태정보 계약 초안](TELEMETRY_CONTRACT.md), [저장소 README](../../../README.md)

## 1. 목표와 경계

표준 MAVLink로 연결된 Boat에서 기존 QGC 화면으로 항로 작성·저장·전송·운항·중단·재개·복귀를
일관되게 수행하도록 보완한다. 화면 재설계보다 기존 동작의 의미와 사용 가능 조건을 먼저 맞춘다.

```text
선박 장비·badaMux ↔ Pixhawk/PX4 ↔ MAVLink ↔ QGC
```

QGC는 PX4가 보고한 기체 종류·상태·지원 기능을 사용한다. 선박 장비의 CAN/EtherCAT 처리,
badaMux 직접 제어·보정, 장비별 출항 점검 화면, 전용 dialect는 기본 Boat 지원의 의존성이 아니다.
별도 Manager 체계나 Custom Build를 만들지 않고 기존 Vehicle·FirmwarePlugin·Fact·미션 구조를 활용한다.

기존 지원을 처음부터 없는 것으로 간주하지 않는다. 현재 Boat는 Rover 공통 클래스에 포함되어 있고,
일부 지형 처리와 미션 명령 필터링에는 이미 Boat/Rover 처리가 있다. 필요한 차이만 추가한다.

### 공식 기여 범위

- 기체 종류에 따른 공통 UI와 펌웨어별 명령 처리를 분리한다.
- PX4를 첫 검증 대상으로 삼되 공통 변경에서 ArduPilot Boat·Rover·항공기·Sub를 손상하지 않는다.
- 특정 장비·제조사 이름이나 PX4 커스텀 모드 숫자를 공통 QML에 넣지 않는다.
- 실제 지원이 확인되지 않은 위치 유지·역추진·접안·제어권 획득 기능을 Boat 타입만으로 노출하지 않는다.
- 기본 WP 운항을 우선한다. 수심 측량 패턴, AIS, 자동 회피·접안, 해도 위험 판단은 후속 제안이다.
- 공식 PR 전 최신 `mavlink/qgroundcontrol:master`와 차이를 재확인하고 필요한 변경을 이식한다.
  Stable 기반의 로컬 완료와 upstream의 검토·승인·병합을 별도 상태로 관리한다.

## 2. 현재 소스에서 확인한 보완 지점

아래는 정적 분석 결과이며 실제 PX4 무인선에서 재현한 결과가 아니다.

| 항목 | 확인한 소스 | 구현 시 판단 |
| --- | --- | --- |
| 기체 분류 | [QGCMAVLink.cc](../../../src/MAVLink/QGCMAVLink.cc), [Vehicle.h](../../../src/Vehicle/Vehicle.h) | `rover`는 Rover와 Boat 공통 의미. 이를 유지하며 Boat 판별 속성/헬퍼 추가 검토 |
| Guided 조작 | [GuidedActionsController.qml](../../../src/FlyView/GuidedActionsController.qml) | Land·고도 변경 조건에 Boat 명시 제외가 없고 미션 시작에 이륙 문구가 남음 |
| 단일 기체 Pause | 같은 파일의 `actionPause` | 고도 슬라이더와 `guidedModeChangeAltitude(..., true)` 경로 사용. 고도와 무관한 Boat 중단 경로 필요 여부 검토 |
| PX4 모드·명령 | [PX4FirmwarePlugin.cc](../../../src/FirmwarePlugin/PX4/PX4FirmwarePlugin.cc) | 미션 시작·Pause·Go To·RTL을 실제 무인선 펌웨어와 대조. ArduPilot Hold 동작을 대입하지 않음 |
| 속도 편집 | [SpeedSection.cc](../../../src/MissionManager/SpeedSection.cc) | 활성화가 Multirotor/FixedWing으로 제한됨. 생성뿐 아니라 재읽기와 저장 왕복도 수정 대상 |
| 미션 명령 목록 | `PX4FirmwarePlugin::supportedMissionCommands` | 이착륙 명령은 이미 일부 클래스별로 제한됨. 공통 목록에 남은 고도 전용 명령 등을 추가 점검 |
| 오프라인 기체 | [App.SettingsGroup.json](../../../src/Settings/App.SettingsGroup.json), [PlanMasterController.cc](../../../src/MissionManager/PlanMasterController.cc), [MissionController.cc](../../../src/MissionManager/MissionController.cc) | 실제 Boat 타입이 공통 클래스로 축약되는 경로 점검. 클래스 enum에 11만 추가해서 해결하지 않음 |
| 기본 계기판 | [QGCCorePlugin.cc](../../../src/API/QGCCorePlugin.cc) | 상대고도·상승률 중심 기본값. 사용자 저장 레이아웃과 구분하여 개선 |
| 텔레메트리 | [VehicleFactGroup.cc](../../../src/Vehicle/FactGroups/VehicleFactGroup.cc), [VehicleGPSFactGroup.cc](../../../src/Vehicle/FactGroups/VehicleGPSFactGroup.cc), [VehicleEFIFactGroup.cc](../../../src/Vehicle/FactGroups/VehicleEFIFactGroup.cc) | 기존 속도·방향·EFI Fact를 활용. 원천 유효성·수신 만료까지 자동 보장되는 것은 아님 |

## 3. 해야 할 일

`[x]`는 해당 칸에 적힌 로컬 구현·자동 검증의 완료를 뜻한다. 최종 인수는 별도 열로 관리한다.
실제 PX4·실선 검증, 전체 lint, 공식 upstream 반영은 아직 완료하지 않았다.
검증 근거와 미실행 범위는 아래 P1/P2 구현 기록 및 테스트 가이드를 따른다.

| ID | 작업 | 완료 조건 | 구현·자동 검증 상태 | 최종 인수 |
| --- | --- | --- | --- | --- |
| BOAT-01 | 지원 환경·메시지 계약 확정 | PX4/보드/XML 버전, Boat HEARTBEAT, 지원 모드·명령과 필수 상태정보 합의 | 초안 작성, PX4 환경·계약 합의 대기 | [ ] |
| BOAT-02 | Boat 식별 | Boat만 정확하게 판별하고 기존 Rover 공통 동작·변경 알림·null 처리가 유지됨 | [x] P1 구현·자동 검증 완료 | [ ] |
| BOAT-03 | 오프라인·파일 보존 | Boat 계획 생성→저장→재열기→연결/해제 후 실제 타입 보존, 구형 파일 호환 | [x] P1 구현·자동 검증 완료; GUI 재실행 수동 확인 대기 | [ ] |
| BOAT-04 | Guided 조작 정합성 | Boat에 이착륙·고도 조작이 노출/실행되지 않고 지원되는 조작만 가능 | [x] P2 구현·자동 검증 완료 | [ ] |
| BOAT-05 | 미션 실행 | 시작·중단·재개·Go To·RTL의 요청·응답·실제 상태를 검증 | [x] P2 코드·모의 명령/QML 검증 완료; 실제 운항 결과 대조 대기 | [ ] |
| BOAT-06 | 계획 편집·명령 제한 | 고도 UI·지형 의존을 조정하고 지원되지 않는 항공기 명령 유입 처리 | [x] P3 기본 WP·고도 UI·명령 제한 구현·자동 검증; 복합 패턴 별도 | [ ] |
| BOAT-07 | 속도 | 활성화·ground speed 명령 생성·재읽기·파일/기체 왕복 일치 | [x] P3 속도 생성·파일/MockLink 왕복 검증 완료 | [ ] |
| BOAT-08 | 계기판·방향 | SOG/Heading/COG를 구분하고 적절한 기본값 제공, 사용자 설정 보존 | [x] P4 기본 계기판·Heading/COG·아이콘·배치 보존 구현·자동 검증; 실기 대기 | [ ] |
| BOAT-09 | 수동 조종 | 기존 조이스틱의 조향·추진·중립·역추진 지원 조건과 입력 상실 처리 확인 | P4 가상 입력 세션·축 검증 반영; 물리 조종·중립/역추진 후속 | [ ] |
| BOAT-10 | 상태·문구·체크리스트 | 미수신/무효/만료·지원 안 됨을 구분하고 armed를 이동/엔진 시동으로 표시하지 않음 | P2 운항 안내·P4 상태·미수신/무효·전체 단절 복구·공통 점검 반영; 개별 만료·장비별 점검 후속 | [ ] |
| BOAT-11 | 통합·회귀 검증 | 아래 T01~T15와 실행 환경·제한·증거 기록 | 일부 완료 — P1/P2/P3 자동 회귀 통과; 전체 통합·실기·플랫폼 검증 대기 | [ ] |
| BOAT-12 | 공식 기여 준비 | 최신 master 재대조, 문제별 PR, 영문 설명·UI 증거·CI 결과 준비 | 예정 — P5 | [ ] |

## 4. 화면별 UI 구성

화면별 UI 정의와 QML 파일별 수정 범위를 함께 관리한다. 화면 정의는 사용자가 보는 정보·조작·상태를,
다음 절의 QML 표는 이를 구현할 파일과 C++ 의존성을 설명한다.
아래는 기존 QGC 화면을 사용하는 구성 제안이며, 새 디자인의 확정 시안이나 신규 화면 구현 완료를 뜻하지 않는다.

### 공통 탐색과 표시 원칙

- 기존 Fly / Plan / Configure / Settings / Analyze 탐색 구조를 유지한다.
  문서에서 Fly는 운항 화면, Configure는 기체 설정 화면을 뜻한다. 최상위 메뉴 이름의 Boat별 변경은
  다중 기체·번역 영향을 검토한 뒤 별도로 결정한다.
- Boat는 현재 선택한 기체 또는 오프라인 계획의 기체 타입으로 판별한다.
  선박 하나를 선택했다고 앱 전체를 영구 Boat 모드로 바꾸지 않는다.
- 상단의 선택 기체·연결·모드·전원·GNSS 정보는 기존 위치를 유지한다.
  armed, 운항 모드, 실제 이동, 엔진 시동, 제어 주체를 서로 다른 의미로 취급한다.
- 기능 자체가 미지원이거나 Boat에서 무의미하면 해당 조작을 숨긴다.
  지원하지만 현재 상태 때문에 사용할 수 없는 조작은 기존 QGC 패턴에 맞춰 비활성 사유 또는 실행 불가 안내를 제공한다.
  기존에 숨김을 사용하는 상태 전이도 있으므로 적용 방식은 화면별로 확인한다.
- 미수신·무효·만료 값은 정상 숫자나 0으로 채우지 않는다. 표시·복구 정책은
  [상태정보 계약](TELEMETRY_CONTRACT.md)의 해당 데이터와 연결한다.
- 아래 화면에 badaMux 전용 직접 제어·장비 점검 화면이나 새로운 최상위 메뉴를 추가하지 않는다.

### UI-01 운항 화면: 기존 Fly View

| 구성 | Boat에서의 화면 정의 |
| --- | --- |
| 목적·진입 | 기존 Fly View에서 선박 위치·상태·임무 진행을 확인하고 지원되는 운항 명령 실행 |
| 기본 배치 | 지도·항적을 중심으로 상단 상태 표시, 기존 도구 모음, 계기판·영상 배치를 유지 |
| 기본 정보 | 대지속도, 선수방위, 다음 WP 거리, Home 거리 중심의 초기 계기판 제안. COG·ETA는 유효한 데이터와 공간을 고려해 선택 표시 |
| 조작 | 미션 시작·계속/재개·중단·Go To·복귀 중 펌웨어와 현재 상태가 허용하는 항목. 가상 조이스틱은 기존 설정으로 표시 |
| Boat 차이 | 이착륙·고도 변경 조작과 관련 고도 슬라이더 제외. 기존 사용자 계기판 배치를 강제 교체하지 않음 |
| 상태·완료 조건 | 기체 없음·연결 중·정상·미션 진행·단절·기체 전환을 처리. 지도·수치·명령이 모두 선택한 기체와 일치하고 고도 없는 Go To/중단이 동작 |
| 검증 연결 | BOAT-04~05·08~10, T04~T07·T10~T13 |

### UI-02 항로 계획 화면: 기존 Plan View

| 구성 | Boat에서의 화면 정의 |
| --- | --- |
| 목적·진입 | 기존 Plan View에서 연결 상태와 무관하게 Boat 항로를 작성·저장·전송 |
| 기본 배치 | 지도 위 WP·순번·연결선·Home, 기존 계획 도구 모음과 우측 항목 편집 패널 유지 |
| 편집 정보 | WP 위경도, 지원되는 운항 속도·대기/도착 관련 항목, 경로 거리와 유효할 때의 예상시간. 편집 항목은 실제 지원 명령과 메타데이터에 따름 |
| 조작 | WP 추가·이동·수정·삭제, 파일 저장/열기, 업로드/다운로드. 기존 파일 선택·진행·오류 안내 재사용 |
| Boat 차이 | 기본 WP의 고도·지형 기준 선택과 항공기 전용 명령/통계 조정. 화면 고도 숨김이 wire frame·고도 필드 제거/일괄 0 변경으로 이어지지 않음 |
| 상태·완료 조건 | 빈 계획·미저장 변경·오프라인·송수신 중·실패·구형 파일을 처리. Boat 타입·WP·속도가 저장/기체 왕복 후 유지되고 불필요한 지형 대기로 막히지 않음 |
| 검증 연결 | BOAT-03·06~07, T02~T03·T08~T09·T13 |

### UI-03 기체 설정 화면: 기존 Configure

| 구성 | Boat에서의 화면 정의 |
| --- | --- |
| 목적·진입 | 기존 설정 요약·컴포넌트 목록에서 연결한 선박의 센서·전원·조종기·운항 설정 확인 |
| 기본 배치 | 기존 설정 트리/목록과 상세 패널 유지. PX4 AutoPilotPlugin이 제공하는 컴포넌트를 재사용 |
| 정보·조작 | 현재 펌웨어가 지원하는 GNSS/센서·전원·RC/조이스틱·모드·failsafe 설정과 기존 고급 파라미터 편집 |
| Boat 차이 | 실제로 적용되지 않는 항공기 전용 설정을 검토해 조정. Boat라는 이유만으로 공통 센서 보정이나 유효한 액추에이터 설정을 일괄 제거하지 않음 |
| 상태·완료 조건 | 파라미터 로딩·누락·기체 단절·armed로 인한 기존 제한 유지. 다른 기체로 전환하면 그 기체의 설정과 지원 항목으로 복원 |
| 검증 연결 | BOAT-02·09~10, T01·T11~T13·T15. 설정 변경이 생기면 기존 VehicleConfig UI 시험 확장 |

### UI-04 앱 설정·연결·오프라인 기체 선택: 기존 Settings/초기 설정

| 구성 | Boat에서의 화면 정의 |
| --- | --- |
| 목적·진입 | 기존 링크 설정으로 PX4에 연결하고, 미연결 상태에서는 Boat 계획 대상을 선택 |
| 기본 배치 | 기존 앱 설정·통신 링크 편집·초기 기체 선택 UI 유지. Boat 전용 연결 마법사를 새로 만들지 않음 |
| 정보·조작 | 기존 Serial/UDP/TCP 설정과 연결 상태, 지원 펌웨어 및 오프라인 Boat 선택, 기존 지도·단위·영상 설정 |
| Boat 차이 | Rover와 구분되는 오프라인 Boat 선택·보존을 제공. 사용자에게 메시지 ID·CRC 등 구현 세부사항을 입력하도록 요구하지 않음 |
| 상태·완료 조건 | 최초 실행·기체 미연결·연결 실패·해제·앱 재실행에서 선택을 검증. 현재 실기와 오프라인 편집 대상이 다르면 혼동 없이 기존 계획 전환 안내 사용 |
| 검증 연결 | BOAT-01~03·10, T02~T03·T07·T11·T13 |

### UI-05 출항 전 체크리스트: 기존 Fly View 팝업

| 구성 | Boat에서의 화면 정의 |
| --- | --- |
| 목적·진입 | 기존 도구 모음의 체크리스트 진입점에서 운항 전 확인 |
| 기본 배치 | 기존 팝업·항목 그룹·확인 방식 재사용. 별도 badaMux 출항 제어 화면으로 확장하지 않음 |
| 정보·조작 | 기존 GNSS·전원·통신 등 적용 가능한 항목, 필요 시 선박용 수동 확인 문구. 자동 상태와 운용자 확인을 구분 |
| Boat 차이 | 항공기 전용 문구를 조정하고 공통 Rover 모델 재사용 가능성부터 확인. 특정 엔진/러더 장비나 미지원 센서를 필수 항목으로 고정하지 않음 |
| 상태·완료 조건 | 닫기/재열기·기체 전환·연결 해제 처리. 기존 설정에 없는 새 강제 운항 차단을 임의로 추가하지 않고 체크 완료를 장비 정상 보증으로 표시하지 않음 |
| 검증 연결 | BOAT-10, T04·T11·T13·T15 |

### UI-06 명령 확인·진행·오류: 기존 공통 다이얼로그

| 구성 | Boat에서의 화면 정의 |
| --- | --- |
| 목적·진입 | 운항 액션을 선택할 때 실제 실행 의미와 결과 확인 |
| 기본 배치 | 기존 확인·슬라이더/버튼·메시지 안내 패턴 재사용 |
| 정보·조작 | 명령 대상·동작 설명·필요 입력, 요청 처리·거부·시간 초과·결과 미확인. 기존 구현에 별도 진행 UI가 없으면 우선 기존 상태/알림에서 정확하게 안내 |
| Boat 차이 | 미션 시작의 이륙 문구, Pause의 고도 입력 등 제거. 복귀를 접안 완료·추진 정지로 표현하지 않음 |
| 상태·완료 조건 | 확인창이 열린 상태의 기체 변경·단절·지원 상태 변경을 처리. 이전 기체로 실행되지 않고 중복 입력과 늦은 응답을 검증. ACK 수락을 물리 완료로 표시하지 않음 |
| 검증 연결 | BOAT-04~05·10, T04~T07·T13 |

### UI-07 분석·로그·메시지 상세: 기존 Analyze/상태 상세

| 구성 | Boat에서의 화면 정의 |
| --- | --- |
| 목적·진입 | 기존 메시지 상세와 설정에 따라 노출되는 Analyze 도구로 운항·통신 문제 확인 |
| 기본 배치 | 기존 로그·MAVLink Inspector·메시지 목록 사용. 기본 Boat 지원을 위해 새 장비 진단 화면을 만들지 않음 |
| 정보·조작 | 기존 수신 메시지·로그·펌웨어 상태와 오류. 추가 상태정보를 넣는 경우 기체·단위·유효성 의미를 유지 |
| Boat 차이 | 표준 메시지 수신이 장비 실측/최신성을 보장하는지 계약과 대조. 미지원 엔진·러더 항목을 정상 0으로 추가하지 않음 |
| 상태·완료 조건 | Boat에서도 기존 도구가 열리고 선택한 기체의 데이터만 표시. 관련 오류와 정상 복구를 추적할 수 있으며 기체 전환/로그 재생과 실시간 상태를 혼동하지 않음 |
| 검증 연결 | BOAT-10~11, T07·T10~T11·T13~T15 |

### 화면 정의 완료 기준

- 각 UI 항목의 관련 BOAT 작업·데이터 계약·QML·테스트를 연결해 누락을 확인한다.
- 정상 화면뿐 아니라 미연결·무효/만료·명령 거부·단절 화면의 동작도 정의한다.
- 위치·크기·색상 확정이 필요한 변경은 기존 QGC 화면을 바탕으로 변경 전후 시안/스크린샷을 준비한다.
- 체크리스트 추가, 계기판 기본 항목, 메뉴 문구 등 아직 제안인 부분은 upstream 리뷰와 실제 데이터 지원을 거쳐 확정한다.
- 구현 후 데스크톱·Android에서 화면별 증거를 남긴다. 문서 표를 작성한 것을 UI 구현 완료로 취급하지 않는다.

## 5. QML 수정 범위

표의 파일은 검토/수정 후보다. 기존 동작이 충분한 파일은 변경하지 않는다. 화면 레이아웃 전체 복제는 피한다.

| 영역 | 기존 QML | 수정 범위와 연계 C++ |
| --- | --- | --- |
| 명령 표시·확인·실행 | `src/FlyView/GuidedActionsController.qml`, `GuidedValueSlider.qml`, `GuidedActionPause.qml`, `GuidedActionRTL.qml` | Boat 조건, 이륙 문구, 고도 슬라이더, 중단 경로. 실행 판단은 Vehicle/FirmwarePlugin과 일치 |
| 액션 목록 | `src/FlyView/FlyViewAdditionalActionsList.qml`, `FlyViewToolStripActionList.qml` | 지원되지 않는 항공기 액션이 다른 진입점으로 노출되지 않는지 확인 |
| 계획 편집 | `src/PlanView/SimpleItemEditor.qml`, `MissionSettingsEditor.qml`, `MissionDefaultsEditor.qml` | Boat의 고도 입력·속도·기본값. SimpleMissionItem/SpeedSection/미션 메타데이터와 함께 조정 |
| 계획 상태·지형 | `src/PlanView/PlanView.qml`, `MissionStats.qml`, `TerrainStatus.qml` | Boat 계획이 불필요한 지형/고도 조회를 기다리지 않도록 모델과 UI를 함께 확인. 거리·예상시간 유지 |
| 계기판 | `src/FlyView/TelemetryValuesBar.qml`, `FlyViewInstrumentPanel.qml`, `MultiVehicleList.qml` | Boat 기본 항목, Heading/COG 구분, 선택한 선박의 데이터·유효성. 기본값 생성은 QGCCorePlugin |
| 상단 표시 | `src/Toolbar/FlightModeIndicator.qml`, `MainStatusIndicator.qml`, `ArmedIndicator.qml` | 운항에 적합한 문구. 모드/armed와 실제 이동·추진 상태를 혼동하지 않음 |
| 수동 조작 | `src/FlyView/VirtualJoystick.qml`, `src/Vehicle/VehicleSetup/JoystickComponent*.qml` | 조향·추진 표시와 입력 조건 검토. 펌웨어 매핑·단절 처리는 C++에서 검증 |
| 체크리스트 | `src/FlyView/PreFlightCheckList.qml`, `RoverChecklist.qml` | 공통 점검 재사용, 필요할 때만 Boat 모델 추가. 점검 완료를 장비 기능 보증으로 사용하지 않음 |
| 오프라인 설정 | `src/FirstRunPromptDialogs/`, `src/AppSettings/`, `src/Settings/`의 관련 선택 UI | 참조 위치를 추적해 Boat 선택과 타입 보존을 연결. JSON 문구 변경만으로 처리하지 않음 |

QML 공통 규칙:

- `activeVehicle` 부재·전환·연결 해제에 안전한 바인딩을 사용한다.
- 지원 기능과 상태는 C++의 `Q_PROPERTY`/Fact를 통해 제공한다. QML에 MAVLink 모드 숫자나 패킷 처리를 넣지 않는다.
- `QGCPalette`, `ScreenTools`, 기존 QGC 컨트롤과 `qsTr()`를 사용한다.
- 기존 사용자 계기판을 강제 초기화하지 않는다. Rover/Boat 저장 키 분리가 필요하면 마이그레이션을 설계한다.
- Boat가 아닌 기체의 화면과 번역 의미를 유지한다. 일괄 문자열 치환을 하지 않는다.
- 데스크톱·Android 가로/좁은 화면, 키보드 표시, 팝업·스크롤·터치 접근성을 확인한다.

## 6. 단계별 구현·테스트·완료 조건

| 단계 | 산출물 | 테스트 | 다음 단계 진입 조건 | 현재 상태 |
| --- | --- | --- | --- | --- |
| P0 기준·규약 | BOAT-01, 상태정보 계약의 필수 결정, 구현 전 실패 사례 | 실제 PX4 메시지 캡처/로그와 QGC 수신 경로 대조 | 필수 필드·지원 명령·무효/만료 정책 확정. 선택 장비의 미확정 항목은 기본 운항과 분리 | 초안 작성·환경 합의 대기 |
| P1 식별·계획 보존 | BOAT-02~03 | T01~T03, 기존 QGCMAVLink/PlanMasterController/MissionController 테스트 확장 | Boat 타입이 연결·오프라인·파일 왕복에서 보존되고 구형 파일이 계속 열림 | [x] 로컬 구현·자동 검증 완료; 수동·실기 인수 대기 |
| P2 운항 액션 | BOAT-04~05 | T04~T07, 모의 응답·거부·시간 초과, PX4 SITL/연결 시험 | 고도 없는 조작이 동작하고 성공/거부/결과 미확인 상태가 실제 보고와 일치 | [x] 로컬 구현·자동 검증 완료(208 CTest); PX4/SITL·실선 인수 대기 |
| P3 항로·속도 | BOAT-06~07 | T08~T09, SpeedSection/MissionController/PlanView 시험 | 업로드·다운로드·재저장 후 명령과 값이 유지되고 불필요한 지형 조회로 막히지 않음 | [x] 기본 WP·속도 로컬 구현·자동 검증 완료; 실제 PX4 인수 대기 |
| P4 계기판·공통 UI | BOAT-08~10, UI-01~UI-07 화면 정의 대조 | T10~T13, QML UI 시험·화면 증거·조이스틱 입력 모사 | 저장 배치·기체 전환·미수신/만료 표시·수동 입력 조건과 화면별 범위가 일치 | 기본 표시·배치·전체 단절 복구·가상 입력 세션 반영; 개별 만료·물리 조종·실기 인수 등 후속 |
| P5 통합·공식 기여 | BOAT-11~12 | T14~T15, 관련 플랫폼 빌드·회귀·CI | 미실행 항목/장비 제한 명시, 최신 upstream 기준으로 재검증한 검토 가능한 PR | 예정 |

P0~P5는 작업 순서이며 개발 기간을 뜻하지 않는다. 현재 상태 열은 2026-11-24 기준이다.
문서의 계획 상태, 코드 구현 완료, 자동 검증 완료, 실선 인수 완료, upstream 병합 완료를 별도로 기록한다.

### 테스트 매트릭스

| ID | 환경/시나리오 | 통과 기준 |
| --- | --- | --- |
| T01 | C++ 데이터 기반 분류: Boat/Rover/Multirotor/FixedWing/VTOL/Sub/Unknown | Boat 판별 정확, 기존 클래스 의미 유지, 타입 변경 알림 확인 |
| T02 | 오프라인 Boat 선택·앱 재실행·연결/해제 | 설정과 controllerVehicle의 실제 타입 유지, 다른 기체로 누출 없음 |
| T03 | 신규/구형 `.plan` 저장·재열기·다른 기체 파일 | Boat 보존, 구형 파일 호환, 지원하지 않는 명령을 조용히 변조하지 않음 |
| T04 | Boat의 모든 액션 진입점, 기체 없음·armed/비armed·미션 상태 | 이착륙/고도 조작 제외, 기능·상태에 맞는 버튼과 실행 조건 |
| T05 | 정상 위도·경도, 고도 미수신/무효의 Go To·Pause | 불필요한 고도 슬라이더/검사 없음. 필수 위치가 무효하면 명확하게 거부 |
| T06 | 시작·중단·재개·RTL 정상/거부/IN_PROGRESS/시간 초과 | 응답과 실제 모드·미션 상태를 구분. 결과 불명을 성공으로 표시하지 않음 |
| T07 | 명령 직후 단절·재연결·늦은 응답·다중 링크 | 다른 세션/선박의 응답 오적용과 이전 명령 자동 재실행 없음 |
| T08 | Boat 속도 지정/변경 없음·경계값·ground speed | 생성·재읽기·저장 왕복·기체 왕복 일치, FixedWing/Multirotor 회귀 없음 |
| T09 | 고도 UI·지형 미수신·항공기 명령 포함 파일 | 기본 WP 편집/전송 가능. wire frame/고도는 PX4 계약대로 유지, 일괄 0 치환 없음 |
| T10 | Heading≠COG, 유효한 0, NaN/sentinel, 저속·지연·센서 단절 | 두 방향 구분. 미수신/무효/만료를 실제 0/정상으로 표시하지 않음 |
| T11 | 사용자 계기판 편집 후 Boat↔Rover↔UAV 전환 | 사용자 배치 보존, 선택 기체 정보만 표시, null 바인딩 오류 없음 |
| T12 | 가상/물리 조이스틱·축 반전·중립·입력 상실 | 매핑·범위·역추진 지원 조건 일치, 기체 전환 후 이전 입력 누출 없음 |
| T13 | UI-01~UI-07 각각에서 데스크톱·Android 가로/좁은 화면·팝업·단절 | 화면별 구성·지원 조건·조작 접근 유지, 번역·색상 외 상태 구분, QML 경고 확인 |
| T14 | PX4 SITL 및 실제 선박: 항로 전송→시작→이동→중단→재개→RTL | 요청·PX4 보고·실제 운항 증거가 일치. 패킷 시험과 물리 동작 시험 결과를 분리 |
| T15 | PX4/ArduPilot의 기존 Rover·항공기 및 Boat, 필요 시 Sub | 공통 변경으로 기존 운용이 손상되지 않음. 미지원/미실행 조합은 명시 |

### P1 구현 기록 — 2026-11-17

작업 브랜치는 `feat/boat-identity-offline-plan`이며, `feature/boat`의 `81a1611e7`에서 분기했다.
기준은 Stable v5.1.5(`3a67d31f0`)와 기존 계획 문서 커밋이다.
P1 구현·테스트 커밋은 `e7818cf5e`, 테스트 실행 기반 보완 커밋은 `1e2f8f5b5`다.

- `QGCMAVLink::isBoat()`와 QML용 `Vehicle.boat`를 추가했다. Boat의 `rover` 값은 계속 true이며,
  `VehicleClassRoverBoat`와 펌웨어·명령 메타데이터의 기존 공통 분류는 유지한다.
- 기존 `preferredVehicleClass`·`offlineEditingVehicleClass` 키와 기존 숫자는 유지하고,
  대표 MAV_TYPE 선택지에 Boat(11)를 추가했다. 기존 Rover(10)를 Boat로 추정해 바꾸지 않는다.
- 연결 기체와 파일에서 오프라인 설정으로 타입을 복사할 때 Boat를 보존한다.
  다른 기체의 기존 대표 타입 정규화와 `.plan`의 실제 `vehicleType` 저장 형식은 유지한다.
- `vehicleType`이 없는 구형 mission v2 파일은 현재 오프라인 선택을 사용한다.
  편집 중인 Boat 계획은 다른 기체 연결·해제 시 기존 전환 안내와 함께 유지되고,
  계획을 비우면 마지막 연결 기체의 오프라인 타입으로 돌아간다.
- QML 화면 파일은 변경하지 않았다. 기존 Fact 기반 선택 UI가 Boat 항목을 표시하며,
  Plan View UI 테스트에 Boat를 추가해 기존 Rover 흐름과 함께 검증한다.

자동 테스트 추가 범위:

| 테스트 | P1 검증 내용 |
| --- | --- |
| `QGCMAVLinkTest` | Boat/Rover/멀티로터/Hexa/FixedWing/VTOL/Sub/Generic/Unknown 판별과 대표 타입 |
| `AppSettingsTest` | 선택지·타입 알림·QML 속성·QSettings 저장·오프라인 Vehicle 재생성·선호 기체 연동 |
| `PlanMasterControllerTest` | PX4 Boat/Rover/드론 및 기존 APM Rover/드론/Sub 연결·해제, 파일 왕복 16행, 편집 중 Boat 계획 보존 |
| `TestFixturesTest` | 테스트 설정 임시 변경 후 Boat 타입 복원 |
| `PlanViewUITest` | Rover와 Boat 각각에서 빈 계획에 WP 추가, 이륙 도구 숨김과 Return 버튼 문구 |

실행 환경은 macOS 26.6.2 arm64, AppleClang 21, Qt 6.11.1, Debug이며 PX4/APM 플러그인을 모두 포함한다.
MAVLink/XML 기준 커밋은 `c409cf690454db6d3e004bd14173bc6c7ff1e0ff`이며 실제 PX4 펌웨어는 실행하지 않았다.
영상과 무관한 P1 검증을 위해 `QGC_ENABLE_GST_VIDEOSTREAMING=OFF`를 사용했다.
이 구성에서 발견한 기존 테스트 기반 문제도 보완했다:
GStreamer 비활성 테스트의 누락된 skip 정의 10개와 초기화 중 skip으로 인한 cleanup 누락,
호스트 언어에 따라 영문 문자열 검증이 실패하는 문제다. 영어 고정은 자동 테스트 실행에만 적용한다.

검증 결과와 제한:

- QGC 앱·테스트 Debug 빌드 통과. 관련 CTest 7개 묶음 통과.
- 전체 Unit CTest 197개 통과. 비활성 GStreamer 항목은 해당 테스트 내부에서 skip한다.
- 변경한 C++ 구간의 clang-format 22.1.5 검사와 `git diff --check` 통과.
  관련 C++ 소스 4개의 Clang-Tidy 실행은 오류 없이 완료했으며 기존 경고는 남아 있다.
- 변경 파일 대상 pre-commit 전체 통과로 기록하지 않는다. 기존 전체 파일 포맷 위반과
  `Vehicle.cc`의 기존 null 검사 경고 2건을 변경 전 코드에서도 재현했다.
  중첩 빌드 디렉터리와 AppleClang PCH/LLVM 분석 도구 차이는 별도 분석용 명령으로 처리했다.
- QSettings 저장과 Vehicle/PlanMasterController 재생성을 검증했다.
  실제 GUI 앱 종료·재실행, Android, PX4 SITL·실선 운항과 영상 기능은 이번 검증 범위에 포함하지 않는다.
  APM Boat/VTOL 연결 모의 구성은 현재 Copter 파라미터를 사용하므로 연결 시험 대상에서 제외했다.
  T01~T03의 자동 검증을 실제 장비 인수나 공식 지원 완료로 간주하지 않는다.

로컬 실행 명령:

환경 설치·앱 실행·화면별 수동 확인은 [P1 테스트 가이드](P1_TESTING.md)를 따른다.
같은 가이드의 6장에는 실제 MAV_TYPE과 공통 클래스의 차이, Boat 선택·연결 식별,
QSettings/계획 파일 저장·복원 및 P1 변경 전후의 ASCII 흐름도와 관련 코드 위치를 정리했다.

```bash
cmake --build build/boat-p1 --parallel 6
ctest --test-dir build/boat-p1 --output-on-failure --parallel 2 \
    -R '^(QGCMAVLinkTest|AppSettingsTest|TestFixturesTest|PlanMasterControllerTest|MissionControllerTest|SpeedSectionTest|PlanViewUITest)$'
ctest --test-dir build/boat-p1 --output-on-failure --parallel 4 -L Unit
```

빌드·CTest·정적 분석·pre-commit 원본 로그는 gitignore 대상인
`build/boat-p1/boat-p1-validation/`에 보관한다. pre-commit은 기존 전체 파일 포맷/null 경고 및
기본 compile database 경로 문제로 실패했으므로, 통과한 개별 검사와 구분해 확인한다.

### P2 구현 기록 — 2026-11-18

`feat/boat-navigation-actions`에서 Boat 운항 액션과 회귀 테스트를 구현했다.
Boat의 이착륙·고도 변경을 차단하고, 고도 없는 Pause/Go To, 비행 상태와 분리된 운항·미션 재개 조건,
확인 대상 변경·통신 단절 시 취소 및 실행 재검사를 추가했다. Rover·드론의 기존 액션 경로와 4인자 실행 API는 유지한다.
새 액션 테스트 2개와 기존 미션·명령·Fly/Plan UI 회귀 10개, 전체 Unit 197개가 통과했다.
최종 구현에서는 이들의 중복을 제거한 CTest 208개를 함께 실행해 모두 통과했다.
펌웨어 지원 확인·실선 인수와 lint 제한을 포함한 상세 내용은 [P2 구현·테스트 기록](P2_TESTING.md)에 정리했다.
고도 편집 UI와 아이콘 변경은 P3/P4에서 진행한다.

### P3 구현 기록 — 2026-11-23

`feat/boat-mission-planning`은 P2 `b789e7fb5`를 기반으로 한다.
Boat의 기본 고도 UI·지형 그래프를 숨기고 거리·예상시간 통계를 유지했다.
시작/WP 속도를 ground speed 명령으로 생성하고 파일과 MockLink 왕복 후 Section으로 복원한다.
오프라인 타입 변경 시 기체별 속도 가용 조건을 다시 적용하며, Rover·VTOL에 Boat의 가용 상태가 남지 않는다.
가져온 항공기 명령은 저장 시 보존하고 업로드 전에 거부한다. 거부 후 미전송 변경 상태를 유지하며 수정·재시도가 가능하다.

새 계획·QML 검사와 P2 액션/미션 왕복, 기존 계획·명령·UI·Unit을 포함한 관련 CTest 212개가 통과했다.
전체 Unit CTest 198개도 통과했으며 두 실행의 중복을 제외하면 213개다.
상세 변경·재현·검증 제한은 [P3 구현·테스트 기록](P3_TESTING.md)을 따른다.
실제 PX4/SITL·실선, Android, 측량 등 복합 패턴의 Boat 전용 지원, 전체 lint와 공식 upstream 반영은 완료하지 않았다.

### P4 기본 표시 구현 기록 — 2026-11-23

`feat/boat-ui`는 P3 `35c837609`를 기반으로 한다.
Boat 지도 아이콘·타입 변경 알림, 기본 대지속도/Heading/GPS COG/거리/복귀 예상시간 계기판과 기체 카드,
사용자 배치의 최초 1회 이관·Boat 전용 저장·초기화, 상단 armed 상태 문구를 구현했다.
기존 Rover·드론 배치와 기본 구성을 보존하며 Heading을 COG나 실제 추진 상태로 바꾸어 해석하지 않는다.

최종 기본 표시·텔레메트리·공통 점검 변경에서 전체 Unit 200개를 포함한 관련 CTest 217개가 통과했다.
상세 동작·저장 키·수동 확인·미실행 범위는 [P4 구현·테스트 기록](P4_TESTING.md)을 따른다.
추가로 Boat 초기 미수신과 무효 대지속도를 NaN으로 유지하고, raw m/m/s 기반의 복귀 예상시간과 Heading/COG 독립성을 검증했다.
공통 체크리스트의 Boat 문구와 기체/타입 변경 시 점검 상태·지연 콜백 초기화도 구현했다.
P4 전체는 완료하지 않았다. 상태 유효성의 나머지 조건·만료 정책, 실제 수동 조종, 선박별 장비 점검,
Android/좁은 화면과 실기 인수는 후속이며 전체 lint 및 upstream 승인도 미완료다.

### P4 연결 복구 보완 — 2026-11-24

`feat/boat-link-recovery`에서 전체 통신 단절 시 Boat 기본 계기 값을 NaN으로 전환하고,
heartbeat만 복구했을 때 이전 값이 재표시되지 않게 했다. 지도 좌표는 보존하되 새 위치 수신 전에는
Home 거리·복귀 예상시간 계산에 사용하지 않는다. 위치·속도 수신 순서와 같은 좌표의 재수신을 처리한다.
Boat의 GPS/자세 소스 우선순위는 새 연결에서 다시 학습하며 Rover·항공기의 기존 처리는 유지한다.
항목별 stream timeout과 원천 센서 만료는 계속 미확정이다.
검증 방법과 범위는 [P4 테스트 기록](P4_TESTING.md)의 연결 복구 절을 따른다.

### P4 가상 조이스틱 보완 — 2026-11-24

`feat/boat-virtual-joystick`에서 Boat의 기체 전환·단절·화면 숨김·설정 변경 후 이전 가상 입력을 취소하고,
새 조작으로만 송신을 재개하게 했다. 취소된 터치의 지연 release와 resize에서도 축 값을 유지하지 않는다.
C++ 진입점은 비활성/초기 연결/단절 Boat 및 비유한·범위 밖 축 입력을 거부한다.
기존 물리 조이스틱 우선순위·축 매핑·사용자 설정은 유지한다.
T12의 가상 입력 세션과 기존 기체 회귀를 자동 검증하며 실제 중립·역추진·물리 입력 인수는 남아 있다.
상세 범위는 [P4 테스트 기록](P4_TESTING.md)의 가상 조이스틱 절을 따른다.

### P4 GPS COG 입력 유효성 보완 — 2026-11-24

`feat/boat-gps-validity`에서 Boat의 GPS/보조 GPS COG를 fix enum·각도 범위·sentinel로 검증한다.
유효한 0도와 미지원/무효 값을 구분하며 이전 값을 지우고 정상 샘플로 복구한다.
Heading·GPS 간 독립성과 기존 Rover·항공기 처리, Vehicle 문맥이 없는 사용을 보존한다.
이는 T10의 입력 검증 보완이며 저속 품질·개별 stream 만료·원천 센서 생존성은 남아 있다.
상세 행렬과 근거는 [P4 테스트 기록](P4_TESTING.md)의 GPS COG 절을 따른다.

### 실행 방법과 증거

실제 구현 시 [테스트 가이드](../../../test/README.md)와 [CI 가이드](../../../.github/ci-overview.md)를 따른다.
기존 `test/MAVLink/QGCMAVLinkTest.*`, `test/MissionManager/SpeedSectionTest.*`,
`PlanMasterControllerTest.*`, `MissionControllerTest.*`, `test/QmlUITests/PlanViewUITest.*`를 먼저 검토한다.
Boat 시나리오에 필요한 모의 기체/메시지 지원을 추가하고, 구현을 그대로 복제하는 테스트는 만들지 않는다.

```bash
just configure
just build
ctest --test-dir build --output-on-failure -R 'QGCMAVLinkTest|SpeedSectionTest|PlanMasterControllerTest|MissionControllerTest|PlanViewUITest'
ctest --test-dir build --output-on-failure -L Unit
just lint
just check
```

위 명령은 이후 단계에도 사용할 기본 실행 방법이다. 구성에 따라 실제 테스트 이름·GUI 실행 조건을 먼저 확인한다.
P1의 로컬 실행은 `build/boat-p1`에서 수행했고, 결과와 미실행 범위는 위 구현 기록에 구분했다.
각 결과에는 QGC/PX4/XML 커밋, OS·장비, 재현 절차, 입력 메시지, 예상/실제 결과,
로그·화면 증거, 통과/실패/미실행·제한을 남긴다. SITL 결과는 실제 추진·조향 인수를 대체하지 않는다.

## 7. PR 분리와 최종 완료 기준

PR 후보는 P1 식별·파일 보존, P2 운항 액션, P3 항로·속도, P4 표시 개선으로 나눈다.
저장 형식이나 상태 유효성의 범용 변경이 커지면 별도 선행 PR로 제안한다.
상호 의존하는 생성/읽기 코드는 같은 PR에서 수정한다. upstream 대상은 기여 가이드에 맞춰 최신 master로 재확인한다.

각 기능 단계의 구현·관련 자동 검증을 마치면 해당 `feat/*` 브랜치에 커밋·push한다.
미실행 실기 검증과 lint 제한은 문서에 남기고 최종 인수 완료로 표시하지 않는다.

`feature/boat`는 통합 브랜치로 유지하고, 개별 PR은 독립적으로 검토 가능한 기능 단위의 `feat/*` 브랜치로 진행한다.
P1의 식별·저장·복원은 하나의 변경으로 다루며, 후속 단계는 `feat/boat-navigation-actions`,
`feat/boat-mission-planning`, `feat/boat-ui` 등으로 분리할 수 있다.
공식 제출 시에는 최신 upstream master 기반 브랜치에 해당 코드·테스트 커밋만 옮기고,
badaMux 제품 명세와 독립적인 테스트 기반 보완은 별도 변경으로 분리한다.

- [ ] BOAT-01~11의 필수 완료 조건 충족, 선택 기능과 미실행 조합 공개.
- [ ] PX4 무인선에서 기본 운항 흐름과 연결 복구 확인.
- [ ] Rover·항공기 회귀, 사용자 저장 설정·구형 계획 호환 검증.
- [ ] 변경 플랫폼의 빌드·관련 테스트·Unit·lint/CI 결과 확보.
- [ ] 공식 기여용 영문 사용자 문서와 변경 전후 UI 증거 준비.
- [ ] upstream PR URL·리뷰 상태·병합 커밋 기록. 로컬 구현 완료를 공식 지원 완료로 표기하지 않음.

## 8. 참고

- [QGC 기여 가이드](../../../.github/CONTRIBUTING.md)
- [QGC Fact System](../../en/qgc-dev-guide/fact_system.md)
- [MAVLink 메시지 정의](https://mavlink.io/en/messages/common.html)
- [PX4 MAVLink 스트리밍](https://docs.px4.io/main/en/mavlink/streaming_messages)
