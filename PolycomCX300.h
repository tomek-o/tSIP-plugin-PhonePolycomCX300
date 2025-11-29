#ifndef PolycomCX300H
#define PolycomCX300H

namespace PolycomCX300
{
    void Poll(void);
    void Close(void);
}

void UpdateRegistrationState(int state);
void UpdateCallState(int state, const char* display);
void UpdateRing(int state);

#endif
