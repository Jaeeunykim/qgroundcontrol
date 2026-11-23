# Boat P4 기본 표시·사용자 배치 구현·테스트 기록

- 작성일: 2026-11-23
- 브랜치: `feat/boat-ui`
- 기준: P3 `35c837609`, Stable v5.1.5 기반
- 상태: 기본 표시 범위 로컬 구현·자동 검증 완료. P4 전체 인수 완료가 아님
- 환경: macOS 26.6.2 arm64, AppleClang 21, Qt 6.11.1, Debug, PX4/APM 포함, GStreamer 비활성

## 1. 구현한 기능

| 항목 | Boat 처리 | 기존 기체·사용자 설정 |
| --- | --- | --- |
| 지도 아이콘 | 선수 방향이 위인 Boat opaque/outline SVG 제공. 기존 Heading 회전 경로 사용 | 기본 FirmwarePlugin의 비Boat 아이콘과 커스텀 플러그인의 virtual override 유지 |
| 타입 변경 | `Vehicle.vehicleImageOpaque/Outline`에 `vehicleTypeChanged` 알림 적용 | Rover↔Boat 변경 시 QML 바인딩이 실제 리소스로 갱신 |
| 새 기본 계기판 | 대지속도·Heading·GPS COG·Home 거리·다음 WP 거리·복귀 예상시간 | Rover·Multirotor·FixedWing·VTOL의 기존 기본 구성 유지 |
| 기체 카드 | 대지속도·Heading 두 항목 | 기존 Rover·항공기 카드 구성 유지 |
| 사용자 배치 | Boat 전용 저장 키, 기존 공용 Rover/Boat 배치 최초 1회 복사 | 기존 공용 배치는 삭제·수정하지 않으며 이후 Boat 편집은 별도 저장 |
| 기본값 초기화 | Boat 배치만 기본값으로 초기화 | Rover·드론 배치 보존, 이전 배치를 매번 다시 가져오지 않음 |
| 공통 체크리스트 | Boat 초기 점검·출항·수상 항로 문구, 기체/타입 변경 시 점검 상태·지연 콜백 초기화 | 기존 Rover 점검 항목·임계값과 다른 기체의 모델 선택 유지 |
| 상단 상태 | Boat가 `flying/landing` 플래그를 보고해도 `Armed`로 표시, 공중 상태로 해석하지 않음 | Rover·드론 기존 문구·조건 유지, 단절/복구·기체 없음 표시 유지 |

`Armed`는 PX4의 armed 상태다. 실제 이동·엔진 시동·추진 정지를 새로 추정하지 않는다.
Heading과 GPS COG는 별도 Fact로 표시한다. Heading은 기존 ATTITUDE 경로, COG는 기본 GPS Fact의
`GPS_RAW_INT.cog` 경로를 사용한다. COG 미지원 sentinel은 기존 GPS 처리대로 NaN이며 실제 0도는 유효 값이다.
수신 만료 정책은 후속 범위다.

## 2. 저장 설정 이관

기존 키는 `<settingsGroup>-<vehicleClass>`이고 Rover와 Boat가 같은 클래스를 사용했다.
다른 기체의 기존 키는 유지하며 Boat만 다음 키를 사용한다.

```text
기존 Rover/Boat 공용 배치
  <settingsGroup>-<VehicleClassRoverBoat>
                 |
                 | Boat 최초 사용: 기존 사용자 배치가 있으면 복사
                 v
Boat 배치: <settingsGroup>-Boat
초기화 이력: <settingsGroup>-BoatInitialized

Rover: 기존 키 유지
Boat:  새 키로 편집·저장·복원
```

- 이미 Boat 배치가 있으면 기존 공용 배치로 덮어쓰지 않는다.
- 사용자가 저장한 배치는 고도 항목을 포함하더라도 강제로 교체하지 않는다.
- 새 Boat 기본 구성을 원하면 Boat를 선택한 상태에서 계기판의 기본값 초기화를 사용한다.
- 이관 후 Boat만 초기화하면 다음 실행/기체 전환 시 옛 공용 배치를 다시 복사하지 않는다.
- 기체 카드들은 각 기체가 읽어야 할 배치 키를 사용한다. Boat 카드 편집으로 Rover·드론 배치가 바뀌지 않는다.

