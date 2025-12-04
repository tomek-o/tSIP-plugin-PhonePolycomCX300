#include "HidDevice.h"
#include "Log.h"
#include "bin2str.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <initguid.h>
extern "C"
{
#include <ddk/hidsdi.h>
#include <setupapi.h>
}
#include <dbt.h>

#include <sstream>
#include <iostream>
#include <algorithm>
#include <assert.h>

using namespace nsHidDevice;
using namespace std;


namespace
{
std::string GetLastErrorMessage(DWORD dw)
{
    LPVOID lpMsgBuf = NULL;
    int rc = FormatMessage(
                 FORMAT_MESSAGE_ALLOCATE_BUFFER |
                 FORMAT_MESSAGE_FROM_SYSTEM |
                 FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL,
                 dw,
                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 //MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),	// fails on one of my PC - probably requires language package
                 (LPTSTR) &lpMsgBuf,
                 0, NULL );
    if (rc == 0)
    {
        if (lpMsgBuf)
            LocalFree(lpMsgBuf);
        return "FormatMessage failed!";
    }
    std::string msg = (LPCTSTR)lpMsgBuf;
    // remove trailing newline
    msg.erase(std::remove(msg.begin(), msg.end(), '\r'), msg.end());
    msg.erase(std::remove(msg.begin(), msg.end(), '\n'), msg.end());
    LocalFree(lpMsgBuf);
    return msg;
}
}


HidDevice::SERROR HidDevice::tabErrorsName[E_ERR_LIMIT] =
{
    { E_ERR_INV_PARAM,                  "Invalid parameter" },
    { E_ERR_NOTFOUND,                   "Device not found" },
    { E_ERR_IO,                         "Error calling I/O function" },
    { E_ERR_TIMEOUT,                    "Timeout" },
    { 0,		                        "No error" }
};

/** \brief Get short error message
*/
std::string HidDevice::GetErrorDesc(int ErrorCode)
{
    int a=0;
    std::stringstream stream;
    while (tabErrorsName[a].nCode > 0)
    {
        if (tabErrorsName[a].nCode == ErrorCode)
        {
            stream << tabErrorsName[a].lpName;
            return stream.str();
        }
        a++;
    }
    return stream.str();
}

void HidDevice::UnicodeToAscii(char *buffer)
{
    unsigned short  *unicode = (unsigned short*)buffer;
    char            *ascii = buffer;
    while (*unicode)
    {
        if (*unicode >= 256)
            *ascii++ = '?';
        else
            *ascii++ = *unicode++;
    }
    *ascii++ = 0;
}

HidDevice::HidDevice(void):
    handle(INVALID_HANDLE_VALUE),
    readHandle(INVALID_HANDLE_VALUE),
    writeHandle(INVALID_HANDLE_VALUE),
    hEventObject(NULL),
    VID(0),
    PID(0),
    usagePage(-1),
    preparsedData(NULL),
    reportInLength(0),
    reportOutLength(0)
{
    pOverlapped = new OVERLAPPED;
    HidD_GetHidGuid(&hidGuid);
}

HidDevice::~HidDevice(void)
{
    OVERLAPPED *o = (OVERLAPPED*)pOverlapped;
    delete o;
    if (preparsedData)
        HidD_FreePreparsedData(preparsedData);
    Close();
}

void HidDevice::GetHidGuid(GUID *guid) const
{
    *guid = hidGuid;
}

