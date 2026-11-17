# Boat P1 테스트 가이드

P1은 Boat 식별과 오프라인 선택·계획 파일 저장/복원 단계다.
먼저 Pixhawk·badaMux·SITL 없이 오프라인 UI와 자동 테스트를 확인할 수 있다.
실제 연결 식별은 MAVLink의 `HEARTBEAT.type`에 따르며, 자율운항 명령·실선 운항 검증은 후속 단계다.

## 1. 현재 Mac에서 바로 실행

이 작업 공간에는 Qt 6.11.1과 Debug 실행 파일이 준비되어 있다.
저장소 루트에서 다음을 실행한다. 일반 GUI 실행에는 `--unittest`나 offscreen 환경 변수를 사용하지 않는다.

```bash
git switch feat/boat-identity-offline-plan
cmake --build build/boat-p1 --parallel 6
./build/boat-p1/Debug/QGroundControl.app/Contents/MacOS/QGroundControl --allow-multiple
```

실행 파일은 `build/boat-p1/Debug/QGroundControl.app` 아래에 있다.
같은 빌드 폴더의 `build/boat-p1/QGroundControl.app`는 배포 리소스용 경로이므로 실행 대상으로 사용하지 않는다.
같은 설정 공간을 사용하는 QGC는 종료하고 시험한다. 자동 테스트는 별도의 테스트 설정 공간을 사용한다.

현재 Qt 위치는 `/private/tmp/qgc-boat-qt/6.11.1/macos`다.
임시 폴더가 정리되면 아래 설치 절차로 Qt를 다시 준비하고 CMake를 새 경로로 재구성한다.

## 2. 새 macOS 개발 환경 준비

로컬 검증 환경은 Apple Silicon, macOS 26.6.2, AppleClang 21, Qt 6.11.1, Debug다.
전체 Xcode와 해당 SDK, Git, CMake 3.25 이상, Ninja, Python 3가 필요하다.
이 문서의 명령은 macOS용이며 Windows·Linux·Android에서의 실행 결과를 의미하지 않는다.

Homebrew가 설치된 Mac에서는 다음과 같이 도구와 Qt를 준비할 수 있다.
Xcode의 초기 설정 및 라이선스 동의가 완료되어 있어야 한다.

```bash
brew install cmake ninja python
xcodebuild -version
python3 -m venv .venv
.venv/bin/python -m pip install aqtinstall==3.3.0
.venv/bin/aqt install-qt mac desktop 6.11.1 clang_64 -O "$HOME/Qt" \
    -m qtgraphs qthttpserver qtlocation qtmultimedia qtpositioning qtquick3d \
    qtquicktimeline qtscxml qtsensors qtspeech qtshadertools qtserialport \
    qtconnectivity qttasktree qtwebsockets
```

Qt Online Installer를 사용해도 된다. 기본 Qt 모듈 외에 위 모듈을 포함한다.
이 브랜치의 요구 버전은 `.github/build-config.json`을 기준으로 확인한다.
Homebrew의 기본 Qt 일부만 설치된 환경은 Graphs 등 필수 모듈이 부족할 수 있다.

저장소 루트에서 구성하고 빌드한다. 첫 구성은 외부 의존성을 내려받으므로 인터넷 연결이 필요하다.
CMake가 프로젝트의 Python 생성 도구 환경을 준비한다.

```bash
cmake -S . -B build/boat-p1 -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.1/macos" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DQGC_BUILD_TESTING=ON \
    -DQGC_ENABLE_GST_VIDEOSTREAMING=OFF \
    -DQGC_MACOS_UNIVERSAL_BUILD=OFF
cmake --build build/boat-p1 --parallel 6
```

현재 작업 공간의 Qt를 재사용할 때에는 `CMAKE_PREFIX_PATH`에
`/private/tmp/qgc-boat-qt/6.11.1/macos`를 지정한다.
Qt 설치가 섞인 캐시를 다시 사용할 경우 위 구성 명령에 `--fresh`를 추가해 재구성한다.
P1 검증에는 GStreamer를 끈다. 이 구성은 영상 기능 검증을 포함하지 않는다.

## 3. 오프라인 화면 수동 확인

기존 작업이 있으면 먼저 별도 파일에 저장하고, 실기 연결과 자동 연결을 해제한 상태에서 진행한다.
아래는 수동 확인 절차이며, 실제 앱 종료·재실행 시험은 아직 수행 결과로 기록하지 않았다.

