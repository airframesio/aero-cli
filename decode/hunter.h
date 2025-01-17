#ifndef HUNTER_H
#define HUNTER_H

#include <QObject>

class SignalHunter : public QObject {
  Q_OBJECT
  
public:
  SignalHunter(quint32 maxTries, QObject *parent = nullptr);
  ~SignalHunter();

  void setParams(quint32 minFreq, quint32 maxFreq, quint32 bandwidth) {
    this->minFreq = minFreq;
    this->maxFreq = maxFreq;
    this->bandwidth = bandwidth;
  }
  
public slots:
  void updatedSignalStatus(bool gotasignal);
  void handleDcd(bool dcd);
     
private:
  bool lastDcd;
  
  quint32 maxTries;
  quint32 fullScans;
  
  quint32 minFreq;
  quint32 maxFreq;
  quint32 bandwidth;
  
  quint32 iterationsSinceSignal;
  
signals:
  void newFreqCenter(double freq_center);
  void noSignalAfterScan();
  void dcdChange(bool old_dcd, bool new_dcd);
};

#endif
