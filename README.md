# EZ-View

a full LVGL ui for EZ. an auton selector with per-auton options, live device health, console, an SD-backed port table, and manual PID tuning. dark theme styled after the EZ docs with robodash-style navigation.

> ## ⚠️  important
>
> **this project is an early prototype and needs heavy review before anyone trusts it on a robot.**
>
> the ui design started as mockups generated with AI and some functionality was built by it too. ive reviewed some of it in a patched version of [vex-v5-qemu](https://github.com/vexide/vex-v5-qemu) emulator. treat every feature as unverified on hardware, read the code before depending on it, and expect bugs :D

## what's in it

| feature | status |
|---|---|
| auton selector — list + description, per-auton options (steppers / enum cyclers / toggles) readable from your auton code | emulator-verified |
| backwards compatible with stock `ez::as::auton_selector.autons_add()` registration; selection syncs both ways | emulator-verified |
| comp-switch + classic DOWN+B test-run flow | needs hardware |
| SD persistence (`/usd/ez_screen.txt`): selection, options, pid constants, port table; graceful without a card | needs hardware |
| device health — live motor temps with color tiers, imu / rotation / distance pings, missing-device warnings | needs hardware (emulator has no devices) |
| console — colored, timestamped `ez_screen::log()` ring with scrollback | emulator-verified |
| ports page — edit port / reversed / cartridge, persisted, applies at next boot | needs hardware |
| tune page — manual pid constants applied live to the chassis (NOT the auto tuner) | needs hardware |
| screen rotation — 0 / 180 landscape, 90 / 270 rebuild as a portrait ui with a bottom tab bar | emulator-verified |
| flappy bird (landscape only) | verified extensively, priorities intact |

<img width="474" height="272" alt="image" src="https://github.com/user-attachments/assets/7638f7cf-8641-4dad-b183-cebe6c353136" />
<img width="430" height="226" alt="image" src="https://github.com/user-attachments/assets/6892fde1-e08b-4db2-8165-0a5ebc1a6ebf" />
<img width="438" height="228" alt="image" src="https://github.com/user-attachments/assets/48930455-309a-45dc-9c17-427d6ccfee12" />
<img width="458" height="256" alt="image" src="https://github.com/user-attachments/assets/e1b6fb4b-84bf-4103-aa8f-6ddebe47ae89" />


## usage

```cpp
#include "ez_screen/ui.hpp"

ez::Drive chassis({-11, -12, -13}, {1, 2, 3}, 7, 3.25, 450);

void initialize() {
  chassis.initialize();

  // stock registration still works and shows up on screen:
  ez::as::auton_selector.autons_add({{"Safe\n\nshort drive, no risk", safe_auton}});

  // ez_screen registration adds per-auton options:
  int match = ez_screen::auton_add("Match", "the main route", 15, 0xFF93D5, match_auton);
  ez_screen::auton_setting_step(match, "Rings", 4, 0, 6);
  ez_screen::auton_setting_enum(match, "Corner", {"Positive", "Negative"});
  ez_screen::auton_setting_toggle(match, "Ladder", true);

  ez_screen::health_add(8, "INTAKE");                      // drive motors + imu are automatic
  ez_screen::init(&chassis);                               // replaces ez::as::initialize()
  // ez_screen::init(&chassis, 90);                        // rotated mounts: 90 / 180 / 270
}

void autonomous() { ez_screen::auton_run(); }
```

inside an auton, read the options the drive team picked on the brain:

```cpp
int rings = ez_screen::setting_get("Rings");
bool ladder = ez_screen::setting_get("Ladder");
```

`src/main.cpp` is a complete example project.

## flappy bird will not run in portrait :(