| 순서 | 조작 | 기대 결과 |
| --- | --- | --- |
| 1 | Plan View에서 빈 계획을 열고 우측 Plan Info → Vehicle Info 펼치기 | 미션 항목·연결 기체가 없을 때 펌웨어/기체 선택 표시 |
| 2 | 펌웨어 PX4 Pro, 기체 Boat 선택 | Rover와 구분되는 Boat 선택 유지 |
| 3 | 지도에서 Home 지정 후 Waypoint 도구로 WP 2개 추가 | 이륙 항목 없이 WP 추가 가능, 기존 복귀 도구 표시 |
| 4 | `boat-p1.plan`으로 저장하고 텍스트 편집기로 확인 | 최상위 `mission` 객체의 `vehicleType`이 11 |
| 5 | 계획 비우기 후 Rover를 선택하고 저장했던 Boat 파일 열기 | 파일의 Boat 타입과 WP 위치·명령·값 복원 |
| 6 | Rover 계획도 저장하고 다시 열기 | `mission.vehicleType`이 10, Boat로 변경되지 않음 |
| 7 | Multirotor·Fixed Wing·VTOL의 기존 계획 열기 | 해당 기체 타입과 기존 계획 편집 흐름 유지 |

기체 선택은 빈 계획이며 미연결 상태일 때 가능하다. WP가 생기거나 실기가 연결되면 선택 대신 기체 정보가 표시된다.
Boat 파일을 읽은 뒤 보이는 기체 설명은 기존 MAVLink 타입의 번역 문구일 수 있다.
지도 배경을 사용하려면 지도 공급자 연결도 필요하지만 타입 저장/복원 자체에는 실기가 필요하지 않다.

앱 재실행 보존은 파일 복원과 구분해 확인한다.
빈 계획·미연결 상태에서 Boat를 선택하고 앱을 완전히 종료한 뒤 다시 실행해 선택이 유지되는지 확인한다.
이후 저장한 Boat 파일도 다시 열어 WP와 `vehicleType: 11`을 확인한다.

## 4. 자동 회귀 테스트

GUI 앱을 종료한 뒤 저장소 루트에서 실행한다. CTest가 offscreen과 테스트별 설정 공간을 지정한다.
실제 Pixhawk가 필요하지 않으며 연결 시나리오는 MockLink의 MAVLink 메시지를 사용한다.
테스트 실행의 언어는 영어로 고정되고 일반 GUI 앱의 사용자 언어 설정은 유지된다.

```bash
ctest --test-dir build/boat-p1 --output-on-failure --parallel 2 \
    -R '^(QGCMAVLinkTest|AppSettingsTest|TestFixturesTest|PlanMasterControllerTest|MissionControllerTest|SpeedSectionTest|PlanViewUITest)$'
ctest --test-dir build/boat-p1 --output-on-failure --parallel 4 -L Unit
```

2026-11-17 로컬 결과는 관련 CTest 7개 및 Unit CTest 197개 통과다.
비활성 영상 테스트의 내부 skip은 영상 기능 통과를 뜻하지 않는다.
기존 포맷/null 검사 경고와 기본 compile database 경로 문제로 전체 pre-commit은 실패했으며,
변경 C++ 구간 포맷·문서 검사와 별도 Clang-Tidy 결과는 [구현 기록](IMPLEMENTATION_PLAN.md)에 구분했다.
로컬 원본 로그는 gitignore 대상인 `build/boat-p1/boat-p1-validation/`에 있으며 Git으로 배포하지 않는다.

## 5. Pixhawk/PX4 연결 식별 확인

보유한 PX4 장비 또는 검증한 SITL을 기존 Serial/UDP/TCP 링크로 연결한다.
MAVLink Inspector에서 autopilot의 `HEARTBEAT.type`을 확인한다.

- `MAV_TYPE_SURFACE_BOAT`(11)이면 연결 기체의 Boat 식별이 true여야 한다.
- `MAV_TYPE_GROUND_ROVER`(10)이면 기존 Rover로 식별된다. 오프라인 Boat 선택으로 펌웨어의 보고 타입을 바꾸지 않는다.
- 기체를 해제하고 빈 계획의 오프라인 타입이 마지막 연결 기체를 따르는지 확인한다.
- 미저장 Boat 계획을 편집한 상태에서 드론에 연결하면 기존 계획 사용 안내를 유지하고,
  그 계획을 보존하는 동안 Boat 타입·WP가 바뀌지 않아야 한다.

PX4 빌드/파라미터에서 어떤 타입을 보고할지는 해당 펌웨어를 별도로 확인해야 한다.
이번 P1은 PX4 펌웨어 설정이나 badaMux 메시지 규약을 변경하지 않는다.
모터 구동·미션 업로드/실행·Go To·중단·RTL은 이 식별 확인과 분리된 후속 운항 시험이다.
