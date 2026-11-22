# ESP32 Wireless Air Mouse — Setup, Debugging Notes & Troubleshooting

This README records the problems encountered while building the BLE air mouse with an ESP32-WROOM development board, an MPU6050 breakout, and Ubuntu. It includes the observed symptoms, how they were isolated, the fixes that worked, and a repeatable checklist for future debugging.

> **Important:** A few issues were conclusively identified (for example, the BLE API mismatch and MPU6050 power behaviour). The exact reason for the temporary missing-HID-service state was not conclusively captured in the chat logs. That section therefore records the evidence and the successful recovery path without claiming an unverified root cause.

---

## 1. Final working setup

### Hardware

- ESP32-WROOM development board (Arduino board selection: **ESP32 Dev Module**).
- MPU6050 / GY-521 breakout.
- Ubuntu computer with Bluetooth support.

### Wiring used

| MPU6050 breakout | ESP32-WROOM |
|---|---|
| VCC | 5V on the breakout configuration that worked |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

**Power note:** This particular breakout did not behave correctly when powered from 3.3V, but worked after its VCC was changed to 5V. The likely explanation is the breakout's onboard regulator/power arrangement, not that the MPU6050 chip itself is a 5V device. The sensor IC uses 3.3V-level I2C. Keep SDA and SCL at ESP32-safe 3.3V logic. Do not apply 5V to ESP32 GPIO pins. Other MPU6050 breakout designs may work from 3.3V; verify the board's schematic/specification rather than assuming all modules need 5V.

### Software

- Ubuntu Linux.
- Arduino IDE **2.3.10**.
- ESP32 by Espressif Systems Arduino core **4.0.0**.
- BLE source directory:
  `~/.arduino15/packages/esp32/hardware/esp32/4.0.0/libraries/BLE/src`
- BLE HID device name: `ESP32 Air Mouse`.
- ESP32 Bluetooth address observed during testing: `D0:EF:76:4B:6C:96`.

Do not confuse Arduino IDE version 2.3.10 with the ESP32 board package version 4.0.0. They are separate components.

---

## 2. How the project works

1. The MPU6050 measures acceleration and angular velocity.
2. ESP32 reads the sensor over I2C.
3. Gyroscope offsets are measured while the device is stationary.
4. A complementary filter combines gyroscope integration with accelerometer tilt.
5. Roll and pitch changes are converted into relative X/Y mouse reports.
6. ESP32 sends those reports through a BLE HID Report characteristic.
7. Ubuntu receives the device as a Bluetooth mouse.

The MPU6050 has no magnetometer. This design estimates roll and pitch; it does not provide an absolute compass heading or reliable absolute yaw.

---

## 3. Issues encountered, diagnosis and fixes

### Issue 1 — MPU6050 returned zero readings / registers did not appear to update

**Observed symptoms**

- `WHO_AM_I` returned `0x68`, so the device responded at the expected I2C address.
- I2C register writes returned status `0` (ACK/success).
- After attempting to write `0x07` to `SMPLRT_DIV` (`0x19`), reading it back still showed `0x00`.
- After attempting to write `0x00` or `0x01` to `PWR_MGMT_1` (`0x6B`), the register still read `0x40`.
- Sensor output was all zeros or otherwise unusable.

**How it was diagnosed**

- Checked the I2C identity register and confirmed the sensor was responding.
- Compared write return status with register readback. An I2C ACK alone did not prove that the intended register value had actually taken effect.
- Changed the breakout's VCC from 3.3V to 5V and tested again.

**Fix that worked**

- Power the specific breakout from 5V, with common ground.
- Keep SDA/SCL logic at 3.3V.

**Why this may have happened**

The breakout likely has an onboard regulator or power circuitry that did not operate properly with 3.3V input. This is a board-specific conclusion based on the successful voltage change; the exact schematic-level cause was not established.

**If it happens again**

