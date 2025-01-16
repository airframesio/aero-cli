#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QHostInfo>
#include <QList>
#include <QTimer>
#include <qcommandlineoption.h>
#include <qhostinfo.h>

#include "decode.h"
#include "logger.h"
#include "notifier.h"

int main(int argc, char *argv[]) {
  QCoreApplication core(argc, argv);
  QCoreApplication::setApplicationName("aero-decode");

  QCommandLineParser parser;
  parser.setApplicationDescription(
      "Demodulate and decode VFOs over ZMQ from SDRReceiver or aero-publish "
      "into SatCom ACARS messages");
  parser.addHelpOption();
  parser.addOption(QCommandLineOption(
      QStringList() << "b" << "bit-rate",
      "Signal bit rate, valid rates: 600, 1200, 10500", "bit-rate"));
  parser.addOption(QCommandLineOption(
      QStringList() << "f" << "fwd",
      "Forward decoded ACARS messages to a list of servers and formats, see "
      "--format for allowable formats; example: FORMAT1=URL1,FORMAT2=URL2,...",
      "fwd"));
  parser.addOption(QCommandLineOption(
      QStringList() << "p" << "publisher",
      "URL of aero-publish or SDRReceiver publishing ZeroMQ server",
      "publisher"));
  parser.addOption(QCommandLineOption(QStringList() << "s" << "station-id",
                                      "Station ID for feeding", "station-id"));
  parser.addOption(QCommandLineOption(QStringList() << "t" << "topic",
                                      "ZeroMQ VFO topic name", "topic"));
  parser.addOption(QCommandLineOption(QStringList() << "v" << "verbose",
                                      "Show verbose output"));
  parser.addOption(QCommandLineOption("burst", "Enable burst mode (C-band)"));
  parser.addOption(
      QCommandLineOption("disable-reassembly", "Disable frame reassembly"));
  parser.addOption(
      QCommandLineOption("format",
                         "ACARS format type to display on console; valid: "
                         "jaero, jsondump, jsonaero, text (default)",
                         "format"));
  parser.process(core);

  if (parser.isSet("verbose")) {
    gMaxLogVerbosity = true;
  }

  const QString rawForwarders = parser.value("fwd");
  const QString publisher = parser.value("publisher");
  const QString topic = parser.value("topic");

  QString format = parser.value("format");
  QString station_id = parser.value("station-id");
  
  int bitRate = parser.value("bit-rate").toInt();

  bool burstMode = parser.isSet("burst");
  bool disableReassembly = parser.isSet("disable-reassembly");

  if (publisher.isEmpty()) {
    CRIT("Required publisher option is missing, example: -p "
         "tcp://127.0.0.1:6004");
    return 1;
  }

  if (station_id.isEmpty()) {
    station_id = QString("%1-AERO-INMARSAT").arg(QHostInfo::localHostName().toUpper());
    WARN("No station ID provided, using generated default %s", station_id.toStdString().c_str());
  }
  
  if (topic.isEmpty()) {
    CRIT("Required topic option is missing, example: -t VFO51");
    return 1;
  }

  if (format.isEmpty()) {
    format = "text";
  }

  EventNotifier notifier;
  Decoder decoder(station_id, publisher, topic, format, bitRate, burstMode,
                  rawForwarders, disableReassembly);

  QObject::connect(&notifier, SIGNAL(hangup()), &decoder, SLOT(handleHup()));
  QObject::connect(&notifier, SIGNAL(interrupt()), &decoder,
                   SLOT(handleInterrupt()));
  QObject::connect(&notifier, SIGNAL(terminate()), &decoder,
                   SLOT(handleTerminate()));
  QObject::connect(&decoder, SIGNAL(completed()), &core, SLOT(quit()));
  QTimer::singleShot(0, &decoder, SLOT(run()));

  EventNotifier::setup();

  return core.exec();
}
