#include "config/AppConfig.h"
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QtGlobal>
#include <algorithm>

namespace {

const char *ConfigDirName = "WhYModem";
const char *ConfigFileName = "WhYModem.ini";
const char *TransmitGroup = "Transmit";
const char *FirstDataDelayKey = "FirstDataDelayMs";
const char *InterPacketDelayKey = "InterPacketDelayMs";

int readDelay(QSettings &settings, const char *key, int fallback)
{
    bool ok = false;
    const int value = settings.value(key, fallback).toInt(&ok);
    return std::max(0, ok ? value : fallback);
}

} // namespace

QString AppConfig::configFilePath()
{
    QString baseDir;
#if defined(Q_OS_WIN)
    baseDir = qEnvironmentVariable("APPDATA");
    if(baseDir.isEmpty())
    {
        baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
#else
    baseDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
#endif
    if(baseDir.isEmpty())
    {
        baseDir = QDir::home().filePath(".config");
    }

    return QDir(QDir(baseDir).filePath(ConfigDirName)).filePath(ConfigFileName);
}

void AppConfig::ensureConfigFile()
{
    const QString path = configFilePath();
    const QFileInfo info(path);
    QDir().mkpath(info.absolutePath());

    if(info.exists())
    {
        return;
    }

    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(TransmitGroup);
    settings.setValue(FirstDataDelayKey, 0);
    settings.setValue(InterPacketDelayKey, 0);
    settings.endGroup();
    settings.sync();
}

TransmitDelayConfig AppConfig::transmitDelays()
{
    ensureConfigFile();

    QSettings settings(configFilePath(), QSettings::IniFormat);
    settings.beginGroup(TransmitGroup);

    TransmitDelayConfig config;
    config.firstDataDelayMs = readDelay(settings, FirstDataDelayKey, 0);
    config.interPacketDelayMs = readDelay(settings, InterPacketDelayKey, 0);

    settings.endGroup();
    return config;
}