1. Check common ground.
2. Confirm SDA=GPIO21 and SCL=GPIO22.
3. Check the I2C address (`0x68`, or `0x69` if AD0 is high).
4. Read `WHO_AM_I` (`0x75`); expected value is `0x68` for MPU6050.
5. Write a register and read it back; do not rely only on `Wire.endTransmission()` returning zero.
6. Verify the breakout's allowed VCC range and regulator design.
7. Never allow 5V I2C pull-ups to reach ESP32 GPIOs.

Useful register settings used in the working sketch:

| Register | Address | Value | Meaning |
|---|---:|---:|---|
| `PWR_MGMT_1` | `0x6B` | `0x00` | Wake the MPU6050 |
| `GYRO_CONFIG` | `0x1B` | `0x00` | ±250 degrees/s |
| `ACCEL_CONFIG` | `0x1C` | `0x00` | ±2 g |
| `CONFIG` | `0x1A` | `0x03` | DLPF configuration |
| `SMPLRT_DIV` | `0x19` | `0x04` | Sample-rate divider |

With these ranges, the conversion factors used by the code are:
- Accelerometer: `16384` LSB/g.
- Gyroscope: `131` LSB/(degree/second).

---

### Issue 2 — Old BLE example did not compile with ESP32 core 4.0.0

**Observed symptom**

An example using the older header:

```cpp
#include <BLEDevice.h>
```

failed because the installed core 4.0.0 BLE library did not contain that legacy header/API layout.

**How it was diagnosed**

Inspected the installed BLE library:

```bash
cd ~/.arduino15/packages/esp32/hardware/esp32/4.0.0/libraries/BLE/src
find . -maxdepth 2 -type f | sort
```

The installed library exposed headers such as:
- `BLE.h`
- `BLEImpl.h`
- `hid/BLEHIDDevice.h`

The old `BLEDevice.h` header was not present.

**Fix**

Use the BLE API provided by the installed core 4.0.0, including:

```cpp
#include <BLE.h>
#include <hid/BLEHIDDevice.h>
```

The advertising test using `BLE.begin("ESP32 Air Mouse")` worked, and Ubuntu later discovered the device.

**If it happens again**

- Check the selected board and installed ESP32 core version in Arduino IDE.
- Inspect the actual installed library headers instead of copying a sketch written for a different ESP32 core.
- Do not mix old `BLEDevice` examples with the newer `BLE.h` API unless the installed package actually provides those headers.

---

### Issue 3 — ESP32 did not initially appear in Ubuntu Bluetooth scan

**Observed symptom**

Ubuntu's Bluetooth scan showed other nearby devices, but initially did not show `ESP32 Air Mouse`.

**How it was diagnosed**

- Confirmed that Ubuntu could scan and discover other Bluetooth devices.
- Checked the ESP32 Serial Monitor to determine whether BLE initialization and advertising had started.
- Tested advertising separately from HID and sensor code.
- After using the core 4.0.0 BLE API, Ubuntu discovered:

```text
ESP32 Air Mouse
D0:EF:76:4B:6C:96
```

**Fix**

First make a minimal BLE advertising sketch work. Only after the device is visible should HID services and MPU6050 code be added.

**Ubuntu checks**

```bash
bluetoothctl
```

Inside `bluetoothctl`:

```text
power on
agent on
default-agent
scan on
```

Wait for the ESP32 name/MAC. Stop scanning with:

```text
scan off
```

Outside `bluetoothctl`, if the adapter itself appears unpowered:

```bash
bluetoothctl show
sudo btmgmt power on
```

If the Bluetooth service appears stuck:

```bash
sudo systemctl restart bluetooth
```

Then retry scanning.

**If it happens again**

- Check Serial Monitor for BLE initialization errors.
- Confirm the ESP32 is advertising, not merely running the sketch.
- Confirm the board is not repeatedly resetting.
- Confirm Ubuntu's adapter is powered and scanning.
- Test with a minimal advertising sketch before debugging HID.

---

