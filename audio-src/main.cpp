#include <QAudioDevice>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QMediaDevices>
#include <QTimer>

#include "logger.h"
#include "notifier.h"
#include "sink.h"

int main(int argc, char *argv[]) {
  QCoreApplication core(argc, argv);
  QCoreApplication::setApplicationName("aero-audio-src");

  QCommandLineParser parser;
  parser.setApplicationDescription("Publish audio samples from audio input/output devices over ZMQ");
  parser.addHelpOption();
  parser.addOption(QCommandLineOption(QStringList() << "t" << "topic", "ZMQ topic name to publish", "topic"));
  parser.addOption(QCommandLineOption(QStringList() << "v" << "verbose",
                                      "Show verbose output"));
  parser.addOption(QCommandLineOption("list-devices", "List all available audio input and output device IDs"));
  parser.addPositionalArgument("device", "Audio device ID to sample and publish ZMQ");
  parser.process(core);

  if (parser.isSet("list-devices")) {
    INF("Available audio input devices:");
    
    quint32 devicesCount = 0;
    for (const QAudioDevice &device : QMediaDevices::audioInputs()) {
      INF("  %-16s", device.id().toStdString().c_str());
      devicesCount++;  
    }

    if (devicesCount == 0) {
      WARN("None");
    }

    INF("\nAvailable audio output devices:");
    
    devicesCount = 0;
    for (const QAudioDevice &device : QMediaDevices::audioOutputs()) {
      INF("  %-16s", device.id().toStdString().c_str());
      devicesCount++;
    }

    if (devicesCount == 0) {
      WARN("None");
    }
    
    return 0;  
  }
  
  if (parser.isSet("verbose")) {
    gMaxLogVerbosity = true;
  }

  const QStringList args = parser.positionalArguments();
  if (args.isEmpty()) {
    CRIT("Required audio device is not provided. Please provide an audio device ID (use --list-devices to see valid audio input/output devices)");
    return 1;
  }

  const QString topic = parser.value("topic");
  if (topic.isEmpty()) {
    CRIT("Missing ZMQ topic; please specify a ZMQ topic name to broadcast with");
    return 1;
  }
  
  if (topic.size() != 5) {
    CRIT("ZMQ topic should be 5 characters long, %s is not valid", topic.toStdString().c_str());
    return 1;  
  }
  
  const QString targetDeviceId = args.at(0);
  QAudioDevice targetDevice;
    
  for (const QAudioDevice &device : QMediaDevices::audioInputs()) {
    if (device.id().toStdString() == targetDeviceId.toStdString()) {
      targetDevice = device;
    }
  }

  for (const QAudioDevice &device : QMediaDevices::audioOutputs()) {
    if (device.id().toStdString() == targetDeviceId.toStdString()) {
      targetDevice = device;
    }
  }

  if (targetDevice.isNull()) {
    CRIT("Audio device is not valid: %s", targetDeviceId.toStdString().c_str());
    CRIT("For a list of valid audio device IDs, please use option --list-devices");
    return 1;
  }

  EventNotifier notifier;
  AudioSink sink(topic, targetDevice, nullptr);
  
  QObject::connect(&notifier, SIGNAL(hangup()), &sink, SLOT(handleHup()));
  QObject::connect(&notifier, SIGNAL(interrupt()), &sink,
                   SLOT(handleInterrupt()));
  QObject::connect(&notifier, SIGNAL(terminate()), &sink,
                   SLOT(handleTerminate()));
  QObject::connect(&sink, SIGNAL(completed()), &core, SLOT(quit()));
  QTimer::singleShot(0, &sink, SLOT(begin()));

  EventNotifier::setup();

  return core.exec();
}
