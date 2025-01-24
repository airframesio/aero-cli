#include "hunter.h"

SignalHunter::SignalHunter(quint32 maxTries, QObject *parent)
    : QObject(parent) {
  this->maxTries = maxTries;
  this->fullScans = 0;
  this->iterationsSinceSignal = 0;
  this->lastDcd = false;
  this->enabled = true;
}

SignalHunter::~SignalHunter() {}

void SignalHunter::handleDcd(bool dcd) {
  if (dcd != lastDcd) {
    emit dcdChange(lastDcd, dcd);
    lastDcd = dcd;
  }  
}

void SignalHunter::updatedSignalStatus(bool gotasignal) {
  if (!enabled) return;
  
  if (gotasignal) {
    iterationsSinceSignal = 0;
  } else {
    iterationsSinceSignal++;

    if (iterationsSinceSignal > 0 && iterationsSinceSignal % maxTries == 0) {
      double new_freq_center = minFreq + (bandwidth >> 1) * (int)(iterationsSinceSignal / maxTries);
      if (new_freq_center > maxFreq - (bandwidth >> 1)) {
        new_freq_center = 0.0;
        iterationsSinceSignal = 0;
        fullScans++;

        emit noSignalAfterScan();
      }
      
      emit newFreqCenter(new_freq_center);
    }
  }
}
