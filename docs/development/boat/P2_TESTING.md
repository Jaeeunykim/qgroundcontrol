# Boat P2 운항 액션 구현·테스트 기록

- 작성일: 2026-11-18
- 브랜치: `feat/boat-navigation-actions`
- 기준: P1 `2a7d8a99e`, Stable v5.1.5 기반
- 환경: macOS 26.6.2 arm64, AppleClang 21, Qt 6.11.1, Debug, PX4/APM 플러그인 포함, GStreamer 비활성
- 범위: BOAT-04~05와 T04~T07의 로컬 구현·자동 검증. 실제 PX4 펌웨어 버전과 실선 인수는 미확정

## 1. 동작 변경

| 항목 | Boat 처리 | Rover·드론 호환 |
| --- | --- | --- |
| Takeoff·Land·고도 변경·Land Abort·고도 입력 Orbit | 액션 표시·실행 차단. Vehicle API의 이착륙·고도 변경·Land Abort도 거부 | 기존 타입 조건과 경로 유지. Orbit은 기존 지원 기능 검사도 유지 |
| 단일 기체 Pause | 고도 슬라이더와 고도 변경 요청 없이 `Vehicle.pauseVehicle()` 사용 | 기존 Pause의 고도 선택·변경 경로 유지 |
| Go To·Pause·Return | Boat는 `flying` 대신 armed와 해당 지원 기능을 기준으로 노출 | 기존 flying 조건 유지 |
| Pause 표시 | PX4 Hold 모드에서도 사용 가능. Go To와 Pause가 같은 모드이므로 모드명만으로 정지를 판단하지 않음 | 기존 paused 판별 유지 |
| 미션 시작·계속 | Boat 시작 안내의 Takeoff 문구 제거. armed 상태에서 미션 계속 사용 | 기존 안내와 표시 조건 유지 |
| 미션 재개 | Boat가 armed 상태로 미션 모드에 진입한 이력을 추적. 비행 이력을 요구하지 않음 | 기존 비행 이력 조건 유지 |
| 선택 기체의 미션 시작 안내 | 선택 목록에 Boat가 포함되면 이륙을 전제로 하지 않는 문구 사용 | 항공기만 선택한 기존 문구 유지 |
| 액션 확인 | 대상 기체 기록, 기체 전환·통신 단절 시 취소, 실행 시 대상·상태 재확인, 확인 소비 후 재실행 차단 | 같은 확인창 보호를 Rover·드론에도 검증 |

기존 `executeAction(actionCode, actionData, sliderOutputValue, optionChecked)` 4인자 API를 유지했다.
확인창은 대상 기체까지 검증하는 `executeConfirmedAction()`을 사용한다.
기체가 바뀌거나 통신이 복구되어도 이전 확인창·지연 표시를 자동으로 다시 실행하지 않는다.
Boat 미션 이력도 기체 전환 시 초기화한다.

## 2. PX4 명령 경로

```text
Boat Go To
  -> Vehicle: current/target coordinate validity + maximum distance checks
  -> PX4FirmwarePlugin::guidedModeGotoLocation()
  -> DO_REPOSITION: target latitude/longitude, altitude = NaN
       COMMAND_INT available -> MAV_FRAME_GLOBAL, x/y scaled by 1e7
       otherwise             -> existing COMMAND_LONG fallback

Boat Pause
  -> Vehicle::pauseVehicle() / firmware capability check
  -> PX4FirmwarePlugin::pauseVehicle()
  -> existing DO_REPOSITION: yaw/latitude/longitude/altitude = NaN
  -> no altitude slider, no altitude-change follow-up command

Mission / Return
  -> existing PX4 mission-mode + arming validation / RTL-mode validation
  -> no new custom flight mode or Boat-specific dialect
```

