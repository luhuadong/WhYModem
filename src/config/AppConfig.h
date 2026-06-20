#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QString>

struct TransmitDelayConfig
{
    int firstDataDelayMs;
    int interPacketDelayMs;
};

class AppConfig
{
public:
    static QString configFilePath();
    static void ensureConfigFile();
    static TransmitDelayConfig transmitDelays();
    static void setTransmitDelays(const TransmitDelayConfig &config);
};

#endif // APPCONFIG_H
