#include "PolycomCX300.h"
#include "HidDevice.h"
#include "Log.h"
#include "CustomConf.h"
#include "Mutex.h"
#include "ScopedLock.h"
#include "..\tSIP\tSIP\phone\Phone.h"

void Key(int keyCode, int state);
int RunScriptAsync(const char* script);

/*
Note:
- if two keys are pressed, only the first one is registered
- volume control buttons works "inside device"
- handset (+ its cable) is compatible with typical analog phone handset

Reported by HidP_GetCaps:
Usage Page: b
Input Report Byte Length: 8
Output Report Byte Length: 2            // <<< WTF? OutputReportByteLength = 2 while EP4 wMaxPacketSize = 64
Feature Report Byte Length: 64
Number of Link Collection Nodes: 8
Number of Input Button Caps: 10
Number of InputValue Caps: 1
Number of InputData Indices: 23
Number of Output Button Caps: 1
Number of Output Value Caps: 0
Number of Output Data Indices: 1
Number of Feature Button Caps: 0
Number of Feature Value Caps: 4
Number of Feature Data Indices: 4
*/

using namespace nsHidDevice;

namespace
{

const int VendorID = 0x095d;
const int ProductID = 0x9201;

/* https://github.com/probonopd/OpenPhone */
const uint8_t STATUS_AVAILABLE[] = {0x16, 0x01};
const uint8_t STATUS_BUSY[] = {0x16, 0x03};
const uint8_t STATUS_BE_RIGHT_BACK[] = {0x16, 0x05};
const uint8_t STATUS_AWAY[] = {0x16, 0x05};
const uint8_t STATUS_DO_NOT_DISTURB[] = {0x16, 0x06};
const uint8_t STATUS_OFF_WORK[] = {0x16, 0x07};

const uint8_t STATUS_LED_GREEN[] = {0x16, 0x01};
const uint8_t STATUS_LED_RED[] = {0x16, 0x03};
const uint8_t STATUS_LED_ORANGE_RED[] = {0x16, 0x04};
const uint8_t STATUS_LED_ORANGE[] = {0x16, 0x05};
const uint8_t STATUS_LED_OFF[] = {0x16, 0x07};
const uint8_t STATUS_LED_GREEN_ORANGE[] = {0x16, 0x08};

const uint8_t SPEAKER_LED_OFF[] = {0x02, 0x00};
const uint8_t SPEAKER_LED_ON[] = {0x02, 0x01};

const uint8_t DISPLAY_CLEAR[] = {0x13, 0x00};

const uint8_t TEXT_MODE_FOUR_CORNERS[] = {0x13, 0x0D};
const uint8_t TEXT_TOP_LEFT[] = {0x14, 0x01, 0x80};
const uint8_t TEXT_BOTTOM_LEFT[] = {0x14, 0x02, 0x80};
const uint8_t TEXT_TOP_RIGHT[] = {0x14, 0x03, 0x80};
const uint8_t TEXT_BOTTOM_RIGHT[] = {0x14, 0x04, 0x80};

const uint8_t TEXT_MODE_TWO_LINES[] = {0x13, 0x15};
const uint8_t TEXT_TOP_LINE[] = {0x14, 0x05, 0x80};
const uint8_t TEXT_BOTTOM_LINE[] = {0x14, 0x0A, 0x80};
const uint8_t TEXT_END[] = {0x80, 0x00};


enum { REPORT_IN_SIZE = 8 };

Mutex mutexState;
int regState = 0;
int callState = 0;
int ringState = 0;
std::string callDisplay;
bool displayUpdateFlag = false;
bool ringUpdateFlag = false;

HidDevice hidDevice;

const E_KEY KEY_NONE = static_cast<E_KEY>(-1);
enum E_KEY lastKey = KEY_NONE;
bool lastOffHook = false;

#	define DET_LOG if (customConf.detailedLogging) LOG

std::string GetCallDisplay(void) {
    ScopedLock<Mutex> lock(mutexState);
    return callDisplay;
}

void HandleReportIn(const uint8_t *report) {
    enum E_KEY key = KEY_NONE;

    switch (report[1])
    {
    case 0x01:
        key = KEY_0;
        break;
    case 0x02:
        key = KEY_1;
        break;
    case 0x03:
        key = KEY_2;
        break;
    case 0x04:
        key = KEY_3;
        break;
    case 0x05:
        key = KEY_4;
        break;
    case 0x06:
        key = KEY_5;
        break;
    case 0x07:
        key = KEY_6;
        break;
    case 0x08:
        key = KEY_7;
        break;
    case 0x09:
        key = KEY_8;
        break;
    case 0x0A:
        key = KEY_9;
        break;
    case 0x0B:
        if (customConf.dialKey == "*") {
            key = KEY_OK;
        } else {
            key = KEY_STAR;
        }
        break;
    case 0x0C:
        if (customConf.dialKey == "#") {
            key = KEY_OK;
        } else {
            key = KEY_HASH;
        }
        break;
    case 0x00:
        // key up or different key
        break;

    default:
        LOG("Unhandled key code in HID report = 0x%02X", report[1]);
        break;
    }

    const uint8_t REPORT0_OFF_HOOK = 0x01;
    uint8_t report0WithoutHook = report[0] & ~REPORT0_OFF_HOOK;
    switch (report0WithoutHook)
    {
    case 0x20:
        if (ringState) {
            key = KEY_CALL_HANGUP;
        } else {
            key = KEY_C;
        }
        break;
    case 0x02:  // HOLD key
        RunScriptAsync("ToggleHold()");
        break;
    default:
        break;
    }

    if (lastKey == KEY_NONE && key != KEY_NONE) {
        DET_LOG("Key code = %d, active", key);
        Key(key, 1);
    } else if (lastKey != KEY_NONE && key == KEY_NONE) {
        DET_LOG("Key code = %d, inactive", lastKey);
        Key(lastKey, 0);
    }

    if (key == lastKey && (report[0] & 0x08)) {
        // long key press
        DET_LOG("Key code = %d, long press", key);
    }

    lastKey = key;

    bool offHook = report[0] & REPORT0_OFF_HOOK;
    if (offHook != lastOffHook) {
        DET_LOG("OFF HOOK = %d", static_cast<int>(offHook));
        Key(KEY_HOOK, offHook ? 0 : 1); // tSIP: 1 = handset down
    }
    lastOffHook = offHook;
}

int ClearDisplay(HidDevice &dev) {
    return dev.WriteReportOut(DISPLAY_CLEAR, sizeof(DISPLAY_CLEAR));
}

int SetDisplayTwoLines(HidDevice &dev, const std::string &line1, const std::string &line2="") {
    int status = 0;

    status = dev.WriteReportOut(TEXT_MODE_TWO_LINES, sizeof(TEXT_MODE_TWO_LINES));
    if (status != 0)
        return status;

    for (unsigned int i=0; i<2; i++) {
        std::string text;
        if (i == 0) {
            text = line1;
            status = dev.WriteReportOut(TEXT_TOP_LINE, sizeof(TEXT_TOP_LINE));
            if (status != 0) {
                // when trying to write 3 bytes: GetLastError = 1784 (The supplied user buffer is not valid for the requested operation.)
                LOG("Error writing TEXT_TOP_LINE");
                return status;
            }
        } else {
            text = line2;
            status = dev.WriteReportOut(TEXT_BOTTOM_LINE, sizeof(TEXT_BOTTOM_LINE));
            if (status != 0) {
                LOG("Error writing TEXT_BOTTOM_LINE");
                return status;
            }
        }
#if 0
            status = dev.WriteReportOut(TEXT_END, sizeof(TEXT_END));
            if (status != 0) {
                LOG("Error writing TEXT_END");
                return status;
            }
#endif
        enum { CHUNK_LENGTH = 8 };
        for (unsigned int textPos = 0; textPos < text.length(); textPos += CHUNK_LENGTH) {
            uint8_t buffer[1 + 1 + (2*CHUNK_LENGTH)];
            uint8_t chunk[CHUNK_LENGTH];
            unsigned int chunkLen = (text.length() - textPos >= CHUNK_LENGTH) ? CHUNK_LENGTH : (text.length() - textPos);
            memcpy(chunk, &text[textPos], chunkLen);
            if (8 - chunkLen > 0) {
                memset(chunk + chunkLen, 0x00, 8 - chunkLen);
            }

            unsigned int pos = 0;
            buffer[pos++] = 0x15;
            buffer[pos++] = ((textPos + 8 < text.length()) ? 0x00 : 0x80); // continuation bit

            for (unsigned int j = 0; j < CHUNK_LENGTH; j++) {
                buffer[pos++] = chunk[j];
                buffer[pos++] = 0x00;             // filler
            }
        #if 1
            // trying to write 18 bytes: this works under Linux but not under Windows 7/10
            status = dev.WriteReportOut(buffer, sizeof(buffer));
            if (status != 0) {
                LOG("Error trying to write whole buffer");
                return status;
            }
        #else
            // trying to split buffer
            Sleep(10);
            for (unsigned int j=0; j<sizeof(buffer)/2; j++) {
                LOG("j = %u, writing 0x%02X, 0x%02X", j, buffer[j*2], buffer[j*2+1]);
                status = dev.WriteReportOut(&buffer[j*2], 2);
                if (status != 0)
                    return status;
                Sleep(10);
            }
        #endif
        }
    }
    return status;
}

int UpdateDisplay(HidDevice &dev) {
    int status;

    displayUpdateFlag = false;

    std::string callDisplayVal = GetCallDisplay();

    status = 0;

#if 1
    status = ClearDisplay(dev);
    if (status != 0) {
        LOG("ClearDisplay error %d", status);
    }
#endif

    status = SetDisplayTwoLines(dev, "abc", "123");

    if (status != 0) {
        LOG("UpdateDisplay status/error = %d", status);
    }
    return status;
}

int UpdateRing(HidDevice &dev) {
    int status;

    ringUpdateFlag = false;


    LOG("UpdateRing: state = %d, type = %u", ringState, customConf.ringType);

    status = 0;
    Sleep(10);
    return status;
}

int SendKeepalive(HidDevice &dev) {
    // Send feature report - without this the phone asks to upgrade Office Communicator
    // report id = 0x17, language (0x09 = EN)
    unsigned char sendbuf[] = {0x17, 0x09, 0x04, 0x01, 0x02};
    int status = dev.WriteReport(HidDevice::E_REPORT_FEATURE, sendbuf[0], sendbuf+1, sizeof(sendbuf)-1);
    if (status != 0) {
        LOG("Error sending keepalive: %s", HidDevice::GetErrorDesc(status).c_str());
    } else {
        DET_LOG("Keepalive sent");
    }
    return status;
}

int SetLed(HidDevice &dev, const uint8_t *leds) {
    // According to Wireshark this sends 3 bytes, not 2.
    // Python with cx300.py and hid/hidapi behaves the same way under Windows.
    // WTF?

    // temporary buffer to make sure third byte is set to 0, otherwise with third byte by = 0x16
    // voicemail LED is lit and mute LED is lit
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, leds, sizeof(STATUS_LED_GREEN));
    int status;
    status = hidDevice.WriteReportOut(buf, sizeof(STATUS_LED_GREEN));
    //status = hidDevice.WriteReport(HidDevice::E_REPORT_OUT, 0, STATUS_LED_GREEN, sizeof(STATUS_LED_GREEN));
    return status;
}

}