## 3. 검증

| 테스트 | 검증 내용 |
| --- | --- |
| `BoatPresentationTest` | Boat/Rover/Multirotor/FixedWing/VTOL × 기본 바/기체 카드 10행 |
| 같은 테스트 | Boat/기존 아이콘 경로·SVG 렌더링, 타입 변경 알림과 실제 QML 바인딩 갱신 |
| 같은 테스트 | 기존 배치·폰트 이관, 원본 보존, 기존 Boat 배치 우선, 재생성·전환·Boat만 초기화 |
| 같은 테스트 | Boat·Rover·드론 카드 동시 표시 시 Fact·사용자 편집의 기체 간 분리 |
| 같은 테스트 | 기존 Fact의 NaN과 유효한 대지속도 0·Heading 0 구분. 수신 만료 검증을 대신하지 않음 |
| `BoatTelemetryTest` | 실제 VFR_HUD/ATTITUDE/GPS_RAW_INT 파싱으로 초기 미수신·무효 속도·유효한 0, Heading≠COG와 GPS sentinel 검증 |
| 같은 테스트 | m/ft·m/s/knots와 무관한 복귀 예상시간, 무효 거리/속도 후 이전 예상값 제거·회복, 기존 Rover·드론 경로 유지 |
| `BoatChecklistTest` | 실제 체크리스트의 Boat/Rover/Multirotor/FixedWing/VTOL/Sub 전환, 통과 상태 분리·지연 콜백 취소·동일 객체 타입 변경·null 처리 |
| `BoatStatusTest` | 실제 MainStatusIndicator에서 Boat armed/flying/landing, Rover·멀티로터·고정익·VTOL, 단절/복구·null 상태 |

최종 기본 표시·텔레메트리·공통 점검 변경에서 전체 Unit 200개를 포함한 관련 CTest 217개가 통과했다.
최종 `ctest -L Unit` 단독 실행도 200개 모두 통과했다.
원본 로그와 목록은 로컬 `build/boat-p3/boat-p4-validation/`과
`build/boat-p3/boat-telemetry-validation/`, `build/boat-p3/boat-checklist-validation/`에 보관한다.

- Mac 앱·테스트 빌드 통과.
- 변경 생산 C++ 3개 Clang-Tidy 종료 코드 0. 기존 경고는 남아 있으며 분석용 compile database에서 AppleClang PCH를 제외했다.
- 변경 QML/하네스는 공식 Qt qmllint 종료 코드 0, 기존 경고 유지.
- 기본 표시 범위의 CMake lint, Q_ASSERT·로그·null·번역·테스트 대기/메시지 정책 검사 통과.
  텔레메트리 보완 범위의 null 검사에는 `Vehicle.cc`의 기존 `getParameter()` 경고 2개가 남으며 기준 코드에서도 재현했다.
  나머지 해당 정책 검사와 CMake lint는 통과했다.
- 새 C++ 테스트는 clang-format 검사 통과. 기존 파일 전체 포맷 위반 등으로 전체 lint/CI 통과로 기록하지 않는다.
- SVG는 Qt로 렌더링해 형태·방향·두 스타일을 시각적으로 확인했다.
- GStreamer 비활성 테스트 내부 skip은 영상 기능을 검증한 결과가 아니다.

### 추가 보완: 미수신·무효 값과 방향 분리

- Boat 생성 또는 타입 11 진입 시 Heading·대지속도·다음 WP 거리·복귀 예상시간을 미수신 NaN으로 초기화한다.
  Home 거리는 현재 좌표와 Home의 유효성으로 다시 계산한다. Rover·드론의 기존 초기값은 유지한다.
