#include <zmq.h>

#include "aerol.h"
#include "decode.h"
#include "logger.h"
#include "mskdemodulator.h"
#include "oqpskdemodulator.h"

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
                 const QString &format, int bitRate, bool burstMode,
                 const QString &rawForwarders, bool disableReassembly,
                 QObject *parent)
    : QObject(parent) {
  this->publisher = publisher;
  this->topic = topic;
  this->bitRate = bitRate;
  this->burstMode = burstMode;
  this->disableReassembly = disableReassembly;
  this->format = parseOutputFormat(format);

  running = false;

  if (!validBitRates.contains(this->bitRate)) {
    CRIT("Unsupported bit rate: %d", this->bitRate);
    return;
  }

  if (this->format == OutputFormat::None) {
    CRIT("Invalid output format provided: %s", format.toStdString().c_str());
    return;
  }

  if (!rawForwarders.isEmpty()) {
    parseForwarder(rawForwarders);
  }

  zmqContext = ::zmq_ctx_new();
  if (zmqContext == nullptr) {
    CRIT("Failed to create new ZeroMQ context, error code = %d", zmq_errno());
    return;
  }

  zmqSub = ::zmq_socket(zmqContext, ZMQ_SUB);
  if (zmqSub == nullptr) {
    CRIT("Failed to create ZeroMQ socket, error code = %d", zmq_errno());
    return;
  }

  aerol = new AeroL(this);
  aerol->setBitRate(this->bitRate);
  aerol->setBurstmode(this->burstMode);
  // TODO: setDoNotDisplay?

  MskDemodulator::Settings mskSettings;
  mskSettings.freq_center = 0;

  mskDemod = new MskDemodulator(this);
  mskDemod->setAFC(true);
  mskDemod->setCPUReduce(false);
  mskDemod->setSettings(mskSettings);

  OqpskDemodulator::Settings oqpskSettings;
  // oqpskSettings.freq_center = 0;

  oqpskDemod = new OqpskDemodulator(this);
  oqpskDemod->setAFC(true);
  oqpskDemod->setCPUReduce(false);
  oqpskDemod->setSettings(oqpskSettings);

  if (this->bitRate > 1200) {
    DBG("Connecting audioReceived signal to OQPSK demodulator");

    // TODO: implement signal hunter
    connect(this, SIGNAL(audioReceived(const QByteArray &, quint32)),
            oqpskDemod, SLOT(dataReceived(const QByteArray &, quint32)));
    connect(oqpskDemod,
            SIGNAL(processDemodulatedSoftBits(const QVector<short> &)), aerol,
            SLOT(processDemodulatedSoftBits(const QVector<short> &)));
  } else {
    DBG("Connecting audioReceived signal to MSK demodulator");

    // TODO: implement signal hunter
    connect(this, SIGNAL(audioReceived(const QByteArray &, quint32)), mskDemod,
            SLOT(dataReceived(const QByteArray &, quint32)));
    connect(mskDemod,
            SIGNAL(processDemodulatedSoftBits(const QVector<short> &)), aerol,
            SLOT(processDemodulatedSoftBits(const QVector<short> &)));
  }

  if (this->disableReassembly) {
    DBG("Reassembly disabled, connecting to ACARSfragmentsignal signal");
    connect(aerol, SIGNAL(ACARSfragmentsignal(ACARSItem &)), this,
            SLOT(handleACARS(ACARSItem &)));
  } else {
    DBG("Reassembly enabled, connecting to ACARSsignal signal");
    connect(aerol, SIGNAL(ACARSsignal(ACARSItem &)), this,
            SLOT(handleACARS(ACARSItem &)));
  }

  running = true;
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
  DBG("Starting concurrent publishing consumer thread");
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
  int bufSize = 192000;
  int recvSize = 0;
  int status = 0;
  quint32 sampleRate = 48000;

  unsigned char rateBuf[4] = {0};
  char *samplesBuf = nullptr;

  if (!running)
    goto Exit;

  samplesBuf = (char *)::malloc(bufSize * sizeof(char));
  if (samplesBuf == nullptr) {
    CRIT("Failed to allocate samples buffer for ZeroMQ");
    goto Exit;
  }

  status = ::zmq_connect(zmqSub, publisher.toStdString().c_str());
  if (status == -1) {
    CRIT("Failed to connect to publisher, error code: %d; is aero-publish or "
         "SDRReceiver running?",
         status);
    goto Exit;
  }

  status =
      ::zmq_setsockopt(zmqSub, ZMQ_SUBSCRIBE, topic.toStdString().c_str(), 5);
  if (status == -1) {
    CRIT("Failed to subscribe to %s; error code = %d",
         topic.toStdString().c_str(), zmq_errno());
    goto Exit;
  }

  while (running) {
    while (((recvSize = ::zmq_recv(zmqSub, nullptr, 0, ZMQ_DONTWAIT)) < 0) &&
           running) {
      ::usleep(10000);
    }

    if (!running)
      break;

    recvSize = ::zmq_recv(zmqSub, rateBuf, sizeof(rateBuf), ZMQ_DONTWAIT);
    if (recvSize != sizeof(rateBuf)) {
      continue;
    }

    if (!running)
      break;

    ::memcpy(&sampleRate, rateBuf, sizeof(sampleRate));

    recvSize = ::zmq_recv(zmqSub, samplesBuf, bufSize, ZMQ_DONTWAIT);
    if (!running)
      break;

    if (recvSize >= 0) {
      QByteArray qdata(samplesBuf, recvSize);
      emit audioReceived(qdata, sampleRate);
    }
  }

Exit:
  if (samplesBuf != nullptr) {
    ::free(samplesBuf);
  }

  emit completed();
}

void Decoder::handleACARS(ACARSItem &item) {
  auto label = item.LABEL;
  if (label[1] == (char)127)
    label[1] = '?';

  INF("[%7s] ACK=%s BLK=%c C=%d LBL=%2s %s%s",
      item.PLANEREG.toStdString().c_str(),
      std::string({item.TAK ? (char)'?' : (char)item.TAK}).c_str(), item.BI,
      item.moretocome ? 1 : 0, label.toStdString().c_str(),
      item.message.isEmpty() ? "" : "MSG=",
      item.message.remove("\n").remove("\r").toStdString().c_str());
}

void Decoder::handleHup() {
  DBG("Got SIGHUP signal from EventNotifier");

  // TODO: do debug dump?
}

void Decoder::handleInterrupt() {
  DBG("Got SIGINT signal from EventNotifier");
  running = false;
}

void Decoder::handleTerminate() {
  DBG("Got SIGTERM signal from EventNotifier");
  running = false;
}
