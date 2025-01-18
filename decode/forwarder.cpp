#include "forwarder.h"
#include "logger.h"
#include <qt5/QtCore/qglobal.h>
#include <sys/socket.h>
#include <unistd.h>

OutputFormat parseOutputFormat(const QString &raw) {
  QString norm = raw.toLower();

  if (norm == "text")
    return OutputFormat::Text;
  if (norm == "jaero")
    return OutputFormat::Jaero;
  if (norm == "jsondump")
    return OutputFormat::JsonDump;

  return OutputFormat::None;
}

ForwardTarget::ForwardTarget(const QUrl &url, OutputFormat fmt)
    : QObject(nullptr), connfd(-1), servinfo(nullptr), activeinfo(nullptr),
      target(url), format(fmt) {
  scheme = url.scheme().toLower();
}

ForwardTarget::~ForwardTarget() {
  if (servinfo != nullptr) {
    ::freeaddrinfo(servinfo);
    servinfo = nullptr;
    activeinfo = nullptr;
  }

  if (connfd != -1) {
    ::close(connfd);
    connfd = -1;
  }
}

void ForwardTarget::reconnect() {
  addrinfo hints = {0};
  addrinfo *servinfo = nullptr;
  addrinfo *p = nullptr;

  DBG("Attempting to connect to forwarder target %s", target.toString().toStdString().c_str());
  
  if (servinfo != nullptr) {
    ::freeaddrinfo(servinfo);
    servinfo = nullptr;
    activeinfo = nullptr;
  }

  if (connfd != -1) {
    ::close(connfd);
    connfd = -1;
  }

  ::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = (scheme == "tcp") ? SOCK_STREAM : SOCK_DGRAM;

  QString port = QString("%1").arg(target.port());

  if (::getaddrinfo(target.host().toStdString().c_str(),
                    port.toStdString().c_str(), &hints, &servinfo) != 0) {
    goto Exit;
  }

  for (p = servinfo; p != nullptr; p = p->ai_next) {
    connfd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (connfd == -1) {
      continue;
    }

    if (scheme == "tcp") {
      if (::connect(connfd, p->ai_addr, p->ai_addrlen) == -1) {
        ::close(connfd);
        connfd = -1;
        continue;
      }
    }

    activeinfo = p;
    break;
  }

Exit:
  if (connfd == -1) {
    DBG("Failed to connect to forwarder target");
  } else {
    DBG("Connected to forwarder target");
  }
}

int ForwardTarget::sendFrame(const QByteArray &data) {
  const std::string buf = data.toStdString();
  const char *raw = buf.c_str();

  int bytesWritten = -1;
  if (scheme == "tcp") {
    bytesWritten = ::send(connfd, raw, ::strlen(raw), MSG_NOSIGNAL);
  } else {
    bytesWritten = ::sendto(connfd, raw, ::strlen(raw), 0, activeinfo->ai_addr,
                            activeinfo->ai_addrlen);
  }

  return bytesWritten;
}

void ForwardTarget::send(const QByteArray &data) {
  DBG("Attempting to send %lld bytes to forwarding target %s", data.size(), target.toString().toStdString().c_str());

  if (connfd == -1) {
    DBG("Invalid socket detected, attempting reconnect");
    reconnect();  
  }
  
  int bytesWritten = sendFrame(data);
  if (bytesWritten == -1) {
    reconnect();

    if (connfd == -1) {
      DBG("Failed attempt to reconnect to forwarding target during send()");
      return;
    }

    bytesWritten = sendFrame(data);
    if (bytesWritten == -1) {
      DBG("Failed again to send frame to forwarding target");
    }
  } else {
    DBG("Sent %d to forwarding target", bytesWritten);
  }
}

ForwardTarget *ForwardTarget::fromRaw(const QString &raw) {
  if (raw.isEmpty()) {
    return nullptr;
  }

  QStringList tokens = raw.split("=");
  if (tokens.size() != 2) {
    CRIT("Malformed forwarding target syntax: %s", raw.toStdString().c_str());
    return nullptr;
  }

  OutputFormat fmt = parseOutputFormat(tokens[0]);
  if (fmt == OutputFormat::None) {
    CRIT("Forwarding target format is invalid: %s",
         tokens[0].toStdString().c_str());
    return nullptr;
  }

  if (tokens[1].isEmpty()) {
    CRIT("Forwarding target URL is empty");
    return nullptr;
  }

  QUrl url(tokens[1]);
  if (!url.isValid()) {
    CRIT("Forwarding target URL is invalid: %s",
         tokens[1].toStdString().c_str());
    return nullptr;
  }

  QString scheme = url.scheme().toLower();
  if (scheme != "tcp" && scheme != "udp") {
    CRIT("Forwarding target scheme is unsupported: %s",
         scheme.toStdString().c_str());
    return nullptr;
  }

  if (url.host().isEmpty()) {
    CRIT("Forwarding target URL is missing host");
    return nullptr;
  }

  if (url.port() == -1) {
    CRIT("Forwarding target URL is missing port");
    return nullptr;
  }

  return new ForwardTarget(url, fmt);
}
