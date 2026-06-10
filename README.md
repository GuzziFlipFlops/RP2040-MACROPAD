# ESP32 $25 Robot Arm Rover

This is an ESP-IDF project for a very cheap Wi-Fi robot rover with a 5-DOF servo arm. It is meant for kids and beginners who are learning robotics, electronics, web controls, and simple programming ideas without needing expensive parts.

The ESP32 creates its own open Wi-Fi access point, serves a control page, and drives all seven motors as servos:

- 2 MG996 servos for the drivetrain
- 1 MG996 servo for the arm
- 4 SG90 servos for the arm

After flashing, connect to the robot Wi-Fi network and control it from a phone, tablet, or laptop browser.

```text
Wi-Fi name: ESP32-Robot-Arm
Password: none
Web page: http://192.168.4.1
```

Because the Wi-Fi network has no password, use it for local learning and testing, not as a secure network.

## What It Does

- Starts an ESP32 access point in AP mode
- Hosts a browser control panel at `192.168.4.1`
- Provides one joystick for driving the robot
- Provides one joystick for controlling the selected arm servo
- Lets you set each arm servo by angle or direct PWM pulse width
- Includes a "Program Your Robot" mode with block-style actions
- Lets kids build simple robot programs without installing an app

The programming mode has two block types:

- Drivetrain block: pick the drivetrain motor, direction, speed, and time
- Arm servo block: pick one servo output, pick an angle, and run it

Servo blocks run one at a time, so the arm moves in a simple, beginner-friendly sequence.

## Why This Exists

Robotics kits are often too expensive for classrooms, clubs, and families. This project is designed to be built from common low-cost parts, scrap materials, and a normal ESP32 DevKit. The goal is a robot that feels exciting but stays cheap enough for kids to experiment with, break, fix, and understand.

The target cost is about **$25**, depending on what parts you already have.

## Bill of Materials

| Part | Quantity | Estimated Cost |
| --- | ---: | ---: |
| MG996 servo | 3 | $9 total |
| SG90 servo | 4 | $4 total |
| ESP32 DevKit / ESP32-WROOM-32 board | 1 | $3 |
| Old 2-cell 3.7 V battery pack in series | 1 | free if reused |
| 18650 cell + TP4056 charger module alternative | 1 set | about $4 |
| 6 V boost converter for servos | 1 | about $1 |
| Jumper wires, cardboard/wood/3D printed frame, screws, tape, glue | as needed | varies |

If you reuse an old 2-cell battery pack, the electronics can be around $16 before frame materials. If buying the battery/charger and boost converter, the project is still meant to land around the **$25** range.

## Servo Outputs

Use a separate 5-6 V servo power supply and connect the servo power ground to ESP32 GND. Do not power all servos from the ESP32 3.3 V pin.

| Output | Servo | GPIO |
| --- | --- | --- |
| Left drivetrain | MG996 continuous-rotation servo | GPIO18 |
| Right drivetrain | MG996 continuous-rotation servo | GPIO19 |
| Arm base / heavy joint | MG996 servo | GPIO21 |
| Arm shoulder | SG90 servo | GPIO22 |
| Arm elbow | SG90 servo | GPIO23 |
| Arm wrist | SG90 servo | GPIO25 |
| Arm claw | SG90 servo | GPIO26 |

If the rover spins when it should drive forward, reverse one drivetrain servo mechanically or change the sign of the right motor output in `main/main.c`.

## Power Notes

Servos can pull much more current than the ESP32 can provide. Use:

- A 5-6 V rail for the MG996 and SG90 servos
- Common ground between the servo power supply and ESP32
- A boost converter if using a single 18650 cell
- Careful battery charging when using lithium cells

For kids, adult supervision is recommended when wiring batteries or charging lithium cells.

## Build and Flash

From an ESP-IDF shell:

```powershell
idf.py set-target esp32
idf.py build
idf.py -p COMx flash monitor
```

Replace `COMx` with your ESP32 serial port.

## Files

- `main/main.c` - ESP32 firmware, Wi-Fi AP, web server, web UI, and servo control
- `CMakeLists.txt` - ESP-IDF project file
- `sdkconfig` - ESP32 project configuration
- `HACKSTER_STORY.md` - a draft story/writeup for Hackster

## License

This project is released under the MIT License. See `LICENSE`.
