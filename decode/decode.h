#ifndef DECODE_H
#define DECODE_H

#include <QList>
#include <QObject>
#include <QUrl>
#include <QtConcurrent>

enum OutputFormat { None, Text, Jaero, JsonDump, JsonAero };

struct ForwardTarget {
  QUrl target;
  OutputFormat format;

  ForwardTarget(const QUrl &url, OutputFormat fmt) : target(url), format(fmt) {}

  static ForwardTarget *fromRaw(const QString &raw);
};

class Decoder : public QObject {
  Q_OBJECT

public:
  Decoder(const QString &publisher, const QString &topic, const QString &format,
          int bitRate, bool burstMode, const QString &rawForwarders,
          bool disableReassembly, QObject *parent = nullptr);
  ~Decoder();

  bool isRunning() const { return running; }
  
private:
  void parseForwarder(const QString &raw);
  void publisherConsumer();

  const QList<int> validBitRates = {600, 1200, 10500};
  
  QFuture<void> consumerThread;
  bool running;

  void *volatile zmqContext;
  void *volatile zmqSub;

  bool burstMode;
  bool disableReassembly;
  int bitRate;
  
  QString publisher;
  QString topic;
  OutputFormat format;

  QList<ForwardTarget> forwarders;

public slots:
  void run();

  void handleHup();
  void handleInterrupt();
  void handleTerminate();

signals:
  void completed();
  void audioReceived(const QByteArray &, quint32);
};

#endif
