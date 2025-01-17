#include <QByteArray>
#include <QHostAddress>
#include <QJsonDocument>
#include <QTcpSocket>
#include <QUdpSocket>
#include <zmq.h>

#include "aerol.h"
#include "decode.h"
#include "logger.h"
#include "mskdemodulator.h"
#include "oqpskdemodulator.h"
#include "output.h"

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
    : target(url), format(fmt) {
  QString scheme = target.scheme().toLower();
  if (scheme == "tcp") {
    conn = new QTcpSocket();
  } else {
    conn = new QUdpSocket();
  }

  reconnect();
}

ForwardTarget::~ForwardTarget() {
  if (conn != nullptr) {
    conn->deleteLater();
    conn = nullptr;
  }
}

void ForwardTarget::reconnect() {
  conn->connectToHost(QHostAddress(target.host()), target.port());
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

Decoder::Decoder(const QString &station_id, const QString &publisher,
                 const QString &topic, const QString &format, int bitRate,
                 bool burstMode, const QString &rawForwarders,
                 bool disableReassembly, QObject *parent)
    : QObject(parent) {
  this->publisher = publisher;
  this->station_id = station_id;
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
  // TODO: do we want to display ISU messages? if so, make sure to
  //       setDoNotDisplay to ignore spammy messages

  MskDemodulator::Settings mskSettings;
  mskSettings.zmqAudio = true;
  mskSettings.freq_center = 0;
  mskSettings.Fs = (this->bitRate == 600) ? 12000 : 24000;

  mskDemod = new MskDemodulator(this);
  mskDemod->setAFC(true);
  mskDemod->setCPUReduce(false);
  mskDemod->setSettings(mskSettings);

  OqpskDemodulator::Settings oqpskSettings;
  oqpskSettings.zmqAudio = true;
  oqpskSettings.freq_center = 0;

  oqpskDemod = new OqpskDemodulator(this);
  oqpskDemod->setAFC(true);
  oqpskDemod->setCPUReduce(false);
  oqpskDemod->setSettings(oqpskSettings);

  hunter = new SignalHunter(15, this);
  connect(hunter, SIGNAL(newFreqCenter(double)), this,
          SLOT(handleNewFreqCenter(double)));
  connect(hunter, SIGNAL(noSignalAfterScan()), this,
          SLOT(handleNoSignalAfterFullScan()));

  if (this->bitRate > 1200) {
    DBG("Connecting audioReceived signal to OQPSK demodulator");

    hunter->setParams(0, 25000, 10500);

    // TODO: support burst mode

    connect(this, SIGNAL(audioReceived(const QByteArray &, quint32)),
            oqpskDemod, SLOT(dataReceived(const QByteArray &, quint32)));
    connect(oqpskDemod,
            SIGNAL(processDemodulatedSoftBits(const QVector<short> &)), aerol,
            SLOT(processDemodulatedSoftBits(const QVector<short> &)));
    connect(oqpskDemod, SIGNAL(SignalStatus(bool)), hunter,
            SLOT(updatedSignalStatus(bool)));
    connect(hunter, SIGNAL(newFreqCenter(double)), oqpskDemod,
            SLOT(CenterFreqChangedSlot(double)));
  } else {
    DBG("Connecting audioReceived signal to MSK demodulator");

    hunter->setParams(0, 6000, 900);

    // TODO: support burst mode

    connect(this, SIGNAL(audioReceived(const QByteArray &, quint32)), mskDemod,
            SLOT(dataReceived(const QByteArray &, quint32)));
    connect(mskDemod,
            SIGNAL(processDemodulatedSoftBits(const QVector<short> &)), aerol,
            SLOT(processDemodulatedSoftBits(const QVector<short> &)));
    connect(mskDemod, SIGNAL(SignalStatus(bool)), hunter,
            SLOT(updatedSignalStatus(bool)));
    connect(hunter, SIGNAL(newFreqCenter(double)), mskDemod,
            SLOT(CenterFreqChangedSlot(double)));
  }

  connect(aerol, SIGNAL(DataCarrierDetect(bool)), hunter,
          SLOT(handleDcd(bool)));
  connect(hunter, SIGNAL(dcdChange(bool, bool)), this,
          SLOT(handleDcdChange(bool, bool)));

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
  for (auto target : forwarders) {
    if (target != nullptr) {
      delete target;
    }
  }

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

    forwarders.append(target);
  }
}

