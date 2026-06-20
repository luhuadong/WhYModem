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
};

#endif // APPCONFIG_H