### Issue 4 — BLE HID mouse paired, but the cursor did not move

**Observed symptoms**

- Ubuntu could discover `ESP32 Air Mouse`.
- The device could appear connected or paired.
- Pairing alone did not create a working mouse input device.
- At one stage, GATT enumeration showed Generic Attribute, Device Information and Battery services, but no HID service.

**How it was diagnosed**

Used `bluetoothctl` to inspect the device and its GATT database:

```text
info D0:EF:76:4B:6C:96
menu gatt
list-attributes D0:EF:76:4B:6C:96
```

The expected HID service UUID is:

```text
00001812-0000-1000-8000-00805f9b34fb
```

The GATT list initially showed:
- `0x1801` Generic Attribute
- `0x180A` Device Information
- `0x180F` Battery Service

It did not show `0x1812` Human Interface Device at that point.

This proved that successful pairing was not sufficient: the HID service was not visible in that GATT database snapshot.

**Source-code investigation**

Inspected the installed core's HID implementation:

```bash
cd ~/.arduino15/packages/esp32/hardware/esp32/4.0.0/libraries/BLE/src
sed -n '35,100p' hid/BLEHIDDevice.cpp
```

The constructor creates the Device Information, Battery and HID services:

```cpp
_hidSvc = _server.createService(kHIDSvcUUID, 40);
```

It also adds the HID service UUID to advertising. We then inspected the Bluedroid and NimBLE server implementations to trace service registration and startup.

**Outcome**

The BLE HID mouse eventually worked and Ubuntu received mouse movement reports. The exact code change responsible for the intermediate missing-HID-service state was not captured in the conversation, so it would be misleading to claim a specific line was the confirmed root cause.

**If it happens again**

1. Confirm the ESP32 sketch reaches `server.start()` and advertising starts.
2. Check the GATT database again.
3. If `0x1812` is absent, troubleshoot HID service creation/registration/startup. Do not debug MPU6050 orientation yet.
4. If `0x1812` is present, inspect the Report characteristic and notification subscription.
5. Re-check the ESP32 Serial Monitor while Ubuntu connects.

Expected HID characteristics include:

| UUID | Characteristic |
|---|---|
| `0x2A4A` | HID Information |
| `0x2A4B` | Report Map |
| `0x2A4C` | HID Control Point |
| `0x2A4E` | Protocol Mode |
| `0x2A4D` | Report |

The input Report characteristic should support notifications and have a Client Characteristic Configuration descriptor (`0x2902`). The Report characteristic should also have a Report Reference descriptor (`0x2908`).

---

### Issue 5 — BLE pairing failed with `org.bluez.Error.AuthenticationCanceled`

**Observed symptom**

Ubuntu's `bluetoothctl` reported:

```text
org.bluez.Error.AuthenticationCanceled
```

**How it was diagnosed**

The device was visible, but the pairing/security exchange was being cancelled. We inspected the ESP32 core 4.0.0 security API and used its security configuration.

**Configuration used in the working HID sketch**

```cpp
BLE.getSecurity().setIOCapability(BLEIOCapability::NoInputNoOutput);
BLE.getSecurity().setAuthenticationMode(false, false, false);
```

After the security configuration was included, Ubuntu later reported:

```text
Paired: yes
Trusted: yes
ServicesResolved: yes
```

This establishes that pairing succeeded with that configuration. The logs do not isolate which individual security setting was decisive.

**If it happens again**

From `bluetoothctl`:

```text
power on
agent on
default-agent
devices
```

If a stale bond exists, remove that device entry and pair again:

```text
remove D0:EF:76:4B:6C:96
scan on
```

After the ESP32 appears:

```text
scan off
pair D0:EF:76:4B:6C:96
trust D0:EF:76:4B:6C:96
connect D0:EF:76:4B:6C:96
```

Use the actual MAC shown by your ESP32, as it may differ on another board.

Do not repeatedly remove and re-pair a device if pairing already succeeds. If `Paired: yes` and `ServicesResolved: yes`, move on to GATT/HID/input troubleshooting.