int HidDevice::Open(int VID, int PID, char *vendorName, char *productName, int usagePage)
{
    HDEVINFO                            deviceInfoList;
    SP_DEVICE_INTERFACE_DATA            deviceInfo;
    SP_DEVICE_INTERFACE_DETAIL_DATA     *deviceDetails = NULL;
    DWORD                               size;
    int                                 i, openFlag = 0;  /* may be FILE_FLAG_OVERLAPPED */
    int                                 errorCode = E_ERR_NOTFOUND;
    HIDD_ATTRIBUTES                     deviceAttributes;

    this->usagePage = usagePage;
    deviceInfoList = SetupDiGetClassDevs(&hidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);
    deviceInfo.cbSize = sizeof(deviceInfo);
    for (i=0;; i++)
    {
        if (handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
        }
        if (!SetupDiEnumDeviceInterfaces(deviceInfoList, 0, &hidGuid, i, &deviceInfo))
            break;
        // check actual size to allocate buffer
        SetupDiGetDeviceInterfaceDetail(deviceInfoList, &deviceInfo, NULL, 0, &size, NULL);
        if (deviceDetails != NULL)
            free(deviceDetails);
        deviceDetails = (SP_DEVICE_INTERFACE_DETAIL_DATA*)malloc(size);
        deviceDetails->cbSize = sizeof(*deviceDetails);
        // call using allocated buffer
        SetupDiGetDeviceInterfaceDetail(deviceInfoList, &deviceInfo, deviceDetails, size, &size, NULL);

        // note: writing/reading may be unsupported
        handle = CreateFile(deviceDetails->DevicePath, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, openFlag, NULL);
        if (handle == INVALID_HANDLE_VALUE)
        {
            continue;
        }

        // check VID + PID
        deviceAttributes.Size = sizeof(deviceAttributes);
        HidD_GetAttributes(handle, &deviceAttributes);
        //LOG("Found device VID = %04X, PID = %04X, path = %s", deviceAttributes.VendorID, deviceAttributes.ProductID, deviceDetails->DevicePath);
        if (VID != 0 && deviceAttributes.VendorID != VID)
            continue;
        if (PID != 0 && deviceAttributes.ProductID != PID)
            continue;

        errorCode = E_ERR_NOTFOUND;
        if (vendorName != NULL)
        {
            char buffer[512];
            if (!HidD_GetManufacturerString(handle, buffer, sizeof(buffer)))
            {
                errorCode = E_ERR_IO;
                continue;
            }
            UnicodeToAscii(buffer);
            if (strcmp(vendorName, buffer) != 0)
                continue;
        }
        if (productName != NULL)
        {
            char buffer[512];
            if (!HidD_GetProductString(handle, buffer, sizeof(buffer)))
            {
                errorCode = E_ERR_IO;
                continue;
            }
            UnicodeToAscii(buffer);
            if (strcmp(productName, buffer) != 0)
                continue;
        }

        if (usagePage >= 0)
        {
            // returns a pointer to a buffer containing the information about the device's capabilities.
            if (HidD_GetPreparsedData(handle, &preparsedData) == FALSE)
            {
                errorCode = E_ERR_IO;
                continue;
            }

            HIDP_CAPS Capabilities;
            if (HidP_GetCaps(preparsedData, &Capabilities) != HIDP_STATUS_SUCCESS)
            {
                errorCode = E_ERR_IO;
                continue;
            }
            LOG("Device UsagePage = 0x%X", Capabilities.UsagePage);
            if (Capabilities.UsagePage != usagePage /*0x0b*/)
                continue;

            this->VID = deviceAttributes.VendorID;
            this->PID = deviceAttributes.ProductID;

            reportInLength = Capabilities.InputReportByteLength;
            reportOutLength = Capabilities.OutputReportByteLength;

            HidD_FlushQueue(handle);

        }

        break;
    }

    if (handle != INVALID_HANDLE_VALUE)
    {
        path = deviceDetails->DevicePath;
        errorCode = CreateReadWriteHandles(deviceDetails->DevicePath);
    }

    SetupDiDestroyDeviceInfoList(deviceInfoList);
    if (deviceDetails != NULL)
        free(deviceDetails);
    return errorCode;
}

bool HidDevice::IsOpened(void) const
{
    return (handle != INVALID_HANDLE_VALUE);
}

