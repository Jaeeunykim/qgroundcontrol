# Ubuntu 전곡항·선박 PX4 SITL 검증 가이드

- 작성일: 2026-11-24
- 대상: 기존 PX4 SITL + Gazebo(gz), 전곡항 월드·선박 모델, 같은 Ubuntu PC에서 개발 QGC 실행
- 소스: `https://github.com/Jaeeunykim/qgroundcontrol.git`, 통합 브랜치 `feature/boat`
- 상태: 재현 절차·기록 양식 준비. 이 문서 자체는 Ubuntu/SITL 실행 결과가 아니다.

## 1. 기존 환경과 시험 기준 기록

기존 PX4·Gazebo 실행 스크립트와 수정 파일을 보존한다. 월드·선박 모델 이름만으로 PX4의 Boat 타입을 가정하지 않는다.
Ubuntu 쪽 Codex는 다음 항목을 먼저 조사하고 [시험 기록 양식](SITL_TEST_REPORT_TEMPLATE.md)에 기록한다.

| 대상 | 기록 내용 |
| --- | --- |
| Ubuntu | 배포판·버전, `uname -m`, GPU/화면 세션, 사용 가능한 RAM·디스크 |
| PX4 | 저장소 경로, 브랜치·전체 SHA·dirty 상태, 기존 실행 명령, 기체 airframe/SYS_AUTOSTART |
| Gazebo | gz 버전, 월드·모델·플러그인 경로와 커밋/수정 상태, 초기 좌표·Home |
| 연결 | PX4 GCS MAVLink instance, 송신 목적지·포트, router/컨테이너 사용 여부, system/component ID |
| QGC | 저장소·전체 SHA·dirty 상태, Qt·컴파일러·빌드 옵션, 실행 파일·설정 프로필·로그 경로 |
| 판정 | 항로 도착·정지/위치 유지·복귀·속도 변경의 허용 오차·지연과 PX4에서의 의미 |

