#ifndef CustomConfH
#define CustomConfH

#include <string>

namespace Json
{
    class Value;
}

struct CustomConf
{
    bool detailedLogging;
    unsigned int ringType;
    std::string dialKey;
    std::string extraTextFile;          ///< optional file with extra display text (first line)
    unsigned int extraTextUdpPort;      ///< UDP port on 127.0.0.1 for extra display text, 0 = disabled
    unsigned int extraTextMaxChars;     ///< max. length of extra text
    std::string extraTextSeparator;     ///< inserted between clock and extra text
    CustomConf(void);
    void toJson(Json::Value &jv) const;
    void fromJson(const Json::Value &jv);
};

extern CustomConf customConf;

#endif // CustomConfH