void Decoder::reconnectForwarder() {
  for (auto target : forwarders) {
    if (target != nullptr) {
      DBG("Reconnecting forwarder to %s",
          target->target.toDisplayString().toStdString().c_str());
      target->reconnect();
    }
  }
}

void Decoder::publisherConsumer() {
  int bufSize = 192000;
  int recvSize = 0;
  int status = 0;
  quint32 sampleRate = 48000;

  unsigned char rateBuf[4] = {0};
  char *samplesBuf = nullptr;

  const std::string publisherUrl = publisher.toStdString();
  const std::string topicName = topic.toStdString();
  int topicNameSize = ::strlen(topicName.c_str());

  if (!running)
    goto Exit;

  samplesBuf = (char *)::malloc(bufSize * sizeof(char));
  if (samplesBuf == nullptr) {
    CRIT("Failed to allocate samples buffer for ZeroMQ");
    goto Exit;
  }

  DBG("Connecting to ZMQ endpoint at %s", publisherUrl.c_str());

  status = ::zmq_connect(zmqSub, publisherUrl.c_str());
  if (status == -1) {
    CRIT("Failed to connect to publisher, error code: %d; is aero-publish or "
         "SDRReceiver running?",
         status);
    goto Exit;
  }

  DBG("Subscribing to ZMQ topic %s", topicName.c_str());

  status =
      ::zmq_setsockopt(zmqSub, ZMQ_SUBSCRIBE, topicName.c_str(), topicNameSize);
  if (status == -1) {
    CRIT("Failed to subscribe to %s; error code = %d",
         topic.toStdString().c_str(), zmq_errno());
    goto Exit;
  }

  DBG("Listening for samples...");

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

void Decoder::handleNoSignalAfterFullScan() {
  WARN("Scanned entire VFO bandwidth and could not find a signal.");

  if (noSignalExit) {
    WARN("Please confirm and verify that the specified topic is correct and "
         "that aero-publish is using correct settings")
    running = false;
    FATAL("Exiting because of no signal");
  }
}

void Decoder::handleDcdChange(bool old_state, bool new_state) {
  if (new_state) {
    DBG("Data carrier detected: no signal => signal");
  } else {
    DBG("Data carrier lost: signal => no signal");
  }
}

void Decoder::handleNewFreqCenter(double freq_center) {
  DBG("Trying frequency center %.2f in search of signal", freq_center);
}

void Decoder::handleACARS(ACARSItem &item) {
  QString *output = toOutputFormat(format, station_id, disableReassembly, item);
  if (output == nullptr) {
    CRIT("Failed to generate output format!");
    return;
  }

  INF("%s", output->toStdString().c_str());
  delete output;

  for (auto target : forwarders) {
    if (target != nullptr && target->conn != nullptr) {
      QString *output =
          toOutputFormat(target->format, station_id, disableReassembly, item);
      if (output != nullptr) {
        *output += "\n";
        target->conn->write(output->toLatin1());
        delete output;
      }
    }
  }
}

void Decoder::handleHup() {
  DBG("Got SIGHUP signal from EventNotifier");

  // TODO: do debug dump?

  reconnectForwarder();
}

void Decoder::handleInterrupt() {
  DBG("Got SIGINT signal from EventNotifier");
  running = false;
}

void Decoder::handleTerminate() {
  DBG("Got SIGTERM signal from EventNotifier");
  running = false;
}
