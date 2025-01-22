#include <QDebug>
#include <QSocketNotifier>

#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

#include "notifier.h"

int EventNotifier::sighupFd[2];
int EventNotifier::sigintFd[2];
int EventNotifier::sigtermFd[2];

EventNotifier::EventNotifier(QObject *parent) : QObject(parent) {
  // TODO: consider what needs to be done here for Windows support
  
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sighupFd)) {
    qFatal("Failed to create socketpair for SIGHUP");
  }

  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigintFd)) {
    qFatal("Failed to created socketpair for SIGINT");
  }

  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigtermFd)) {
    qFatal("Failed to create socketpair for SIGTERM");
  }

  snHup = new QSocketNotifier(sighupFd[1], QSocketNotifier::Read, this);
  connect(snHup, SIGNAL(activated(QSocketDescriptor, QSocketNotifier::Type)), this, SLOT(handleSigHup()));
  
  snInt = new QSocketNotifier(sigintFd[1], QSocketNotifier::Read, this);
  connect(snInt, SIGNAL(activated(QSocketDescriptor, QSocketNotifier::Type)), this, SLOT(handleSigInt()));
  
  snTerm = new QSocketNotifier(sigtermFd[1], QSocketNotifier::Read, this);
  connect(snTerm, SIGNAL(activated(QSocketDescriptor, QSocketNotifier::Type)), this, SLOT(handleSigTerm()));
}

EventNotifier::~EventNotifier() {}

void EventNotifier::handleSigHup() {
  snHup->setEnabled(false);

  char c;
  std::ignore = ::read(sighupFd[1], &c, sizeof(c));

  emit hangup();
  snHup->setEnabled(true);
}

void EventNotifier::handleSigInt() {
  snInt->setEnabled(false);

  char c;
  std::ignore = ::read(sigintFd[1], &c, sizeof(c));

  emit interrupt();
  snInt->setEnabled(true);
}

void EventNotifier::handleSigTerm() {
  snTerm->setEnabled(false);

  char c;
  std::ignore = ::read(sigtermFd[1], &c, sizeof(c));

  emit terminate();
  snTerm->setEnabled(true);
}

void EventNotifier::hupSignalHandler(int) {
  char c = 1;
  std::ignore = ::write(sighupFd[0], &c, sizeof(c));
}

void EventNotifier::intSignalHandler(int) {
  char c = 1;
  std::ignore = ::write(sigintFd[0], &c, sizeof(c));
}

void EventNotifier::termSignalHandler(int) {
  char c = 1;
  std::ignore = ::write(sigtermFd[0], &c, sizeof(c));
}

int EventNotifier::setup() {
  struct sigaction hup, intr, term;

  hup.sa_handler = EventNotifier::hupSignalHandler;
  ::sigemptyset(&hup.sa_mask);
  hup.sa_flags = 0;
  hup.sa_flags |= SA_RESTART;

  if (::sigaction(SIGHUP, &hup, 0)) {
    return 1;
  }

  intr.sa_handler = EventNotifier::intSignalHandler;
  ::sigemptyset(&intr.sa_mask);
  intr.sa_flags = 0;
  intr.sa_flags |= SA_RESTART;

  if (::sigaction(SIGINT, &intr, 0)) {
    return 1;
  }

  
  term.sa_handler = EventNotifier::termSignalHandler;
  ::sigemptyset(&term.sa_mask);
  term.sa_flags = 0;
  term.sa_flags |= SA_RESTART;

  if (::sigaction(SIGTERM, &term, 0)) {
    return 1;
  }

  return 0;
}
