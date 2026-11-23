# Boat P3 항로·속도 구현·테스트 기록

- 작성일: 2026-11-23
- 브랜치: `feat/boat-mission-planning`
- 기준: P2 `b789e7fb5`, Stable v5.1.5 기반
- 범위: BOAT-06~07의 기본 waypoint 계획·속도 편집, 계획 파일/모의 기체 왕복과 QML 회귀
- 상태: 로컬 구현·빌드·관련 자동 검증 완료. 실제 PX4/SITL·실선 인수와 공식 upstream 반영은 별도
- 환경: macOS 26.6.2 arm64, AppleClang 21, Qt 6.11.1, Debug, PX4/APM 포함, GStreamer 비활성
- MAVLink/XML: `c409cf690454db6d3e004bd14173bc6c7ff1e0ff`

## 1. 구현 범위

| 항목 | Boat 동작 | 기존 기체 |
| --- | --- | --- |
| 계획 기본값 | Alt Frame·Waypoints Altitude 숨김 | Rover·항공기 기존 표시 유지 |
| WP 편집기 | 고도 입력 숨김, Ground Speed로 속도 표시 | 기존 고도·Flight Speed 표시 유지 |
| 계획 상태 | 지형/고도 그래프 숨김, 거리·예상시간 통계 표시, 고도차·경사 제외 | 기존 그래프·통계 선택 유지 |
| 새 WP의 frame | 지형 계산 모드를 상속하지 않고 기존 relative frame 사용 | 기존 global altitude frame 규칙 유지 |
| 지형 조회 | Boat waypoint의 기존 지형 조회 제외에 더해 경로 segment 조회도 제외 | Rover·항공기 조회 조건 유지 |
| 속도 설정 | 시작 설정과 WP 뒤 속도 설정 모두 활성화. `DO_CHANGE_SPEED.param1 = 1`(ground speed) | Rover·VTOL 기존 가용 조건 유지, Multirotor ground speed·FixedWing airspeed 유지 |
| 속도 재읽기 | ground speed와 편집 가능한 비음수 유한 값을 정상 Section으로 복원 | 기존 항공기 스캔 규칙 유지 |
| 미지원 속도 표현 | airspeed·상승속도·음수 sentinel·NaN 명령을 조용히 소비/변조하지 않고 원래 항목으로 유지 | 기존 데이터 경로 유지 |
| 항공기 명령 | 일반 명령 선택에서 제외. 가져오기·저장은 허용하고 업로드 직전에 명확히 거부 | Rover·항공기 명령 목록과 업로드 조건 유지 |

항공기 명령 판별은 기존 이착륙 메타데이터와 local 이착륙, Loiter to Altitude,
Continue and Change Altitude, Condition Change Altitude, Change Altitude, Land Start,
Go Around, VTOL Transition 명령을 포함한다.
원본 항목을 삭제하거나 Boat 명령으로 바꾸지 않는다. Raw 편집기에서 입력해도 업로드 검사를 거친다.
그 외 명령에 대한 펌웨어 지원을 Boat 타입만으로 보증하지 않는다.

업로드 거부 시 미전송 변경 표시를 유지하고 전송 시퀀스를 해제한다.
사용자가 해당 항목을 수정한 뒤 명시적으로 재시도할 수 있다.
업로드 시작 성공과 기체의 미션 수락은 기존 MissionManager 결과 경로로 구분한다.

## 2. 고도·속도 데이터 정책

- UI에서 고도를 숨겨도 MAVLink frame·param7 또는 `.plan`의 고도 정보를 일괄 0/NaN으로 바꾸지 않는다.
- 새 WP는 기존 기본 고도 값을 유지한다. 선박 펌웨어가 그 필드를 어떻게 사용하는지는 실제 PX4 커밋에서 합의해야 한다.
- 가져온 relative/absolute/terrain frame과 유한한 param7을 저장·복원할 수 있다.
  Boat에서 지형 응답을 받아 고도를 재계산하지 않는다.
- 원본 wire 고도가 NaN인 계산 지형 계획을 임의의 값으로 복구하거나 업로드 가능하다고 간주하지 않는다.
  지형 서버를 기다리는 대신 불완전한 데이터로 분류한다.
