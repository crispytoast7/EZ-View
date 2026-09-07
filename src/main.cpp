#include "main.h"

#include "EZ-Template/api.hpp"
#include "ez_screen/ui.hpp"

// Example project showing both registration paths: one auton through the
// stock ez::as API, the rest through ez_screen with per-auton options.

ez::Drive chassis({-11, -12, -13}, {1, 2, 3}, 7, 3.25, 450);

void match_auton() {
  int rings = ez_screen::setting_get("Rings");
  int delay_s = ez_screen::setting_get("Delay");
  pros::delay(delay_s * 1000);
  ez_screen::log(0x4895EF, "auton", "match: scoring %d rings", rings);
  chassis.pid_drive_set(24, 110);
  chassis.pid_wait();
  chassis.pid_turn_set(90, 90);
  chassis.pid_wait();
  chassis.pid_drive_set(-24, 110);
  chassis.pid_wait();
}

void skills_auton() {
  chassis.pid_drive_set(48, 110);
  chassis.pid_wait();
  chassis.pid_drive_set(-48, 110);
  chassis.pid_wait();
}

void safe_auton() {
  chassis.pid_drive_set(12, 80);
  chassis.pid_wait();
}

void initialize() {
  chassis.opcontrol_curve_default_set(0.0, 0.0);
  chassis.initialize();

  // stock-style registration still works and shows up on screen:
  ez::as::auton_selector.autons_add({{"Safe\n\nshort drive, no risk", safe_auton}});

  // ez_screen registration adds per-auton options:
  int match = ez_screen::auton_add("Match", "the master routine, dialed in the pit", 15, 0xFF93D5, match_auton);
  ez_screen::auton_setting_step(match, "Rings", 4, 0, 6);
  ez_screen::auton_setting_enum(match, "Corner", {"Positive", "Negative"});
  ez_screen::auton_setting_toggle(match, "Ladder", true);
  ez_screen::auton_setting_step(match, "Delay", 0, 0, 10, "s");

  int skills = ez_screen::auton_add("Skills", "60 second programming run", 60, 0xF4B942, skills_auton);
  ez_screen::auton_setting_toggle(skills, "Wall reset", true);

  ez_screen::port_add("LEFT F", 11, true, 1);
  ez_screen::port_add("LEFT M", 12, true, 1);
  ez_screen::port_add("LEFT B", 13, true, 1);
  ez_screen::port_add("RIGHT F", 1, false, 1);
  ez_screen::port_add("RIGHT M", 2, false, 1);
  ez_screen::port_add("RIGHT B", 3, false, 1);
  ez_screen::port_add("INTAKE", 8, false, 2);

  ez_screen::health_add(8, "INTAKE");

  ez_screen::init(&chassis);  // pass a rotation (90/180/270) here for other mounts
  master.rumble(chassis.drive_imu_calibrated() ? "." : "---");
}

void disabled() {}
void competition_initialize() {}

void autonomous() {
  chassis.pid_targets_reset();
  chassis.drive_imu_reset();
  chassis.drive_sensor_reset();
  chassis.drive_brake_set(pros::E_MOTOR_BRAKE_HOLD);
  ez_screen::auton_run();
}

void opcontrol() {
  chassis.drive_brake_set(pros::E_MOTOR_BRAKE_COAST);
  while (true) {
    // classic EZ test combo: DOWN+B runs the selected auton when no
    // competition switch is plugged in
    if (!pros::competition::is_connected() && master.get_digital(pros::E_CONTROLLER_DIGITAL_DOWN) &&
        master.get_digital(pros::E_CONTROLLER_DIGITAL_B)) {
      autonomous();
      chassis.drive_brake_set(pros::E_MOTOR_BRAKE_COAST);
    }
    chassis.opcontrol_tank();
    pros::delay(ez::util::DELAY_TIME);
  }
}
