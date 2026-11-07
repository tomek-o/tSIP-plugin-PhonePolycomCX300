#include "ExtraText.h"
#include "CustomConf.h"
#include "Log.h"
#include <winsock2.h>
#include <windows.h>
#include <string.h>

namespace {

SOCKET sock = INVALID_SOCKET;
bool wsaStarted = false;
bool initialized = false;
std::string text;
bool changed = false;
unsigned int fileCheckCnt = 0;
FILETIME lastFileTime;
DWORD lastFileSize = 0;
bool haveFileState = false;

enum { MAX_DATAGRAM = 256 };
enum { FILE_CHECK_INTERVAL = 10 };  // in Poll() calls

void SetText(const std::string &raw) {
    std::string s = ExtraText::Sanitize(raw, customConf.extraTextMaxChars);
    if (s != text) {
        text = s;
        changed = true;
    }
}

void PollUdp(void) {
    if (sock == INVALID_SOCKET)
        return;
    // handle a limited number of datagrams per call
    for (int i = 0; i < 8; i++) {
        char buf[MAX_DATAGRAM + 1];
        int len = recvfrom(sock, buf, MAX_DATAGRAM, 0, NULL, NULL);
        if (len == SOCKET_ERROR) {
            // WSAEWOULDBLOCK = nothing to read, WSAECONNRESET = ICMP port unreachable echo (UDP), both harmless
            return;
        }
        SetText(std::string(buf, len));
    }
}

void PollFile(void) {
    if (customConf.extraTextFile.empty())
        return;
    if ((fileCheckCnt++ % FILE_CHECK_INTERVAL) != 0)
        return;

    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (!GetFileAttributesEx(customConf.extraTextFile.c_str(), GetFileExInfoStandard, &attr)) {
        if (haveFileState) {
            // file removed -> clear text
            haveFileState = false;
            SetText("");
        }
        return;
    }
    if (haveFileState && CompareFileTime(&attr.ftLastWriteTime, &lastFileTime) == 0 && attr.nFileSizeLow == lastFileSize)
        return;
    lastFileTime = attr.ftLastWriteTime;
    lastFileSize = attr.nFileSizeLow;
    haveFileState = true;

    HANDLE h = CreateFile(customConf.extraTextFile.c_str(), GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        haveFileState = false;  // retry later
        return;
    }
    char buf[MAX_DATAGRAM];
    DWORD got = 0;
    if (!ReadFile(h, buf, sizeof(buf), &got, NULL))
        got = 0;
    CloseHandle(h);
    std::string content(buf, got);
    // only first line
    size_t eol = content.find_first_of("\r\n");
    if (eol != std::string::npos)
        content.resize(eol);
    SetText(content);
}

}   // namespace

std::string ExtraText::Sanitize(const std::string &in, unsigned int maxChars) {
    std::string out;
    for (size_t i = 0; i < in.length(); i++) {
        unsigned char c = static_cast<unsigned char>(in[i]);
        if (c == '\r' || c == '\n' || c == 0)
            break;  // only the first line
        out += (c >= 0x20 && c <= 0x7E) ? static_cast<char>(c) : '?';
    }
    size_t b = out.find_first_not_of(' ');
    size_t e = out.find_last_not_of(' ');
    out = (b == std::string::npos) ? std::string() : out.substr(b, e - b + 1);
    if (maxChars > 0 && out.length() > maxChars) {
        out.resize(maxChars);
        size_t last = out.find_last_not_of(' ');
        out = (last == std::string::npos) ? std::string() : out.substr(0, last + 1);
    }
    return out;
}

void ExtraText::Init(void) {
    initialized = true;
    text.clear();
    changed = false;
    haveFileState = false;

    if (customConf.extraTextUdpPort == 0 || customConf.extraTextUdpPort > 65535)
        return;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        LOG("ExtraText: WSAStartup failed");
        return;
    }
    wsaStarted = true;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        LOG("ExtraText: socket() failed, error = %d", WSAGetLastError());
        return;
    }
    u_long nonBlocking = 1;
    ioctlsocket(sock, FIONBIO, &nonBlocking);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // loopback only - never reachable from the network
    addr.sin_port = htons(static_cast<u_short>(customConf.extraTextUdpPort));
    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        LOG("ExtraText: bind to 127.0.0.1:%u failed, error = %d", customConf.extraTextUdpPort, WSAGetLastError());
        closesocket(sock);
        sock = INVALID_SOCKET;
        return;
    }
    LOG("ExtraText: listening on UDP 127.0.0.1:%u", customConf.extraTextUdpPort);
}

void ExtraText::Poll(void) {
    if (!initialized)
        Init();
    PollUdp();
    PollFile();
}

void ExtraText::Close(void) {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    if (wsaStarted) {
        WSACleanup();
        wsaStarted = false;
    }
    initialized = false;
}

std::string ExtraText::Get(void) {
    return text;
}

bool ExtraText::ConsumeChanged(void) {
    bool c = changed;
    changed = false;
    return c;
}
