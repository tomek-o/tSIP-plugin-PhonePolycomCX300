#include "CustomConf.h"
#include <json/json.h>

namespace {
    enum { RING_TYPE_MAX = 5 }; // 0...5
}

CustomConf customConf;

CustomConf::CustomConf(void):
    detailedLogging(false),
    ringType(0),
    dialKey("#")
{

}

void CustomConf::toJson(Json::Value &jv) const
{
    jv = Json::Value(Json::objectValue);
    jv["detailedLogging"] = detailedLogging;
    jv["ringType"] = ringType;
    jv["dialKey"] = dialKey;
}

void CustomConf::fromJson(const Json::Value &jv)
{
    if (jv.type() != Json::objectValue)
        return;
    jv.getBool("detailedLogging", detailedLogging);
    unsigned int tmp;
    jv.getUInt("ringType", tmp);
    if (tmp < RING_TYPE_MAX)
        ringType = tmp;
    jv.getString("dialKey", dialKey);
}
