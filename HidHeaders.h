#ifndef HIDHEADERS_H_INCLUDED
#define HIDHEADERS_H_INCLUDED

/** \brief Portable selection of the Windows HID headers

    - mingw.org / older MinGW (Code::Blocks on Windows): ddk/hidusage.h, ddk/hidpi.h, ddk/hidsdi.h
    - recent mingw-w64 (e.g. i686-w64-mingw32 cross-compiler on Linux): same headers directly in the
      include directory, hidpi.h needs NTSTATUS and has no extern "C" guard
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#if defined(__has_include)
#   if !__has_include(<ddk/hidpi.h>) && __has_include(<hidpi.h>)
#       define HID_HEADERS_FLAT
#   endif
#endif

#ifdef HID_HEADERS_FLAT
#   include <ntstatus.h>
#   ifndef _NTDEF_
        typedef LONG NTSTATUS;
#   endif
    extern "C" {
#   include <hidusage.h>
#   include <hidpi.h>
    }
#   define HID_SDI_HEADER <hidsdi.h>
#else
#   include <ddk/hidusage.h>
#   include <ddk/hidpi.h>
#   define HID_SDI_HEADER <ddk/hidsdi.h>
#endif

#endif // HIDHEADERS_H_INCLUDED
