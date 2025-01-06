#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QList>
#include <QTimer>
#include <SoapySDR/Logger.hpp>

#include "notifier.h"
#include "publisher.h"


int main(int argc, char *argv[]) {
  QCoreApplication core(argc, argv);
  QCoreApplication::setApplicationName("aero-publish");

  QCommandLineParser parser;
  parser.setApplicationDescription(
      "Publish INMARSAT Aero frequency chunks as VFOs over ZMQ");
  parser.addHelpOption();
  parser.addOption(QCommandLineOption(QStringList() << "d" << "device",
                                      "SoapySDR device string", "device"));
  parser.addOption(QCommandLineOption(QStringList() << "v" << "verbose",
                                      "Show verbose output"));
  parser.addOption(QCommandLineOption("enable-biast", "Enable Bias-T"));
  parser.addOption(QCommandLineOption("enable-dcc", "Enable DC correction"));
  parser.addPositionalArgument(
      "settings", "Path to SDRReceiver compliant satellite settings INI file");
  parser.process(core);

  if (parser.isSet("verbose")) {
    SoapySDR::setLogLevel(SOAPY_SDR_DEBUG);
  }

  bool enableBiast = parser.isSet("enable-biast");
  bool enableDcc = parser.isSet("enable-dcc");

  const QString deviceStr = parser.value("device");
  if (deviceStr.isEmpty()) {
    qCritical() << "[ERROR] -d/--device option must be provided, example: -d "
                   "driver=rtlsdr";
    return 1;
  }

  const QStringList args = parser.positionalArguments();
  if (args.isEmpty()) {
    qCritical() << "[ERROR] no settings path was provided, please provide a "
                   "SDRReceiver compliant settings INI file path";
    return 1;
  }

  EventNotifier notifier;
  Publisher publisher(deviceStr, enableBiast, enableDcc, args.at(0));

  QObject::connect(&notifier, SIGNAL(interrupt()), &publisher, SLOT(handleInterrupt()));
  QObject::connect(&publisher, SIGNAL(completed()), &core, SLOT(quit()));
  QTimer::singleShot(0, &publisher, SLOT(run()));

  EventNotifier::setup();
  
  return core.exec();
}
