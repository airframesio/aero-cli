#include <qsocketnotifier.h>
#include <sys/socket.h>
#include <unistd.h>

#include "publisher.h"

Publisher::Publisher(bool enableBiast, bool enableDcc,
                     const QString &settingsPath, QObject *parent)
    : QObject(parent) {
  this->enableBiast = enableBiast;
  this->enableDcc = enableDcc;

  if (!this->loadSettings(settingsPath)) {
    qFatal("[ERROR] failed to parse and load settings");
  }
}

Publisher::~Publisher() {}

bool Publisher::loadSettings(const QString &settingsPath) { return true; }

void Publisher::run() { emit completed(); }
