#include "CommThread.h"
#include "Log.h"
#include "PolycomCX300.h"
#include <windows.h>
#include <assert.h>
#include <time.h>
#include <vector>

namespace {

volatile bool connected = false;
volatile bool exited = false;

}

DWORD WINAPI CommThreadProc(LPVOID data) {
    LOG("Running comm thread");

    while (connected) {
        PolycomCX300::Poll();
        Sleep(50);
    }

    PolycomCX300::Close();
    exited = true;
    return 0;
}


int CommThreadStart(void) {
    DWORD dwtid;
    exited = false;
    connected = true;
    HANDLE CommThread = CreateThread(NULL, 0, CommThreadProc, /*this*/NULL, 0, &dwtid);
    if (CommThread == NULL) {
        connected = false;
        exited = true;
    }

    return 0;
}

int CommThreadStop(void) {
    connected = false;
    while (!exited) {
        Sleep(50);
    }
    return 0;
}
