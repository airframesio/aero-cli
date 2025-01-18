#ifndef DECODE_H
#define DECODE_H

#include "aerol.h"
#include "forwarder.h"
#include "hunter.h"
#include "mskdemodulator.h"
#include "oqpskdemodulator.h"
#include <QByteArray>
#include <QList>
#include <QObject>
#include <QReadWriteLock>
#include <QUrl>
#include <QWaitCondition>
#include <QtConcurrent>

class Decoder : public QObject {
  Q_OBJECT

public:
  Decoder(const QString &station_id, const QString &publisher,
          const QString &topic, const QString &format, int bitRate,
          bool burstMode, const QString &rawForwarders, bool disableReassembly,
          QObject *parent = nullptr);
  Decoder(const Decoder &) = delete;
  Decoder(Decoder &&) noexcept = delete;
  ~Decoder();

  Decoder &operator=(const Decoder &) = delete;
  Decoder &operator=(Decoder &&) noexcept = delete;
  
  bool isRunning() const { return running; }
  void setNoSignalExit(bool noSignalExit) { this->noSignalExit = noSignalExit; }

private:
  bool parseForwarder(const QString &raw);
  void publisherConsumer();
  void forwarderConsumer();

  const QList<int> validBitRates = {600, 1200, 10500};

  QFuture<void> consumerThread;
  QFuture<void> forwarderThread;

  QList<ACARSItem> sendBuffer;
  QReadWriteLock sendBufferRwLock;
  QWaitCondition sendBufferCondition;
  
  QAtomicInt running;
  
  void *volatile zmqContext;
  void *volatile zmqSub;

  bool noSignalExit;
  bool burstMode;
  bool disableReassembly;
  int bitRate;

  QString publisher;
  QString stationId;
  QString topic;
  OutputFormat format;

  QList<ForwardTarget *> forwarders;
  
  AeroL *aerol;
  MskDemodulator *mskDemod;
  OqpskDemodulator *oqpskDemod;

  SignalHunter *hunter;

public slots:
  void run();

  void handleHup();
  void handleInterrupt();
  void handleTerminate();

  void handleNoSignalAfterFullScan();
  void handleNewFreqCenter(double freq_center);
  void handleDcdChange(bool old_state, bool new_state);
  void handleACARS(ACARSItem &item);

signals:
  void completed();
  void audioReceived(const QByteArray &, quint32);
};

#endif