- Boat의 VFR_HUD 대지속도가 NaN·무한대·음수이면 0으로 채우지 않고 NaN으로 표시한다.
  대지속도는 크기 값이므로 음수를 기어/역추진 상태로 해석하지 않는다.
- 대지속도 0은 유효한 정지 속도다. 이때 복귀 예상시간은 계산하지 않으며 NaN이다.
- 복귀 예상시간은 현재 대지속도로 Home까지 직선 이동한다고 가정한 시간이다.
  raw 거리(m)/raw 속도(m/s)로 계산하므로 화면의 feet·knots 선택이 시간을 바꾸지 않는다.
- 거리 또는 속도가 무효하면 이전 추정값을 지우며, 유효한 샘플을 다시 받으면 계산을 재개한다.
- 기본 계기판의 GPS Course Over Ground와 Heading을 서로 덮어쓰거나 대신하지 않는다.
- 이 보완은 명시적인 미수신/무효 상태 처리다. 메시지별 타임아웃·지연·단절 후 값 만료의 전체 구현은 아니다.

### 추가 보완: 공통 체크리스트와 기체 전환

- Rover/Boat 공통 점검 모델을 계속 재사용하며 Boat 제목·출항·수상 항로 확인 문구를 구분한다.
  기존 배터리/GPS/RC 점검 기준을 임의로 바꾸거나 새로운 선박 장비의 자동 점검을 가정하지 않는다.
- 기체 포인터 또는 타입이 바뀌면 이전 점검 통과 상태를 해제하고 현재 타입의 모델을 선택·초기화한다.
- 이전 그룹의 통과 지연 타이머를 중지하고 빈 모델·전환 중 생성되지 않은 그룹을 통과로 처리하지 않는다.
- 연결 기체가 없어도 체크리스트 상태를 null 기체에 쓰지 않는다.
- 같은 기체의 화면을 다시 여는 기존 흐름과 다른 기체의 모델 선택은 유지한다.

## 4. 실행과 수동 확인

P4 로컬 검증은 기존 `build/boat-p3` 빌드 디렉터리를 재사용했다.
디렉터리 이름은 실행 파일의 구현 단계를 보장하지 않으므로 현재 브랜치와 빌드 결과를 함께 확인한다.

```bash
git branch --show-current
cmake --build build/boat-p3 --parallel 6
open -n build/boat-p3/Debug/QGroundControl.app --args --allow-multiple
ctest --test-dir build/boat-p3 --output-on-failure --parallel 2 \
    -R '^(BoatPresentationTest|BoatStatusTest|BoatGuidedActionsTest|BoatPlanEditorsTest|ToolbarIndicatorUITest)$'
```

1. Boat를 선택하고 기존 사용자 배치가 없을 때 대지속도·Heading·COG·거리·복귀 예상시간 중심의 기본값을 확인한다.
2. 기존 배치가 있다면 그대로 이관되는지 확인한다. Boat만 초기화한 뒤 Rover·드론 배치를 비교한다.
3. 기존 장비 연결에서 타입 11을 보고할 때 지도에 Boat가 표시되고 Heading으로 회전하는지 확인한다.
4. 미수신 값과 실제 0 속도/0도 Heading, 통신 단절과 복구를 각각 확인한다.
5. armed 상태를 실제 이동·시동·중립·정지 여부로 해석하지 않고 펌웨어·장비 상태와 별도로 대조한다.

## 5. 남은 P4 범위

- 원천별 유효성의 나머지 조건·수신 만료·복구 정책과 펌웨어 메시지 계약 확정.
- 실제 조이스틱의 조향·추진·중립·역추진·입력 상실과 제어권 검증.
- 선박·운용환경별 장비 점검 항목·임계값·체크리스트 인수와 나머지 화면 문구·접근성 검토.
- Android/좁은 화면, 실제 PX4/SITL·실선, 사용자 장기 저장 설정에 대한 수동 인수.

기본 표시의 자동 검증을 P4 전체 완료, 실제 선박 인수 또는 공식 QGC 지원 승인으로 기록하지 않는다.
