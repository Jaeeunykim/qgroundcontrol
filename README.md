<p align="center">
  <img src="https://raw.githubusercontent.com/Dronecode/UX-Design/35d8148a8a0559cd4bcf50bfa2c94614983cce91/QGC/Branding/Deliverables/QGC_RGB_Logo_Horizontal_Positive_PREFERRED/QGC_RGB_Logo_Horizontal_Positive_PREFERRED.svg" alt="QGroundControl Logo" width="500">
</p>

<p align="center">
  <a href="https://github.com/mavlink/QGroundControl/releases"><img src="https://img.shields.io/github/v/release/mavlink/QGroundControl" alt="Latest Release"></a>
  <a href="https://github.com/mavlink/qgroundcontrol/blob/master/.github/COPYING.md"><img src="https://img.shields.io/github/license/mavlink/QGroundControl" alt="License"></a>
  <a href="https://github.com/mavlink/QGroundControl/actions/workflows/linux.yml"><img src="https://github.com/mavlink/QGroundControl/actions/workflows/linux.yml/badge.svg" alt="Linux Build"></a>
  <a href="https://securityscorecards.dev/viewer/?uri=github.com/mavlink/qgroundcontrol"><img src="https://img.shields.io/ossf-scorecard/github.com/mavlink/qgroundcontrol?label=openssf%20scorecard" alt="OpenSSF Scorecard"></a>
  <a href="https://crowdin.com/project/qgroundcontrol"><img src="https://badges.crowdin.net/qgroundcontrol/localized.svg" alt="Crowdin"></a>
  <a href="https://discord.com/channels/1022170275984457759/1022185820683255908"><img src="https://img.shields.io/discord/1022170275984457759?logo=discord&logoColor=white&label=Discord" alt="Dronecode Discord"></a>
  <a href="https://doi.org/10.5281/zenodo.595404"><img src="https://zenodo.org/badge/DOI/10.5281/zenodo.595404.svg" alt="DOI"></a>
</p>

**QGroundControl** (QGC) is a Ground Control Station (GCS) for UAVs, providing full flight control
and mission planning for any *MAVLink-enabled* drone, including *PX4* and *ArduPilot* platforms.

## Features

- **Mission planning** — plan, edit, and fly autonomous waypoint, survey, and structure-scan missions.
- **Live Fly View** — real-time flight display with map, instruments, and full vehicle telemetry.
- **Vehicle setup** — guided wizards for sensor calibration, radio, flight modes, and power.
- **Parameter tuning** — inspect and edit every vehicle parameter through the Fact System.
- **Video streaming** — GStreamer-based UDP RTP / RTSP video with recording in the Flight Display.
- **Multi-vehicle** — connect to and monitor multiple vehicles simultaneously.
- **MAVLink tooling** — built-in MAVLink Inspector, console, and log download/analysis.
- **Cross-platform** — Windows, macOS, Linux, Android, and iOS from a single codebase.

## Boat support development

This fork is preparing general-purpose Boat support improvements for contribution to upstream QGC.
The working baseline is **v5.1.5** on **`feature/boat`**. P1 identity and offline plan persistence are
implemented on **`feat/boat-identity-offline-plan`**, with passing local automated tests.
P2 navigation action adaptations are implemented on **`feat/boat-navigation-actions`**, with local
automated regression coverage; validation against the target PX4 firmware and vessel remains pending.
P3 basic mission planning, ground-speed editing, and altitude UI adaptations are implemented on
**`feat/boat-mission-planning`**, with local automated coverage. P4 basic presentation and layout
preservation, distinct Heading/GPS COG, and initial/invalid telemetry handling are implemented on
**`feat/boat-ui`**; source freshness policies, remaining operational UI, and P5 remain pending.
These changes have not been accepted upstream.
The first integration target is **PX4/Pixhawk ↔ MAVLink ↔ QGC**.
Boat support must remain independent of the vessel's onboard equipment, including badaMux.

