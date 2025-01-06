#ifndef PUBLISHER_H
#define PUBLISHER_H

#include <QtConcurrent>
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
  void readerThread();
  
  QFuture<void> mainReader;

  bool running;
  bool enableBiast;
  bool enableDcc;
  
  SoapySDR::Device *device;

public slots:
  void run();

  void handleInterrupt();
  
signals:
  void completed();
};

#endif