Boat Go To의 고도 NaN은 고도를 0으로 지정하는 의미가 아니다.
PX4 Navigator는 DO_REPOSITION의 유한하지 않은 고도에 현재 고도를 사용한다.
이 처리는 [PX4 Navigator 소스](https://github.com/PX4/PX4-Autopilot/blob/main/src/modules/navigator/navigator_main.cpp)에서
2026-11-18 정적으로 대조했다. 실제 선박의 펌웨어 커밋에서 같은 처리를 별도로 확인해야 한다.
명령 필드는 [MAVLink DO_REPOSITION 정의](https://mavlink.io/en/messages/common.html#MAV_CMD_DO_REPOSITION)를 따른다.
이 고도 처리는 PX4 플러그인의 Boat 분기에만 적용하고 Rover·항공기의 기존 고도 요구를 유지했다.

액션 호출 성공은 요청을 보냈다는 뜻이다. ACK 수락·실제 모드 보고·도착·물리적 정지는 별도로 취급한다.
거부·미지원·시간 초과는 기존 Vehicle/MavCommandQueue의 결과·오류 메시지 경로로 처리한다.
새 테스트는 ACK만 수락하고 상태를 갱신하지 않는 모의 기체에서 운항 완료를 추정하지 않는지 확인한다.

## 3. 자동 테스트

| 테스트 | 시나리오 |
| --- | --- |
| `BoatNavigationTest` | Boat/Rover/Multirotor/FixedWing/VTOL, 고도 있음/없음, COMMAND_LONG/INT 조합 20행 |
| 같은 테스트 | 고도 없는 Pause, Boat의 항공기 액션 거부, 미션 시작·Return, 무효 좌표·거리 제한 |
| 같은 테스트 | ACK 수락·거부·미지원·무응답·IN_PROGRESS 후 수락/실패와 결과 구분 |
| `BoatGuidedActionsTest` | 실제 QML 컨트롤러·확인창·Takeoff 도구를 사용한 표시 조건, 고도 슬라이더, 실행 경로 |
| 같은 테스트 | Boat/Rover/드론의 확인 중 기체 전환·단절/복구, 기능 변경, 지연 표시 취소, 중복 확인 차단 |
| 같은 테스트 | Hold에서 Boat Pause, 비행 상태 없는 미션 재개, 혼합 선택 기체의 문구 |

MockLink에 DO_REPOSITION 응답·무응답을 지정하는 테스트 API를 추가했다.
기본 응답은 기존과 같은 UNSUPPORTED이며 ACK 수락만으로 모드나 위치를 바꾸지 않는다.
QML 테스트의 텔레메트리·명령 종단은 결정적인 스텁이고, 실제 wire 명령은 별도 MockLink 테스트로 검증한다.

```bash
ctest --test-dir build/boat-p2 --output-on-failure --parallel 2 \
    -R '^(BoatNavigationTest|BoatGuidedActionsTest)$'
ctest --test-dir build/boat-p2 --output-on-failure --parallel 2 \
    -R '^(APMStartMissionTest|SendMavCommandWithSignallingTest|SendMavCommandWithHandlerTest|MavCommandQueueReentrancyTest|PX4ExternalFlightModesTest|StandardModesTest|PlanMasterControllerTest|PlanViewUITest|TopLevelViewsTest|ToolbarIndicatorUITest)$'
ctest --test-dir build/boat-p2 --output-on-failure --parallel 4 -L Unit
```

새 테스트 2개, 기존 회귀 10개, 전체 Unit 197개는 각각 통과했다.
최종 구현에서도 전체 Unit 197개와 관련 12개의 중복을 제거한 CTest 208개가 모두 통과했다.
최종 구현의 검증 결과는 로컬 `build/boat-p2/boat-p2-validation/`에 원본 로그와 함께 보관한다.
영상 비활성 테스트의 내부 skip은 영상 기능 검증을 뜻하지 않는다.

정적 검사에서는 C++ 소스 3개의 Clang-Tidy가 오류 없이 종료했다.
AppleClang PCH와 LLVM의 형식 차이를 피하도록 분석용 compile database에서 PCH만 제외하고 Xcode SDK를 명시했다.
공식 Qt 6.11.1과 이 빌드의 import 경로를 사용한 변경 QML 검사도 종료 코드 0이며 기존 경고는 남아 있다.
전체 pre-commit 통과로 기록하지 않는다. 기존 파일의 포맷 위반·MockLink의 Q_ASSERT·Vehicle null 검사 경고,
기본 빌드 경로를 가정하는 분석 훅과 Qt import 경로 문제가 남아 있다.
CMake 검사 환경의 누락된 PyYAML은 검사 전용 가상환경에 설치했으며 CMake lint는 통과했다.
README·구현 계획·이 문서의 pre-commit 검사와 로컬 문서 링크 검사도 통과했다.

## 4. 실행과 수동 확인

P1의 Qt 환경을 사용하되 빌드 디렉터리는 분리한다. 초기 도구 설치는 [P1 가이드](P1_TESTING.md)를 참고한다.

```bash
cmake -S . -B build/boat-p2 -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH=/private/tmp/qgc-boat-qt/6.11.1/macos \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DQGC_BUILD_TESTING=ON \
    -DQGC_ENABLE_GST_VIDEOSTREAMING=OFF \
    -DQGC_MACOS_UNIVERSAL_BUILD=OFF
cmake --build build/boat-p2 --parallel 6
open -n build/boat-p2/Debug/QGroundControl.app --args --allow-multiple
```

Qt가 다른 위치에 설치되어 있으면 `CMAKE_PREFIX_PATH`를 실제 경로로 변경한다.
기존 QGC를 종료한 뒤 이 빌드를 실행한다. P1 실행 파일은 `build/boat-p1`에 유지한다.
오프라인 Boat 선택만으로는 Fly View의 연결 기체 운항 액션이 활성화되지 않는다.
자동 테스트는 실기 없이 실행할 수 있으며, 수동 운항 확인에는 타입 11을 보고하는 PX4/검증한 SITL 연결이 필요하다.

- Boat가 미연결/비 armed 상태일 때 Go To·Pause·Return 조건 확인.
- armed 상태이며 필요한 기능을 지원할 때 `flying=false`여도 액션이 표시되는지 확인.
- Boat에 Takeoff·Land·고도 변경이 표시되지 않고 Pause에 고도 슬라이더가 없는지 확인.
- 고도 미수신 상태의 Go To/Pause 요청·ACK·모드·실제 동작을 각각 기록.
- 확인 중 다른 기체로 전환하거나 통신을 끊었다 복구했을 때 이전 명령이 실행되지 않는지 확인.
- Rover·드론의 기존 액션·슬라이더·미션 흐름과 비교.

실제 PX4/SITL·실선, Android, 다중 링크 복구·실기 재부팅 후 늦은 ACK는 이번에 실행하지 않았다.
Go To의 도착, Pause의 위치 유지·추진 정지, RTL의 복귀·접안은 펌웨어와 실선에서 별도 인수해야 한다.
계획 화면의 고도 그래프·Alt Frame·Waypoints Altitude 및 Boat 아이콘은 P3/P4 범위다.
