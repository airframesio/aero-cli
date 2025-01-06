#ifndef PUBLISHER_H
#define PUBLISHER_H

#include <QDebug>
#include <QObject>
#include <QSocketNotifier>
#include <SoapySDR/Device.hpp>

class Publisher : public QObject {
  Q_OBJECT

public:
  Publisher(bool enableBiast, bool enableDcc, const QString &settingsPath,
            QObject *parent = nullptr);
  ~Publisher();

private:
  bool loadSettings(const QString &settingsPath);
  
  bool enableBiast;
  bool enableDcc;

  SoapySDR::Device *device;

public slots:
  void run();
  
signals:
  void completed();
};

#endif