- [ ] Define the PX4-to-QGC telemetry contract and record the tested firmware and MAVLink versions.
- [x] Implement and automatically test Boat identity during connection, offline planning, plan save/load, and vehicle switching (P1).
- [x] Implement Boat navigation action adaptations and automated command/UI regression tests (P2).
- [ ] Accept P1/P2/P3 on the target PX4 firmware and vessel; complete outstanding manual checks and lint requirements.
- [x] Implement basic Boat mission editors, ground-speed round trips, altitude UI adaptations, and aircraft-command upload guards (P3).
- [x] Implement Boat map icons, default instruments, layout preservation, and armed-state wording (P4 basic presentation).
- [ ] Complete source freshness/expiry, remaining telemetry validity, physical manual-control, vessel-specific checklist, and platform acceptance work (P4).
- [ ] Verify missing/stale telemetry, disconnect/reconnect, and Rover/aircraft compatibility.
- [ ] Prepare focused upstream PRs with tests, UI evidence, and documented limitations.

See the [implementation plan](docs/development/boat/IMPLEMENTATION_PLAN.md) for QML scope, staged
tests, and completion criteria, and the [telemetry contract draft](docs/development/boat/TELEMETRY_CONTRACT.md)
for message semantics and decisions required with the PX4 team. Detailed working documents are in Korean.
The [P1 testing guide](docs/development/boat/P1_TESTING.md) covers macOS setup, app launch,
offline save/restore checks, and automated regression tests. Final acceptance also requires the
manual/device coverage recorded in the implementation plan. Checked items above indicate local
implementation and automated tests, not device acceptance or upstream approval.
See the [P2 testing record](docs/development/boat/P2_TESTING.md) for navigation actions, command semantics,
regression coverage, and the separate P2 build/run commands.
See the [P3 testing record](docs/development/boat/P3_TESTING.md) for basic mission planning, speed
round trips, altitude UI, and imported command handling.
See the [P4 testing record](docs/development/boat/P4_TESTING.md) for icons, instruments, settings migration,
status wording, shared checklist transitions, and remaining P4 work.

## badaMux product communication specification

The separate [badaMux product specification](docs/product/badamux/README.md) covers vessel equipment,
PX4 integration, manual control, autonomous operation, message schemas, and acceptance tests.
It is a product protocol proposal, not a dependency or an upstream feature of generic QGC Boat support.

## Download

Grab the latest stable build for your platform, or see all assets on the
[releases page](https://github.com/mavlink/QGroundControl/releases/latest):

<p align="center">
  <a href="https://github.com/mavlink/QGroundControl/releases/latest/download/QGroundControl-installer.exe"><img src="https://img.shields.io/badge/Windows-0078D6?logo=windows&logoColor=white" alt="Windows"></a>
  <a href="https://github.com/mavlink/QGroundControl/releases/latest/download/QGroundControl.dmg"><img src="https://img.shields.io/badge/macOS-000000?logo=apple&logoColor=white" alt="macOS"></a>
  <a href="https://github.com/mavlink/QGroundControl/releases/latest/download/QGroundControl-x86_64.AppImage"><img src="https://img.shields.io/badge/Linux-FCC624?logo=linux&logoColor=black" alt="Linux (AppImage)"></a>
  <a href="https://github.com/mavlink/QGroundControl/releases/latest/download/QGroundControl.apk"><img src="https://img.shields.io/badge/Android-3DDC84?logo=android&logoColor=white" alt="Android"></a>
</p>

## Links

- [Official Website](http://qgroundcontrol.com)
- [User Manual](https://docs.qgroundcontrol.com/en/)
- [Developer Guide](https://dev.qgroundcontrol.com/en/) / [Build Instructions](https://dev.qgroundcontrol.com/en/getting_started/)
- [Discussion & Support](https://docs.qgroundcontrol.com/en/Support/Support.html)
- [Dronecode Discord](https://discord.com/channels/1022170275984457759/1022185820683255908)
- [Security Policy](.github/SECURITY.md)
- [Code of Conduct](.github/CODE_OF_CONDUCT.md)
- [License](https://github.com/mavlink/qgroundcontrol/blob/master/.github/COPYING.md)

## Contributing

QGC is open source and welcomes contributions. See [AGENTS.md](AGENTS.md) for build/test/lint
commands and coding conventions, and [.github/CONTRIBUTING.md](.github/CONTRIBUTING.md) for
architecture patterns and the contribution workflow.

QGC's interface is translated by the community — help translate it into your language on
[Crowdin](https://crowdin.com/project/qgroundcontrol).

## Star history

[![Star History Chart](https://api.star-history.com/svg?repos=mavlink/qgroundcontrol&type=Date)](https://star-history.com/#mavlink/qgroundcontrol&Date)
