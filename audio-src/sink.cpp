#include "sink.h"
#include "logger.h"

AudioSink::AudioSink(const QString &topic, const QAudioDevice device,
                     QObject *parent)
    : QIODevice(parent) {
  topicName = topic;
  targetDevice = device;

  targetFormat.setChannelCount(1);
  targetFormat.setSampleFormat(QAudioFormat::Int16);
  targetFormat.setSampleRate(48000);

  targetSource = new QAudioSource(targetDevice, targetFormat, this);
  targetSource->setBufferSize(48000 * 1.0);

  // TODO: init ZMQ settings
}

AudioSink::~AudioSink() {}

void AudioSink::begin() {
  DBG("Starting audio device read from %s",
      targetDevice.id().toStdString().c_str());

  // TODO: setup ZMQ
  
  open(QIODevice::WriteOnly);
  targetSource->start(this);
}

qint64 AudioSink::readData(char *data, qint64 maxlen) {
  Q_UNUSED(data);
  Q_UNUSED(maxlen);
  return 0;
}

qint64 AudioSink::writeData(const char *data, qint64 len) {
  DBG("Got %lld bytes", len);
  return len;
}

void AudioSink::end() {
  DBG("Ending audio device read");

  targetSource->stop();
  close();

  // TODO: end ZMQ
  
  emit completed();
}

void AudioSink::handleHup() {
  DBG("Got SIGHUP signal from EventNotifier");

  // TODO:
}

void AudioSink::handleInterrupt() {
  DBG("Got SIGINT signal from EventNotifier");

  end();
}

void AudioSink::handleTerminate() {
  DBG("Got SIGTERM signal from EventNotifier");

  end();
}
