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
    CustomConf(void);
    void toJson(Json::Value &jv) const;
    void fromJson(const Json::Value &jv);
};

extern CustomConf customConf;

#endif // CustomConfH
