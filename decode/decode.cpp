#include <QByteArray>
#include <QHostAddress>
#include <QJsonDocument>
#include <QTcpSocket>
#include <QUdpSocket>
#include <zmq.h>

#include "decode.h"
#include "logger.h"
#include "output.h"

Decoder::Decoder(const QString &station_id, const QString &publisher,
                 const QString &topic, const QString &format, int bitRate,
                 bool burstMode, const QString &rawForwarders,
                 bool disableReassembly, QObject *parent)
    : QObject(parent) {
  this->publisher = publisher;
  this->stationId = station_id;
  this->topic = topic;
  this->bitRate = bitRate;
  this->burstMode = burstMode;
  this->disableReassembly = disableReassembly;
  this->format = parseOutputFormat(format);

  running.storeRelease(0);

  if (!validBitRates.contains(this->bitRate)) {
    CRIT("Unsupported bit rate: %d", this->bitRate);
    return;
  }

  if (this->format == OutputFormat::None) {
    CRIT("Invalid output format provided: %s", format.toStdString().c_str());
    return;
  }

  if (!rawForwarders.isEmpty()) {
    if (!parseForwarder(rawForwarders)) {
      CRIT("Some forwarders configuration may be malformed: %s",
           rawForwarders.toStdString().c_str());
      return;
    }
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

  BurstMskDemodulator::Settings burstMskSettings;
  burstMskSettings.zmqAudio = true;
  burstMskSettings.Fs = 48000;

  burstMskDemod = new BurstMskDemodulator(this);
  burstMskDemod->setAFC(true);
  burstMskDemod->setCPUReduce(false);
  burstMskDemod->setSettings(burstMskSettings);

  BurstOqpskDemodulator::Settings burstOqpskSettings;
  burstOqpskSettings.zmqAudio = true;

  burstOqpskDemod = new BurstOqpskDemodulator(this);
  burstOqpskDemod->setAFC(true);
  burstOqpskDemod->setCPUReduce(false);
  burstOqpskDemod->setSettings(burstOqpskSettings);

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
    hunter->setParams(0, 25000, 10500);

    if (burstMode) {
      DBG("Connecting audioReceived signal to burst OQPSK demodulator");

      connect(this, SIGNAL(audioReceived(const QByteArray &, quint32)),
              burstOqpskDemod, SLOT(dataReceived(const QByteArray &, quint32)));
      connect(burstOqpskDemod,
              SIGNAL(processDemodulatedSoftBits(const QVector<short> &)), aerol,
              SLOT(processDemodulatedSoftBits(const QVector<short> &)));
      connect(burstOqpskDemod, SIGNAL(SignalStatus(bool)), hunter,
              SLOT(updatesSignalStatus(bool)));
      connect(hunter, SIGNAL(newFreqCenter(double)), burstOqpskDemod,
              SLOT(CenterFreqChangedSlot(double)));
    } else {
      DBG("Connecting audioReceived signal to OQPSK demodulator");

      connect(this, SIGNAL(audioReceived(const QByteArray &, quint32)),
              oqpskDemod, SLOT(dataReceived(const QByteArray &, quint32)));
      connect(oqpskDemod,
              SIGNAL(processDemodulatedSoftBits(const QVector<short> &)), aerol,
              SLOT(processDemodulatedSoftBits(const QVector<short> &)));
      connect(oqpskDemod, SIGNAL(SignalStatus(bool)), hunter,
              SLOT(updatedSignalStatus(bool)));
      connect(hunter, SIGNAL(newFreqCenter(double)), oqpskDemod,
              SLOT(CenterFreqChangedSlot(double)));
    }
  } else {
    hunter->setParams(0, 6000, 900);

    if (burstMode) {
      DBG("Connecting audioReceived signal to burst MSK demodulator");

      connect(this, SIGNAL(audioReceived(const QByteArrau &, quint32)),
              burstMskDemod, SLOT(dataReceived(const QByteArray &, quint32)));
      connect(burstMskDemod,
              SIGNAL(processDemodulatedSoftBits(const QVector<short> &)), aerol,
              SLOT(processDemodulatedSoftBits(const QVector<short> &)));
      connect(burstMskDemod, SIGNAL(SignalStatus(bool)), hunter,
              SLOT(updateSingalStatus(bool)));
      connect(hunter, SIGNAL(newFreqCenter(double)), burstMskDemod,
              SLOT(CenterFreqChangedSlot(double)));
    } else {
      DBG("Connecting audioReceived signal to MSK demodulator");

      connect(this, SIGNAL(audioReceived(const QByteArray &, quint32)),
              mskDemod, SLOT(dataReceived(const QByteArray &, quint32)));
      connect(mskDemod,
              SIGNAL(processDemodulatedSoftBits(const QVector<short> &)), aerol,
              SLOT(processDemodulatedSoftBits(const QVector<short> &)));
      connect(mskDemod, SIGNAL(SignalStatus(bool)), hunter,
              SLOT(updatedSignalStatus(bool)));
      connect(hunter, SIGNAL(newFreqCenter(double)), mskDemod,
              SLOT(CenterFreqChangedSlot(double)));
    }
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

  running.storeRelease(1);
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

  DBG("Starting concurrent forwarder consumer thread");
  forwarderThread = QtConcurrent::run([this] { forwarderConsumer(); });
}

bool Decoder::parseForwarder(const QString &raw) {
  for (const auto &rawTarget : raw.split(",")) {
    ForwardTarget *target = ForwardTarget::fromRaw(rawTarget);
    if (target == nullptr) {
      return false;
    }

    forwarders.append(target);
  }

  return true;
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

  if (!running.loadAcquire())
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

  while (running.loadAcquire()) {
    while (((recvSize = ::zmq_recv(zmqSub, nullptr, 0, ZMQ_DONTWAIT)) < 0) &&
           running.loadAcquire()) {
      ::usleep(10000);
    }

    if (!running.loadAcquire())
      break;

    recvSize = ::zmq_recv(zmqSub, rateBuf, sizeof(rateBuf), ZMQ_DONTWAIT);
    if (recvSize != sizeof(rateBuf)) {
      continue;
    }

    if (!running.loadAcquire())
      break;

    ::memcpy(&sampleRate, rateBuf, sizeof(sampleRate));

    recvSize = ::zmq_recv(zmqSub, samplesBuf, bufSize, ZMQ_DONTWAIT);
    if (!running.loadAcquire())
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

  DBG("Waiting for forwarder consumer to get the hint to exit");
  forwarderThread.waitForFinished();

  DBG("Forwarder consumer exited");
  emit completed();
}

void Decoder::forwarderConsumer() {
  for (auto target : forwarders) {
    if (target != nullptr) {
      target->reconnect();
    }
  }

  while (running.loadAcquire()) {
    sendBufferRwLock.lockForRead();
    if (sendBuffer.isEmpty()) {
      DBG("No items in sendBuffer, forwarder consumer waiting a second for "
          "next item add");
      sendBufferCondition.wait(&sendBufferRwLock,
                               QDeadlineTimer(MAX_CONNECTION_WAIT_MS));
      if (!running.loadAcquire()) {
        sendBufferRwLock.unlock();
        break;
      }

      if (sendBuffer.isEmpty()) {
        DBG("Still nothing, trying for another wait");
        sendBufferRwLock.unlock();
        continue;
      }
    }

    DBG("sendBuffer is populated with %lld items for processing",
        sendBuffer.size());

    ACARSItem item = sendBuffer.front();
    sendBuffer.pop_front();
    sendBufferRwLock.unlock();

    for (auto target : forwarders) {
      if (target != nullptr) {
        QString *out = toOutputFormat(target->getFormat(), stationId,
                                      disableReassembly, item);
        if (out != nullptr) {
          *out += "\n";

          target->send(out->toLatin1());
          delete out;
        }
      }
    }
  }
}

void Decoder::handleNoSignalAfterFullScan() {
  WARN("Scanned entire VFO bandwidth and could not find a signal.");

  if (noSignalExit) {
    WARN("Please confirm and verify that the specified topic is correct and "
         "that aero-publish is using correct settings")
    running.storeRelease(0);
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
  DBG("Trying frequency center %.1f in search of signal", freq_center);
}

void Decoder::handleACARS(ACARSItem &item) {
  QString *output = toOutputFormat(format, stationId, disableReassembly, item);
  if (output == nullptr) {
    CRIT("Failed to generate output format!");
    return;
  }

  INF("%s", output->toStdString().c_str());
  delete output;

  sendBufferRwLock.lockForWrite();
  sendBuffer.push_back(item);
  sendBufferCondition.wakeAll();
  sendBufferRwLock.unlock();
}

void Decoder::handleHup() {
  DBG("Got SIGHUP signal from EventNotifier");

  // TODO: do debug dump?
}

void Decoder::handleInterrupt() {
  DBG("Got SIGINT signal from EventNotifier");
  running.storeRelease(0);
}

void Decoder::handleTerminate() {
  DBG("Got SIGTERM signal from EventNotifier");
  running.storeRelease(0);
}