int HidDevice::CreateReadWriteHandles(std::string path)
{
    writeHandle = CreateFile (path.c_str(), GENERIC_WRITE,
                              FILE_SHARE_READ|FILE_SHARE_WRITE,
                              (LPSECURITY_ATTRIBUTES)NULL,
                              OPEN_EXISTING,
                              0,
                              NULL);
    if (writeHandle == INVALID_HANDLE_VALUE)
    {
        LOG("Failed to create write handle!");
        return E_ERR_IO;
    }
    readHandle = CreateFile	(path.c_str(), GENERIC_READ,
                             FILE_SHARE_READ|FILE_SHARE_WRITE,
                             (LPSECURITY_ATTRIBUTES)NULL,
                             OPEN_EXISTING,
                             FILE_FLAG_OVERLAPPED,
                             NULL);
    if (readHandle == INVALID_HANDLE_VALUE)
    {
        LOG("Failed to create read handle!");
        return E_ERR_IO;
    }

    if (hEventObject == 0)
    {
        hEventObject = CreateEvent
                       (NULL,  // security
                        TRUE,   // manual reset (call ResetEvent)
                        TRUE,   // initial state = signaled
                        "");    // name
        if (hEventObject == NULL)
        {
            LOG("Failed to create event handle!");
            return E_ERR_OTHER;
        }
        ((OVERLAPPED*)pOverlapped)->hEvent = hEventObject;
        ((OVERLAPPED*)pOverlapped)->Offset = 0;
        ((OVERLAPPED*)pOverlapped)->OffsetHigh = 0;
    }
    return 0;
}


int HidDevice::DumpCapabilities(std::string &dump)
{
    PHIDP_PREPARSED_DATA	PreparsedData;

    // returns a pointer to a buffer containing the information about the device's capabilities.
    if (HidD_GetPreparsedData(handle, &PreparsedData) == FALSE)
        return E_ERR_IO;

    HIDP_CAPS Capabilities;
    if (HidP_GetCaps(PreparsedData, &Capabilities) != HIDP_STATUS_SUCCESS)
    {
        LOG("HidP_GetCaps failed!");
        return E_ERR_IO;
    }

    std::stringstream stream;
    stream << "Usage Page: 0x" << hex << Capabilities.UsagePage << endl;
    stream << dec;
    stream << "Input Report Byte Length: " << Capabilities.InputReportByteLength << endl;
    stream << "Output Report Byte Length: " << Capabilities.OutputReportByteLength << endl;
    stream << "Feature Report Byte Length: " << Capabilities.FeatureReportByteLength << endl;
    stream << "Number of Link Collection Nodes: " << Capabilities.NumberLinkCollectionNodes << endl;
    stream << "Number of Input Button Caps: " << Capabilities.NumberInputButtonCaps << endl;
    stream << "Number of InputValue Caps: " << Capabilities.NumberInputValueCaps << endl;
    stream << "Number of InputData Indices: " << Capabilities.NumberInputDataIndices << endl;
    stream << "Number of Output Button Caps: " << Capabilities.NumberOutputButtonCaps << endl;
    stream << "Number of Output Value Caps: " << Capabilities.NumberOutputValueCaps << endl;
    stream << "Number of Output Data Indices: " << Capabilities.NumberOutputDataIndices << endl;
    stream << "Number of Feature Button Caps: " << Capabilities.NumberFeatureButtonCaps << endl;
    stream << "Number of Feature Value Caps: " << Capabilities.NumberFeatureValueCaps << endl;
    stream << "Number of Feature Data Indices: " << Capabilities.NumberFeatureDataIndices << endl;

    dump = stream.str();
    HidD_FreePreparsedData(PreparsedData);
    return 0;
}

void HidDevice::Close(void)
{
    if (handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }
    if (writeHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(writeHandle);
        writeHandle = INVALID_HANDLE_VALUE;
    }
    if (readHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(readHandle);
        readHandle = INVALID_HANDLE_VALUE;
    }
}