---

### Issue 6 — HID report descriptor and Report ID must match

**Problem addressed**

The HID report descriptor must describe the same report ID and data layout that the firmware sends through the Report characteristic.

The working descriptor includes:

```cpp
0x85, 0x01
```

This declares Report ID 1.

The firmware creates the corresponding input report using:

```cpp
mouseInput = hid->inputReport(1);
```

The mouse report is four bytes:

| Byte | Meaning |
|---:|---|
| 0 | Button bits |
| 1 | Relative X |
| 2 | Relative Y |
| 3 | Wheel |

The descriptor defines three button bits, relative X/Y/wheel values, and Report ID 1.

**Why this matters**

If the Report ID in the descriptor and the report characteristic do not agree, the host may parse the input incorrectly or fail to use it as expected.

**If the cursor stops working after changing the descriptor**

- Confirm `0x85, 0x01` is present.
- Confirm `hid->inputReport(1)` uses the same ID.
- Confirm the descriptor's fields match the bytes sent by `sendMouseReport()`.
- Confirm the report is sent on the matching input Report characteristic.

---

### Issue 7 — `NotifyStatus` compile error / incorrect callback type

**Observed API detail**

In this ESP32 core version, `NotifyStatus` is nested inside `BLECharacteristic`.

The correct callback type is:

```cpp
BLECharacteristic::NotifyStatus status
```

not:

```cpp
NotifyStatus status
```

**Correct callback pattern**

```cpp
mouseInput.onStatus(
    [](BLECharacteristic &chr,
       BLECharacteristic::NotifyStatus status,
       uint32_t code)
    {
        if (status != BLECharacteristic::SUCCESS_NOTIFY) {
            Serial.print("Notification error: ");
            Serial.println((int)status);
        }
    }
);
```

**If it happens again**

Check the installed `BLECharacteristic.h` rather than guessing the type name. Core/library versions can change API details.

---

### Issue 8 — BLE connection existed, but no mouse notifications were reaching Ubuntu

**How it was diagnosed**

The BLE characteristic API in the installed core exposes:
- `onSubscribe(...)`
- `getSubscribedCount()`
- `notify()`
- `onStatus(...)`

The firmware used the subscription callback to check whether the host enabled notifications.

**Useful diagnostic messages**

The ESP32 should report that the client enabled notifications. In the callback, subscription bit 0 indicates notifications.

The sending code should only send reports when a client is subscribed:

```cpp
if (mouseInput.getSubscribedCount() > 0) {
    sendMouseReport(10, 0);
}
```

**If it happens again**

- Check whether the subscription callback fires after connection.
- Check `mouseInput.getSubscribedCount()`.
- Check the notification status callback.
- Check that the host has enabled notifications on the input Report characteristic.
- Verify that the Report characteristic has Notify and a CCCD (`0x2902`).

A BLE connection does not automatically mean that the host has subscribed to the HID input report.

---

### Issue 9 — Mouse was not visible as a Linux input device / cursor did not respond

**How to diagnose**

Pairing and GATT discovery are lower layers. Linux must also expose the connected HID device through its input subsystem.

Useful commands:

```bash
cat /proc/bus/input/devices
```

```bash
ls -l /dev/input/
```

If `evtest` is installed:

```bash
sudo apt install evtest
sudo evtest
```

Select the ESP32 mouse device if it appears. Move the ESP32 and check for relative X/Y events.

**Interpretation**

- No HID service (`0x1812`): ESP32 GATT/HID registration problem.
- HID service exists but no Report notification subscription: report/CCCD/host subscription problem.
- Notifications are sent but no Linux input device appears: host HID/input integration problem.
- Linux input events appear but cursor does not move: inspect desktop input handling or whether the wrong event device was selected.

---

## 4. MPU6050 orientation-to-mouse implementation

Once BLE HID mouse movement worked with fixed test reports, the fixed square-pattern loop was replaced by MPU6050 orientation.

