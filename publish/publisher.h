#ifndef PUBLISHER_H
#define PUBLISHER_H

#include <QDebug>
#include <QObject>
#include <QSocketNotifier>
#include <QtConcurrent>
#include <SoapySDR/Device.hpp>

#include "vfo.h"

class Publisher : public QObject {
  Q_OBJECT

public:
  Publisher(const QString &deviceStr, bool enableBiast, bool enableDcc,
            const QString &settingsPath, QObject *parent = nullptr);
  ~Publisher();

private:
  bool loadSettings(const QString &settingsPath);
  void readerThread();

  QFuture<void> mainReader;

  bool running;
  bool enableBiast;
  bool enableDcc;

  int Fs;
  int center_frequency;
  int tuner_gain;
  int tuner_gain_idx;
  int tuner_idx;
  
  int buflen;

  int nVFO;
  QVector<vfo *> VFOs;
  QVector<vfo *> VFOsub[3];
  QVector<vfo *> VFOmain;

  std::vector<cpx_typef> demodSamples;

  SoapySDR::Device *device;

public slots:
  void run();

  void handleInterrupt();

signals:
  void completed();
};

#endif