void PolycomCX300::Poll(void) {
    static unsigned int loopCnt = 0;
    if (!hidDevice.IsOpened()) {
        if (loopCnt % 100 == 0) {
            int status = hidDevice.Open(VendorID, ProductID, NULL, NULL);
            if (status == 0) {
                LOG("HID device connected");

                if (customConf.detailedLogging) {
                    static bool once = false;
                    if (!once) {
                        once = true;
                        std::string dump;
                        status = hidDevice.DumpCapabilities(dump);
                        LOG("%s", dump.c_str());
                    }
                }

                status = SendKeepalive(hidDevice);
                if (status != 0) {
                    hidDevice.Close();
                }

                ClearDisplay(hidDevice);

                const uint8_t* leds[] = {   STATUS_LED_GREEN, STATUS_LED_RED, STATUS_LED_ORANGE_RED,
                                            STATUS_LED_ORANGE, STATUS_LED_GREEN_ORANGE, STATUS_LED_OFF  };
                for (unsigned int i=0; i<sizeof(leds)/sizeof(leds[0]); i++) {
                    //LOG("Writing LED pattern #%u", i);
                    status = SetLed(hidDevice, leds[i]);
                    if (status != 0) {
                        LOG("Error writing LED pattern");
                        hidDevice.Close();
                        break;
                    }
                    Sleep(300);
                }

            } else {
                LOG("Error opening HID device: %s", HidDevice::GetErrorDesc(status).c_str());
            }
        }
    } else {
        int status = 0;
        if (callState == 0) {
            if ((loopCnt & 0x03) == 0) {
                // updating time
                displayUpdateFlag = true;
            }
        }

        if ((loopCnt & 0x03) == 0) {
            if (ringState) {
                if ((loopCnt & 0x07) == 0) {
                    status = SetLed(hidDevice, STATUS_LED_RED);
                } else {
                    status = SetLed(hidDevice, STATUS_LED_OFF);
                }
            } else {
                if (regState)
                    status = SetLed(hidDevice, STATUS_LED_GREEN);
                else
                    status = SetLed(hidDevice, STATUS_LED_OFF);
            }
        }

        if (status == 0 && ((loopCnt & 0x1FF) == 0)) {
            status = SendKeepalive(hidDevice);
        }
#if 0
        if (status == 0 && displayUpdateFlag) {
            status = UpdateDisplay(hidDevice);
        }
#endif
        if (status == 0 && ringUpdateFlag) {
            status = UpdateRing(hidDevice);
        }

        if (status) {
            LOG("Error updating, %s", HidDevice::GetErrorDesc(status).c_str());
            hidDevice.Close();
        } else {
            unsigned char rcvbuf[REPORT_IN_SIZE];
            memset(rcvbuf, 0, sizeof(rcvbuf));
            int size = sizeof(rcvbuf);
            int status = hidDevice.ReadReport(HidDevice::E_REPORT_IN, 0, (char*)rcvbuf, &size, 100);
            if (status == 0) {
                if (size == sizeof(rcvbuf)) {
                    DET_LOG("REPORT_IN received: %02X %02X %02X %02X %02X %02X %02X %02X",
                        rcvbuf[0], rcvbuf[1], rcvbuf[2], rcvbuf[3], rcvbuf[4], rcvbuf[5], rcvbuf[6], rcvbuf[7]
                    );
                    HandleReportIn(rcvbuf);
                } else {
                    LOG("Unexpected REPORT_IN size = %d", size);
                }
            } else if (status != HidDevice::E_ERR_TIMEOUT) {
                LOG("Error reading report");
                hidDevice.Close();
            }
        }
    }
    loopCnt++;
}

void PolycomCX300::Close(void) {
    hidDevice.Close();
}


void UpdateCallState(int state, const char* display) {
    ScopedLock<Mutex> lock(mutexState);
    callState = state;
    callDisplay = display;
    displayUpdateFlag = true;
}

void UpdateRing(int state) {
    if (ringState != state) {
        ringState = state;
        ringUpdateFlag = true;
    }
    //LOG("ringState = %d", ringState);
    // Does CX300 has a ringer? Probably not.
}

void UpdateRegistrationState(int state) {
    regState = state;
    //LOG("regState = %d", regState);
    displayUpdateFlag = true;
}
