#ifndef bin2strH
#define bin2strH

#include <string>

    int hexStringToInt (std::string);
    std::string intToHexString (int);
    int binStringToInt (std::string);
    std::string intToBinString (int);
    std::string HexStringToBuf(std::string);
    std::string BufToHexString(std::string);
    std::string BufToHexString(const unsigned char* data, unsigned int length);

#endif