### Sensor setup

The firmware:
- Wakes the MPU6050 through `PWR_MGMT_1`.
- Selects ±2g accelerometer range.
- Selects ±250 degrees/s gyroscope range.
- Reads the 14-byte sensor block beginning at register `0x3B`.
- Calibrates gyroscope offsets while the board is stationary.

### Orientation estimate

The complementary filter combines:
- Gyroscope integration for short-term responsiveness.
- Accelerometer-derived tilt to correct long-term gyroscope drift.

The filter form is:

```text
angle = alpha * (previous_angle + gyro_rate * dt)
        + (1 - alpha) * accelerometer_angle
```

The tested starting value was:

```cpp
#define FILTER_ALPHA 0.98f
```

### Mouse mapping and tuning

The implementation uses roll/pitch error relative to the neutral orientation captured at startup. A dead zone suppresses small changes, and gain controls cursor speed.

Initial parameters:

```cpp
#define DEAD_ZONE 2.0f
#define MOUSE_GAIN 0.35f
#define MAX_MOUSE_SPEED 15
```

If cursor motion is too slow, increase `MOUSE_GAIN`. If it is too fast, decrease it. If it jitters while stationary, first check mechanical stability and gyro calibration, then increase the dead zone slightly.

**Important calibration practice:** Keep the device completely still during gyroscope calibration and hold it in the intended neutral operating position during orientation initialization.

**Axis direction:** If a tilt moves the cursor in the opposite direction from what you want, invert the corresponding mouse axis in the mapping. Do not change the sensor register configuration just to reverse cursor direction.

### Known limitations

- MPU6050 has no magnetometer, so yaw/heading is not independently observable.
- Accelerometer-based tilt is affected by linear acceleration, not only gravity.
- Gyroscope bias causes drift if calibration is poor or temperature changes significantly.
- The neutral orientation is the orientation captured at startup; changing how the device is held may require recalibration.

---

## 5. Repeatable debugging workflow

Use this order. It prevents wasting time debugging a higher layer when a lower layer is still broken.

### Stage A — Confirm the board and firmware

1. Select **ESP32 Dev Module** for the ESP32-WROOM board.
2. Confirm the installed ESP32 core version.
3. Upload a simple Serial Monitor test.
4. Confirm there are no boot loops or brownouts.

### Stage B — Confirm MPU6050 independently

1. Verify wiring and common ground.
2. Read `WHO_AM_I`; expect `0x68` for the tested module.
3. Read back configuration registers after writing them.
4. Confirm stationary accelerometer and gyroscope values are sensible.
5. Do not proceed to orientation mapping until raw readings work.

### Stage C — Confirm BLE advertising

1. Upload a minimal BLE advertising sketch using the installed core's API.
2. Open Serial Monitor.
3. Run `bluetoothctl` and `scan on`.
4. Confirm `ESP32 Air Mouse` appears.

### Stage D — Confirm HID service

Run:

```text
bluetoothctl
```

Then:

```text
menu gatt
list-attributes D0:EF:76:4B:6C:96
```

Confirm HID service `0x1812` and its characteristics are present.

### Stage E — Confirm pairing and notification subscription

Run:

```text
info D0:EF:76:4B:6C:96
```

Look for:
- `Paired: yes`
- `Trusted: yes`
- `Connected: yes`
- `ServicesResolved: yes`

Then check the ESP32 Serial Monitor for the notification subscription callback.

### Stage F — Confirm fixed HID reports

Before using the IMU, send simple test movements:

```cpp
sendMouseReport(30, 0);
delay(500);
sendMouseReport(0, 30);
delay(500);
sendMouseReport(-30, 0);
delay(500);
sendMouseReport(0, -30);
delay(500);
```

If this does not move the cursor, do not debug the MPU6050 yet.

### Stage G — Enable orientation control

