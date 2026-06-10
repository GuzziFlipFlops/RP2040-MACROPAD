# Prebuilt ESP32 Firmware

These files can be flashed directly to an ESP32-WROOM-32 / ESP32 DevKit.

```powershell
python -m esptool --chip esp32 -p COM4 -b 460800 --before default-reset --after hard-reset write-flash --flash-mode dio --flash-size 2MB --flash-freq 40m 0x1000 firmware/bootloader.bin 0x8000 firmware/partition-table.bin 0x10000 firmware/esp32_robot_arm.bin
```

Replace `COM4` with your ESP32 serial port.

After flashing, connect to Wi-Fi network `ESP32-Robot-Arm` and open:

```text
http://192.168.4.1
```
