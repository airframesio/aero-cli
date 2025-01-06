#ifndef NOTIFIER_H
#define NOTIFIER_H

#include <QObject>
#include <QSocketNotifier>

class EventNotifier : public QObject {
  Q_OBJECT

public:
  EventNotifier(QObject *parent = nullptr);
  ~EventNotifier();

  static void hupSignalHandler(int);
  static void intSignalHandler(int);
  static void termSignalHandler(int);

  static int setup();
  
private:
  static int sighupFd[2];
  static int sigintFd[2];
  static int sigtermFd[2];

  QSocketNotifier *snHup;
  QSocketNotifier *snInt;
  QSocketNotifier *snTerm;
    
public slots:
  void handleSigHup();
  void handleSigInt();
  void handleSigTerm();

signals:
  void hangup();
  void interrupt();
  void terminate();  
};

#endif