PX4 콘솔의 `ver all`, `param show SYS_AUTOSTART`, `mavlink status` 등 읽기 명령과 실제 실행 스크립트를 대조한다.
GCS의 기본 수신 포트는 UDP 14550이지만 기존 startup/router 설정이 우선이다.
Docker/VM을 사용한다면 그 안의 loopback과 Ubuntu 호스트 loopback을 구분한다.
연결 기준은 [PX4 시뮬레이션 문서](https://docs.px4.io/main/en/simulation/index)를 참고한다.

## 2. 개발 소스와 Ubuntu 빌드

기존 QGC 작업 디렉터리가 있다면 `git status --short --branch`, `git remote -v`부터 확인한다.
기존 수정 파일을 덮어쓰지 않으며, 필요하면 새 디렉터리를 사용한다. 새 clone 예시는 다음과 같다.

```bash
git clone --branch feature/boat --recurse-submodules https://github.com/Jaeeunykim/qgroundcontrol.git qgroundcontrol-boat
cd qgroundcontrol-boat
git rev-parse HEAD
git status --short --branch
```

현재 소스의 환경 기준은 [build-config.json](../../../.github/build-config.json),
Linux CI는 [linux.yml](../../../.github/workflows/linux.yml), 설치·빌드 방법은
[도구 가이드](../../../tools/README.md)를 따른다. 현재 Qt 설정은 6.11.1이며 Linux CI는 Ubuntu 24.04를 사용한다.
기존 PX4 빌드에 쓰는 Qt/컴파일러가 QGC의 요구사항도 충족한다고 가정하지 않는다.

- 시스템 의존성이 없으면 저장소의 `python3 tools/setup/install_dependencies --platform debian`을 사용한다.
- 개발 도구는 `python3 tools/setup/install_python.py dev`로 준비하고 해당 가상환경을 활성화한다.
- Qt가 없으면 `tools/setup/install_qt.py install --help`와 build-config의 버전·모듈을 확인해 설치한다.
  x86_64와 arm64의 Qt host/arch·설치 디렉터리는 다르다. 기존 Gazebo 패키지를 임의로 교체하지 않는다.
- `just`는 저장소 지침의 최소 버전을 충족해야 한다. 배포판 기본 패키지가 오래된 경우 개발 도구 설치 경로를 사용한다.

Qt SDK 경로를 `QT_DIR`로 지정한 뒤 별도 디렉터리에 빌드한다. 아래 Qt 경로는 x86_64 기본 설치 예시다.

```bash
source .venv/bin/activate
export QT_DIR="$HOME/Qt/6.11.1/gcc_64"
just info
just --set build_dir build/boat-ubuntu configure
JOBS=4 just --set build_dir build/boat-ubuntu build
```

영상이 시험 범위에 없고 GStreamer를 비활성화해야 한다면 위 configure 대신 다음 명령을 사용할 수 있다.
실제 명령과 옵션을 보고서에 남기며 영상 검증 완료로 기록하지 않는다.

```bash
python3 tools/configure.py -B build/boat-ubuntu -t Debug --testing --qt-root "$QT_DIR" -- -DQGC_ENABLE_GST_VIDEOSTREAMING=OFF
```

Linux 실행 파일은 기본 Debug 구성에서 `build/boat-ubuntu/Debug/QGroundControl`이다.

## 3. 자동 회귀와 별도 수동 설정 프로필

```bash
ctest --test-dir build/boat-ubuntu --output-on-failure --parallel 2 \
    -R '^(Boat.*|VehicleLinkManagerTest|QGCMAVLinkTest|AppSettingsTest|PlanMasterControllerTest|PlanViewUITest)$'
ctest --test-dir build/boat-ubuntu --output-on-failure --parallel 4 -L Unit
just lint
```

GUI가 없는 세션은 Linux CI와 같이 `xvfb-run`·Mesa 등 화면/렌더링 의존성을 준비한다.
실제 Gazebo와 수동 QGC는 Ubuntu 데스크톱 세션에서 실행한다. 테스트 실패·skip·도구 설치 실패를 각각 기록한다.
Mac의 자동 테스트 통과 기록을 Ubuntu 결과로 옮겨 쓰지 않는다.

기존 설치 QGC를 종료하고, 개발 QGC만 해당 GCS 포트를 수신하게 한다.
아래는 수동 시험용 설정·캐시·데이터를 별도로 두는 실행 예시다.
Qt의 [QSettings 경로 규칙](https://doc.qt.io/qt-6/qsettings.html#platform-specific-notes)에 따라
XDG 경로를 QGC 실행 프로세스에만 지정한다.

```bash
boat_profile_dir="$PWD/build/boat-ubuntu/manual-profile"
mkdir -p "$boat_profile_dir/config" "$boat_profile_dir/data" "$boat_profile_dir/cache" "$boat_profile_dir/logs"
boat_run_id="$(date +%Y%m%d-%H%M%S)"
XDG_CONFIG_HOME="$boat_profile_dir/config" \
XDG_DATA_HOME="$boat_profile_dir/data" \
XDG_CACHE_HOME="$boat_profile_dir/cache" \
    ./build/boat-ubuntu/Debug/QGroundControl --log-output \
    > "$boat_profile_dir/logs/qgc-$boat_run_id.log" 2>&1
```

앱 재실행 때도 같은 프로필을 사용한다. 계획 파일은 시험용 디렉터리에 명시적으로 저장한다.
첫 실행에서는 새 Boat 기본 계기판을 확인하고, 기존 설정 이관 시험은 별도 프로필에서 진행한다.
기존 사용자 배치에는 고도 항목이 남을 수 있으며 Boat 기본값 초기화와 구분해 판정한다.

## 4. Boat 연결의 진입 조건

기존 PX4/Gazebo 실행 명령을 사용하고 QGC의 MAVLink Inspector에서 PX4 autopilot의
`HEARTBEAT.type = MAV_TYPE_SURFACE_BOAT(11)`을 확인한다. system/component와 autopilot도 기록한다.
타입 10은 Rover이며 오프라인 선택을 Boat로 바꿔도 연결 기체의 타입은 바뀌지 않는다.
타입이 다르면 airframe/firmware 발행 경로를 조사하고 번호만 바꾼 것을 실제 선박 제어 합격으로 기록하지 않는다.
타입 근거는 [MAVLink MAV_TYPE](https://mavlink.io/en/messages/minimal.html#MAV_TYPE)을 참고한다.

## 5. 수동 시험 순서와 완료 조건

| ID | 시험 | 확인할 결과·증거 |
| --- | --- | --- |
| S01 | SITL 미연결, PX4/Boat 선택→WP·시작/구간 속도 설정→저장→재열기→앱 정상 종료·재실행 | Boat 타입·WP·속도 유지, Alt Frame/고도 입력·그래프 숨김, 파일 사본 |
| S02 | 타입 11 기체 연결 | Boat 아이콘, 고도 없는 계획 UI, 대지속도·Heading·COG·거리 계기, type/system/component 캡처 |
| S03 | WP 2~3개·서로 다른 속도 업로드→기체에서 다시 다운로드→별도 파일로 저장 | 항목·좌표·속도·frame·고도 필드 왕복 대조. 화면에서 고도가 숨겨져도 wire 값을 0으로 일괄 변경하지 않음 |
| S04 | 미션 시작·진행 | 실제 PX4 모드·armed·현재 WP·위치·속도와 선박 경로. ACK와 이동 시작을 구분 |
| S05 | 운항 중 Pause→계속/재개 | 고도 슬라이더 없음, PX4 모드·감속/위치 유지·재개 항목을 실제 확인. Pause를 무조건 추진 정지로 해석하지 않음 |
| S06 | GoTo | 목표 좌표·PX4 모드·실제 접근. 사용한 PX4 버전이 DO_REPOSITION의 NaN 고도를 처리하는지 별도 확인 |
| S07 | RTL | 실제 Home·모드·복귀 경로·도착 후 상태를 사전 기준으로 판정 |
| S08 | 거부/미지원/시간 초과 | 성공으로 표시하지 않는지, 실제 모드·경로와 명령 결과를 구분하는지. 원래 지원하지 않는 명령을 새로 지원한 것으로 간주하지 않음 |
| S09 | 전체 MAVLink 중단→단절→heartbeat만 재개→각 stream 재개 | 기본 계기 NaN, heartbeat만으로 이전 값 미복구, 같은 위치 재수신도 Home 거리 복구. 늦은 응답·기체 전환에 이전 명령 자동 재실행 없음 |
| S10 | Heading≠COG·유효한 0·GPS fix 없음·COG sentinel/범위 밖 | Heading/COG 분리, GPS/GPS2 독립성, 무효 값 제거와 정상 샘플 복구. 시뮬레이터가 사례를 만들지 못하면 미실행 표시 |
| S11 | Boat 가상 조이스틱 | 좌우 배치·자동 복귀별 입력, 대상 변경/단절/숨김 후 새 터치 전 미송신, 늦은 release·resize로 이전 입력 재등장 없음 |
| S12 | Rover·드론 회귀 | 기존 고도 UI·액션·계기·저장 설정과 입력 매핑 비교. 해당 SITL 모델이 없으면 자동 시험과 수동 미실행을 구분 |

전체 단절은 heartbeat만 끄는 시험과 다르다. 다른 기체 MAVLink 패킷이 계속 오면 기존 LinkManager는
연결이 살아 있다고 판단할 수 있다. Gazebo 물리 시계 일시정지도 MAVLink 단절과 같지 않다.
기존 router/송신 instance 등 확인한 경로에서 중단·복구하며 복원 명령을 미리 기록한다.
heartbeat-only 또는 센서 무효 입력을 만들 수 없다면 해당 세부 항목을 미실행으로 남긴다.

일부 stream만 멈추거나 PX4가 오래된 센서 값을 반복 송신하는 경우의 원천별 만료는 후속 범위다.
임의 timeout을 인수 기준으로 만들지 않는다. 속도 0의 정지 의미·추진 중립·역추진과 수동 입력 상실 동작도
실제 펌웨어·모델의 제어 구현으로 확인한다. 가상 조이스틱 취소가 선박 정지를 보장한다고 기록하지 않는다.

## 6. 증거와 판정

- QGC 수신 telemetry log, 콘솔 로그, PX4 ULog/모드·미션 로그, 화면과 원본 `.plan`을 보관한다.
- 송신 명령의 param/frame·ACK가 필요하면 양방향 packet/router 로그도 확보한다.
  QGC 수신 로그만으로 모든 송신 필드가 입증된다고 가정하지 않는다.
- 각 항목은 통과/실패/미실행/해당 없음과 근거를 기록한다. 지원하지 않는 장비는 이유와 함께 분리한다.
- 허용 오차·지연 또는 펌웨어 의미가 합의되지 않았다면 관측 결과와 미확정을 남긴다.
- [시험 기록 양식](SITL_TEST_REPORT_TEMPLATE.md)을 복사해 채우고 이슈별 재현 단계·관련 로그 시각을 첨부한다.
  SITL 통과는 실제 선박·Android·upstream 승인 완료를 의미하지 않는다.

## 7. Ubuntu Codex에 전달할 시작 문구

```text
이 PC의 기존 PX4 SITL + Gazebo 전곡항·선박 환경을 보존하면서
feature/boat 통합 브랜치를 빌드하고 시험해주세요.
먼저 AGENTS.md와 docs/development/boat/UBUNTU_SITL_TESTING.md를 읽고,
기존 PX4/Gazebo 실행 명령·커밋·HEARTBEAT.type·MAVLink 포트를 조사해주세요.
QGC는 기존 설정과 분리한 프로필로 실행해주세요.
SITL_TEST_REPORT_TEMPLATE.md를 복사해 실제 실행 결과와 로그를 기록해주세요.
자동 회귀와 S01~S12를 진행하되 명령 ACK·PX4 모드·실제 움직임을 구분해주세요.
사례를 만들 수 없거나 정책이 미합의인 세부 항목은 미실행/미확정으로 남기고,
마지막에 정확한 재실행 명령과 발견한 문제의 재현 단계를 정리해주세요.
```
