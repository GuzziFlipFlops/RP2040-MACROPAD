# Hackster Story Draft

## A $25 ESP32 Robot Arm Rover for Kids Learning Robotics

I wanted to build a robot that kids could actually afford to make, experiment with, and understand. A lot of robot arm kits are cool, but they can get expensive fast. This project keeps the parts list small and cheap: an ESP32 DevKit, three MG996 servos, four SG90 servos, and a simple battery setup. The goal is a rover with a 5-DOF arm that can be controlled from any phone or laptop without installing an app.

When the ESP32 turns on, it creates its own Wi-Fi network called `ESP32-Robot-Arm`. There is no password, so a student can connect directly, open `http://192.168.4.1`, and start driving. The web page has a joystick for the rover drivetrain and another joystick for the arm. There are also sliders for the arm servos, including direct PWM control for testing and calibration.

The best part for learning is the "Program Your Robot" mode. It works like simple block coding. Kids can add a drivetrain block to move a motor for a selected amount of time, or add an arm block to move one servo to an angle. Then they press Go and the robot follows the sequence. It is not meant to be a complicated industrial robot controller. It is meant to make the idea of robotics feel reachable.

The robot uses seven servo outputs. Two MG996 servos drive the left and right drivetrain. One MG996 servo is used for the heavier arm joint, and four SG90 servos handle the other arm joints. The ESP32 controls all of them using PWM. The web interface is served directly by the ESP32, so there is no router, cloud account, or phone app required.

The estimated cost is around $25. The rough BOM is:

- 3x MG996 servos, about $9 total
- 4x SG90 servos, about $4 total
- ESP32 DevKit, about $3
- Old 2-cell 3.7 V battery pack in series, reused if available
- Or 1x 18650 cell plus TP4056 charger module, about $4 total
- 6 V boost converter for the servos, about $1
- Scrap cardboard, wood, plastic, screws, glue, or 3D printed parts for the body

The design is intentionally flexible. The frame can be made from cardboard, laser-cut wood, 3D printed parts, or whatever a classroom already has. That is part of the learning: the robot is cheap enough that students can change it, improve it, and make mistakes without being scared of ruining an expensive kit.

One important safety note: the servos should not be powered from the ESP32 3.3 V pin. They need a separate 5-6 V supply, and the servo supply ground must connect to the ESP32 ground. If lithium batteries are used, kids should have adult supervision for charging and wiring.

This project is open source under the MIT License. My hope is that teachers, parents, makerspaces, and students can use it as a starting point for a very low-cost robotics lesson: drive a rover, move an arm, learn PWM, learn Wi-Fi control, and then start changing the code.

It is small, cheap, and simple on purpose. That makes it a good first robot.
