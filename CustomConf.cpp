#include "CustomConf.h"
#include <json/json.h>

namespace {
    enum { RING_TYPE_MAX = 5 }; // 0...5
}

CustomConf customConf;

CustomConf::CustomConf(void):
    detailedLogging(false),
    ringType(0),
    dialKey("#"),
    extraTextUdpPort(0),
    extraTextMaxChars(14),
    extraTextSeparator("   ")
{

}

void CustomConf::toJson(Json::Value &jv) const
{
    jv = Json::Value(Json::objectValue);
    jv["detailedLogging"] = detailedLogging;
    jv["ringType"] = ringType;
    jv["dialKey"] = dialKey;
    jv["extraTextFile"] = extraTextFile;
    jv["extraTextUdpPort"] = extraTextUdpPort;
    jv["extraTextMaxChars"] = extraTextMaxChars;
    jv["extraTextSeparator"] = extraTextSeparator;
}

void CustomConf::fromJson(const Json::Value &jv)
{
    if (jv.type() != Json::objectValue)
        return;
    jv.getBool("detailedLogging", detailedLogging);
    unsigned int tmp = ringType;
    jv.getUInt("ringType", tmp);
    if (tmp < RING_TYPE_MAX)
        ringType = tmp;
    jv.getString("dialKey", dialKey);
    jv.getString("extraTextFile", extraTextFile);
    jv.getUInt("extraTextUdpPort", extraTextUdpPort);
    jv.getUInt("extraTextMaxChars", extraTextMaxChars);
    jv.getString("extraTextSeparator", extraTextSeparator);
}
