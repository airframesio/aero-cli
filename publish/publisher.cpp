#include <QFileInfo>
#include <QSettings>
#include <SoapySDR/Constants.h>
#include <SoapySDR/Device.hpp>
#include <qsettings.h>
#include <sys/socket.h>
#include <unistd.h>

#include "publisher.h"

Publisher::Publisher(const QString &deviceStr, bool enableBiast, bool enableDcc,
                     const QString &settingsPath, QObject *parent)
    : QObject(parent) {
  this->enableBiast = enableBiast;
  this->enableDcc = enableDcc;

  if (!loadSettings(settingsPath)) {
    qFatal("[ERROR] failed to parse and load settings");
  }

  device = SoapySDR::Device::make(deviceStr.toStdString());
  if (device == nullptr) {
    qFatal("");
  }

  device->setGainMode(SOAPY_SDR_RX, 0, 1);
  device->setDCOffsetMode(SOAPY_SDR_RX, 0, enableDcc);  
  device->writeSetting("biastee", enableBiast ? "true" : "false");
}

Publisher::~Publisher() {
  device->writeSetting("biastee", "false");
  
  if (device != nullptr) {
    SoapySDR::Device::unmake(device);
  }
}

bool Publisher::loadSettings(const QString &settingsPath) {
  QFileInfo info(settingsPath);
  if (!info.exists() || !info.isFile()) {
    qCritical() << "";
    return false;
  }
  
  QSettings settings(settingsPath, QSettings::IniFormat);

  Fs = settings.value("sample_rate").toInt();
  if (Fs == 0) {
    qCritical() << "";
    return false;
  }

  return true;
}

void Publisher::run() {
  running = true;
  mainReader = QtConcurrent::run([this] { readerThread(); });
}

void Publisher::readerThread() {

  // TODO: do stuff

  emit completed();
}

void Publisher::handleInterrupt() { running = false; }