- 속도 0은 기존 Fact의 유효 범위에 포함한다. 실제 정지/재개 의미는 펌웨어 검증 대상이다.
  0 속도 등으로 계산한 Boat 예상시간이 유한하지 않으면 `N/A`로 표시한다.
- 속도 단위 변환은 기존 Fact 경로를 사용한다. 장비별 최고 속도나 새로운 Boat 기본 속도를 가정하지 않는다.

## 3. 검증

새 자동 테스트:

- `BoatMissionPlanningTest`: PX4/APM 오프라인 Boat/Rover/Multirotor/FixedWing/VTOL 속도 가용성,
  0·2.5·30 m/s 명령 생성·Section 복원·파일 재저장, 미병합 명령 보존,
  고도/frame 보존, Boat↔Rover 명령 목록 캐시 분리, 오프라인 타입 변경 후 가용 상태 재계산.
- `BoatPlanEditorsTest`: 실제 QML Defaults/SimpleItem/MissionStats 컴포넌트에서 기체별 표시·속도 문구·미확정 시간 표시.
- `BoatNavigationTest` 확장: Boat/Rover/드론의 MockLink 미션 업로드·다운로드와 속도 복원,
  항공기 명령 업로드 거부·dirty 유지·수정 후 재시도.
- `PlanViewUITest` 확장: Boat 지형 그래프/버튼 숨김·통계 표시 및 기존 Rover 그래프 유지.

실행 명령:

```bash
cmake --build build/boat-p3 --parallel 6
ctest --test-dir build/boat-p3 --output-on-failure --parallel 2 \
    -R '^(BoatMissionPlanningTest|BoatPlanEditorsTest|BoatNavigationTest|PlanViewUITest)$'
ctest --test-dir build/boat-p3 --output-on-failure --parallel 4 -L Unit
```

검증 결과:

- Mac Debug 앱·테스트 빌드 통과.
- 새/확장 테스트와 기존 MissionManager·명령·UI·Unit을 포함한 관련 CTest 212개 통과.
- 최종 소스의 전체 Unit CTest 198개도 통과. 두 실행의 중복을 제외하면 213개 테스트다.
- 변경 생산 C++ 5개 Clang-Tidy 종료 코드 0, 기존 경고 유지. AppleClang PCH를 제외한 분석용 compile database와 Xcode SDK 사용.
- 공식 Qt의 변경 QML 4개 검사 종료 코드 0, 기존 경고 유지.
- Q_ASSERT·로깅·고정 qWait·QTest ignoreMessage·Vehicle null·번역 매크로 검사와 CMake lint 통과.
- 새 C++ 테스트와 확장한 BoatNavigationTest는 clang-format 검사 통과.
  기존 파일 전체 포맷은 기준 커밋에서도 위반하며 관련 비교를 로그에 보관한다. 전체 lint/CI 통과로 표기하지 않는다.
- CMake lint의 누락 PyYAML은 검사 전용 가상환경에 설치했다.

원본 빌드·테스트·정적 검사 로그와 테스트 목록은 로컬 `build/boat-p3/boat-p3-validation/`에 보관한다.

## 4. 수동 확인

```bash
open -n build/boat-p3/Debug/QGroundControl.app --args --allow-multiple
```

1. 미연결 상태에서 PX4·Boat를 선택하고 Plan으로 이동한다.
2. Defaults의 Alt Frame·Waypoints Altitude와 지형 그래프가 숨겨지고 Ground Speed가 표시되는지 확인한다.
3. WP를 두 개 추가하고 시작 속도와 WP별 속도를 다르게 지정한다.
4. 저장·재열기 후 Boat 타입·WP·속도와 거리·시간 통계가 유지되는지 확인한다.
5. 기존 aircraft 명령이 든 계획은 원본 항목을 유지하며, Boat에 업로드 시 안내하고 거부하는지 확인한다.
6. 계획을 비우고 Rover·드론을 선택해 기존 고도 UI·속도 조건·그래프를 비교한다.

자동 명령 왕복은 MockLink로 검증한다. 실제 PX4/SITL·실선에서 waypoint 고도 해석,
명령별 지원, 속도 0·속도 변경의 실제 운항 동작, 복귀·정지 결과를 별도로 인수해야 한다.
측량 등 복합 패턴의 Boat 전용 지원, 계기판·아이콘(P4), Android와 실선 시험은 이 단계의 완료 범위가 아니다.
