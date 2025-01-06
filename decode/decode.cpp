#include <zmq.h>

#include "decode.h"
#include "logger.h"

OutputFormat parseOutputFormat(const QString &raw) {
  QString norm = raw.toLower();

  if (norm == "text")
    return OutputFormat::Text;
  if (norm == "jaero")
    return OutputFormat::Jaero;
  if (norm == "jsondump")
    return OutputFormat::JsonDump;
  if (norm == "jsonaero")
    return OutputFormat::JsonAero;

  return OutputFormat::None;
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

  return new ForwardTarget(url, fmt);
}

Decoder::Decoder(const QString &publisher, const QString &topic,
                 const QString &format, const QString &rawForwarders,
                 bool disableReassembly, QObject *parent)
    : QObject(parent) {
  this->publisher = publisher;
  this->topic = topic;
  this->disableReassembly = disableReassembly;
  this->format = parseOutputFormat(format);

  if (this->format == OutputFormat::None) {
    FATAL("Invalid output format provided: %s", format.toStdString().c_str());
  }

  if (!rawForwarders.isEmpty()) {
    parseForwarder(rawForwarders);
  }
}

Decoder::~Decoder() {
  if (zmqSub != nullptr) {
    ::zmq_close(zmqSub);
  }

  if (zmqContext != nullptr) {
    ::zmq_ctx_destroy(zmqContext);
  }
}

void Decoder::run() {
  running = true;
  consumerThread = QtConcurrent::run([this] { publisherConsumer(); });
}

void Decoder::parseForwarder(const QString &raw) {
  for (const auto &rawTarget : raw.split(",")) {
    ForwardTarget *target = ForwardTarget::fromRaw(rawTarget);
    if (target == nullptr) {
      continue;
    }

    forwarders.append(*target);
    delete target;
  }
}

void Decoder::publisherConsumer() {
  while (running) {
    
  }
  
  emit completed();  
}

void Decoder::handleHup() {
  DBG("");

  // TODO:
}

void Decoder::handleInterrupt() {
  DBG("");
  running = false;
}

void Decoder::handleTerminate() {
  DBG("");
  running = false;
}
