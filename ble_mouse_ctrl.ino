#include <BLE.h>
#include <hid/BLEHIDDevice.h>

BLEServer server;
BLEHIDDevice *hid;
BLECharacteristic mouseInput;

uint8_t mouseReportDescriptor[] = {
    0x05, 0x01,
    0x09, 0x02,
    0xA1, 0x01,

    0x09, 0x01,
    0xA1, 0x00,
    0x85, 0x01,

    // Mouse buttons
    0x05, 0x09,
    0x19, 0x01,
    0x29, 0x03,
    0x15, 0x00,
    0x25, 0x01,
    0x95, 0x03,
    0x75, 0x01,
    0x81, 0x02,
    0x95, 0x01,
    0x75, 0x05,
    0x81, 0x03,

    // X, Y and Wheel
    0x05, 0x01,
    0x09, 0x30,
    0x09, 0x31,
    0x09, 0x38,
    0x15, 0x81,
    0x25, 0x7F,
    0x75, 0x08,
    0x95, 0x03,
    0x81, 0x06,

    0xC0,
    0xC0
};


void sendMouseReport(int8_t x, int8_t y, uint8_t buttons = 0)
{
    uint8_t report[4];

    report[0] = buttons;
    report[1] = (uint8_t)x;
    report[2] = (uint8_t)y;
    report[3] = 0;

    Serial.print("Subscribers: ");
    Serial.println(mouseInput.getSubscribedCount());

    mouseInput.setValue(report, sizeof(report));
    mouseInput.notify();
}


void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("Starting BLE HID Mouse...");

    if (!BLE.begin("ESP32 Air Mouse"))
    {
        Serial.println("BLE initialization failed!");
        return;
    }
    BLE.getSecurity().setIOCapability(BLEIOCapability::NoInputNoOutput);
    BLE.getSecurity().setAuthenticationMode(false, false, false);
    server = BLE.createServer();

    hid = new BLEHIDDevice(server);

    hid->manufacturer("Espressif");
    hid->pnp(0x02, 0x1234, 0x5678, 0x0100);
    hid->hidInfo(0x00, 0x01);

    hid->reportMap(
        mouseReportDescriptor,
        sizeof(mouseReportDescriptor)
    );

    mouseInput = hid->inputReport(1);

    // Subscription callback
    mouseInput.onSubscribe(
        [](BLECharacteristic &chr,
           const BLEConnInfo &conn,
           uint16_t subValue)
        {
            Serial.println();
            Serial.println("Subscription changed!");

            Serial.print("Subscription value: ");
            Serial.println(subValue);

            Serial.print("Subscribed clients: ");
            Serial.println(chr.getSubscribedCount());

            if (subValue & 0x01)
            {
                Serial.println("Client enabled notifications");
            }
            else
            {
                Serial.println("Client disabled notifications");
            }
        }
    );

    // Notification status callback
    mouseInput.onStatus(
        [](BLECharacteristic &chr,
           BLECharacteristic::NotifyStatus status,
           uint32_t code)
        {
            Serial.print("Notification status: ");
            Serial.print((int)status);

            Serial.print(" | Backend code: ");
            Serial.println(code);

            if (status == BLECharacteristic::SUCCESS_NOTIFY)
            {
                Serial.println("Notification sent successfully");
            }
            else
            {
                Serial.println("Notification failed");
            }
        }
    );

    server.start();

    BLEAdvertising advertising = BLE.getAdvertising();

    advertising.setAppearance(BLE_APPEARANCE_HID_MOUSE);
    advertising.start();

    Serial.println();
    Serial.println("BLE HID Mouse Ready!");
    Serial.println("Device name: ESP32 Air Mouse");
    Serial.println("Waiting for Bluetooth connection...");
}


void loop()
{
    if (mouseInput.getSubscribedCount() > 0)
    {
        Serial.println();
        Serial.println("Sending mouse movement");

        sendMouseReport(30, 0);
        delay(500);

        sendMouseReport(0, 30);
        delay(500);

        sendMouseReport(-30, 0);
        delay(500);

        sendMouseReport(0, -30);
        delay(500);
    }
    else
    {
        delay(500);
    }
}