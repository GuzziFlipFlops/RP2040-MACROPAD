# Schematic

This folder contains the generated wiring schematic for the robot.

- `generate_schematic.py` - Python script that generates the diagram
- `robot_arm_rover_schematic.svg` - generated schematic image
- `robot_arm_rover_schematic.png` - generated schematic image for uploads

Run the generator from the repo root:

```powershell
python schematic/generate_schematic.py
```

The schematic shows:

- 3.7 V Li-ion battery to DC-DC converter input
- 6 V converter output to the servo power rail
- Common ground between battery, converter, ESP32, and all servos
- ESP32 GPIO signal wires to all seven servo motors

Note: 3.7 V to 6 V is a boost converter setup. Some ESP32 DevKit boards can accept 6 V on VIN, but check your board regulator. If needed, use a separate 5 V regulator for the ESP32.