int HidDevice::WriteReport(enum E_REPORT_TYPE type, int id, const unsigned char *buffer, int len)
{
    BOOL status = 0;
    DWORD bytesWritten = 0;
    unsigned char sendbuf[65];
    memset(sendbuf, 0, sizeof(sendbuf));
    sendbuf[0] = id;
    assert (len < (int)sizeof(sendbuf));
    memcpy(sendbuf+1, buffer, std::min((int)sizeof(sendbuf)-1, len));

    switch (type)
    {
    case E_REPORT_IN:
        return E_ERR_INV_PARAM;
    case E_REPORT_OUT:
        status = WriteFile(writeHandle, sendbuf, len+1, &bytesWritten, NULL);
        break;
    case E_REPORT_FEATURE:
        status = HidD_SetFeature(handle, sendbuf, sizeof(sendbuf));
        break;
    default:
        return E_ERR_INV_PARAM;
    }

    if (status == FALSE)
    {
        DWORD dw = GetLastError();
        LOG("Error: WriteReport, len = %d, HEX: %s, GetLastError = %d (%s)", len+1, BufToHexString(sendbuf, len+1).c_str(), dw, GetLastErrorMessage(dw).c_str());
    }

    return status == 0 ? E_ERR_IO : 0;
}

int HidDevice::WriteReportOut(const unsigned char *buffer, int len)
{
    BOOL status = FALSE;
    DWORD   bytesWritten = 0;

    SetLastError(0);
    status = WriteFile(writeHandle, buffer, len, &bytesWritten, NULL);
    if (status == FALSE)
    {
        DWORD dw = GetLastError();
        LOG("Error: WriteReportOut, len = %d, HEX: %s, GetLastError = %d (%s)", len, BufToHexString(buffer, len).c_str(), dw, GetLastErrorMessage(dw).c_str());
    }
    return status == FALSE ? E_ERR_IO : 0;
}

/* ------------------------------------------------------------------------ */

int HidDevice::ReadReport(enum E_REPORT_TYPE type, int id, char *buffer, int *len, int timeout)
{
    BOOL status = 0;
    DWORD bytesRead = 0;
    DWORD result;

    int outBufSize = *len;

    char rcvbuf[65];
    memset(rcvbuf, 0, sizeof(rcvbuf));
    rcvbuf[0] = id;
    assert (*len < (int)sizeof(rcvbuf));
    *len += 1;

    switch (type)
    {
    case E_REPORT_IN:
        status = ReadFile(readHandle, rcvbuf, *len, &bytesRead, (LPOVERLAPPED)pOverlapped);
        if( !status )
        {
            DWORD dw = GetLastError();
            if( GetLastError() == ERROR_IO_PENDING )
            {
                result = WaitForSingleObject(hEventObject, timeout);
                switch (result)
                {
                case WAIT_OBJECT_0:
                    *len = outBufSize;
                    ResetEvent(hEventObject);
                    memcpy(buffer, rcvbuf+1, *len);
                    return 0;
                case WAIT_TIMEOUT:
                    result = CancelIo(readHandle);
                    ResetEvent(hEventObject);
                    return E_ERR_TIMEOUT;
                default:
                    ResetEvent(hEventObject);
                    return E_ERR_IO;
                }
            }
            else
            {
                LOG("Error: ReadReport, GetLastError = %d (%s)", dw, GetLastErrorMessage(dw).c_str());
                return E_ERR_IO;
            }
        }
        else
        {
            *len = outBufSize;
            memcpy(buffer, rcvbuf+1, *len);
            return 0;
        }
        break;
    case E_REPORT_OUT:
        return E_ERR_INV_PARAM;
    case E_REPORT_FEATURE:
        // Capabilities.FeatureReportByteLength?
        status = HidD_GetFeature(handle, rcvbuf, *len);
        if (status)
            memcpy(buffer, rcvbuf+1, std::min<int>(*len, outBufSize));
        break;
    default:
        return E_ERR_INV_PARAM;
    }
    return status == 0 ? E_ERR_IO : 0;
}

