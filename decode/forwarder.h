#ifndef FORWARDER_H
#define FORWARDER_H

#include <netdb.h>
#include <QObject>
#include <QUrl>

const int MAX_CONNECTION_WAIT_MS = 1000;

enum OutputFormat { None, Text, Jaero, JsonDump };

class ForwardTarget : public QObject {
  Q_OBJECT

public:
  ForwardTarget(const QUrl &url, OutputFormat fmt);
  ForwardTarget(const ForwardTarget &) = delete;
  ForwardTarget(ForwardTarget &&) noexcept = delete;
  ~ForwardTarget();

  ForwardTarget &operator=(const ForwardTarget &) = delete;
  ForwardTarget &operator=(ForwardTarget &&) noexcept = delete;

  void reconnect();
  void send(const QByteArray &data);

  OutputFormat getFormat() const { return format; }
  const QUrl &getTarget() const { return target; }
  
  static ForwardTarget *fromRaw(const QString &raw);

private:
  int sendFrame(const QByteArray &data);
  
  QString scheme;
  QUrl target;
  int connfd;
  addrinfo *servinfo;
  addrinfo *activeinfo;
  OutputFormat format;
};

OutputFormat parseOutputFormat(const QString &raw);

#endif