1. Remove the fixed square test loop.
2. Read the MPU6050.
3. Calibrate gyro offsets while stationary.
4. Initialize neutral roll/pitch.
5. Enable complementary filter.
6. Tune dead zone and gain.
7. Reverse X/Y mapping if needed.

---

## 6. Quick troubleshooting table

| Symptom | First thing to check | Likely layer |
|---|---|---|
| MPU values all zero | VCC, GND, SDA/SCL, WHO_AM_I | Hardware/I2C |
| WHO_AM_I works but register readback does not change | Verify power and register read/write sequence | Hardware/I2C |
| `BLEDevice.h` not found | Installed ESP32 core and headers | Arduino BLE API |
| ESP32 not visible in scan | Serial advertising status and Ubuntu adapter power | BLE advertising |
| ESP32 visible but pairing cancelled | Security configuration and stale bond | BLE security |
| Paired but no `0x1812` service | HID service creation and server registration | GATT/HID |
| HID service present but no cursor movement | Report characteristic, Report ID, CCCD, subscription | HID reports |
| No subscription callback | Host notification subscription / Report characteristic | GATT notifications |
| Notifications succeed but no Linux input device | Linux HID/input enumeration | Ubuntu input stack |
| Fixed square test works but IMU control does not | I2C readings, calibration, filter and axis mapping | Sensor/orientation |
| Cursor drifts while stationary | Gyro calibration, neutral pose, dead zone | Orientation filter |
| Cursor moves in the wrong direction | X/Y sign mapping | Mouse mapping |

---

## 7. Useful commands reference

### Check ESP32 BLE device

```bash
bluetoothctl
```

```text
power on
agent on
default-agent
scan on
```

### Inspect paired/connected state

```text
info D0:EF:76:4B:6C:96
```

### Inspect GATT database

```text
menu gatt
list-attributes D0:EF:76:4B:6C:96
```

### Inspect Linux input devices

```bash
cat /proc/bus/input/devices
ls -l /dev/input/
sudo evtest
```

### Inspect installed ESP32 BLE headers

```bash
cd ~/.arduino15/packages/esp32/hardware/esp32/4.0.0/libraries/BLE/src
find . -maxdepth 2 -type f | sort
```

### Inspect HID service source

```bash
sed -n '35,100p' hid/BLEHIDDevice.cpp
```

---

## 8. Lessons learned

1. **Debug one layer at a time.** Verify I2C, then BLE advertising, then HID service, then fixed reports, then IMU orientation.
2. **An I2C ACK is not proof of correct sensor configuration.** Read registers back.
3. **Use the API installed with the selected ESP32 core.** Old BLE examples may not compile against a newer core.
4. **Pairing is not the same as HID operation.** Check the HID GATT service and input Report characteristic.
5. **The HID descriptor, Report ID, characteristic and payload must agree.**
6. **A BLE connection is not proof that notifications are enabled.** Check subscription state.
7. **Keep the fixed mouse test.** It is a useful way to isolate BLE/HID problems from IMU problems.
8. **Keep the MPU6050 stationary during gyro calibration.**
9. **Record the exact board package and wiring that worked.** This makes future reproduction much easier.

---

## 9. Final known-good baseline

- Board: ESP32-WROOM DevKit / ESP32 Dev Module.
- Arduino IDE: 2.3.10.
- ESP32 Arduino core: 4.0.0.
- BLE API: `BLE.h` and `hid/BLEHIDDevice.h`.
- HID appearance: mouse.
- HID service: `0x1812`.
- Input Report ID: `1`.
- Mouse report: buttons, relative X, relative Y, wheel.
- MPU6050 address: `0x68`.
- MPU6050 SDA/SCL: GPIO21/GPIO22.
- Breakout VCC: 5V in the tested working setup; I2C logic remains 3.3V.
- Orientation: complementary-filter roll/pitch.
- Mouse control: relative movement from neutral orientation.

**When troubleshooting, return to the last known-good stage instead of changing BLE, sensor, and filtering code simultaneously.**
