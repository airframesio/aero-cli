#include "hunter.h"

SignalHunter::SignalHunter(quint32 maxTries, QObject *parent)
    : QObject(parent) {
  this->maxTries = maxTries;
  this->fullScans = 0;
  this->iterationsSinceSignal = 0;
}

SignalHunter::~SignalHunter() {}

void SignalHunter::updatedSignalStatus(bool gotasignal) {
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
