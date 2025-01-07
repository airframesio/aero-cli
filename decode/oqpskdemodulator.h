#ifndef OQPSKDEMODULATOR_H
#define OQPSKDEMODULATOR_H

#include "DSP.h"
#include "coarsefreqestimate.h"
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QVector>
#include <assert.h>

class OqpskDemodulator : public QObject {
  Q_OBJECT
public:
  struct Settings {
    int coarsefreqest_fft_power;
    double freq_center;
    double lockingbw;
    double fb;
    double Fs;
    double signalthreshold;
    bool zmqAudio;

    Settings() {
      coarsefreqest_fft_power = 14; // 13;//2^coarsefreqest_fft_power
      freq_center = 8000;           // Hz
      lockingbw = 10500;            // Hz
      fb = 10500;                   // bps
      Fs = 48000;                   // Hz
      signalthreshold = 0.65;       // 0.6;
      zmqAudio = false;
    }
  };
  explicit OqpskDemodulator(QObject *parent);
  ~OqpskDemodulator();
  void setAFC(bool state);
  void setCPUReduce(bool state);
  void setSettings(Settings settings);
  void invalidatesettings();
  qint64 writeData(const char *data, qint64 len);
  double getCurrentFreq();
signals:
  void ScatterPoints(const QVector<cpx_type> &buffer);
  void OrgOverlapedBuffer(const QVector<double> &buffer);
  void PeakVolume(double Volume);
  void SampleRateChanged(double Fs);
  void BitRateChanged(double fb, bool burstmode);
  void Plottables(double freq_est, double freq_center, double bandwidth);
  void BBOverlapedBuffer(const QVector<cpx_type> &buffer);
  void MSESignal(double mse);
  void SignalStatus(bool gotasignal);
  void WarningTextSignal(const QString &str);
  void EbNoMeasurmentSignal(double EbNo);
  void processDemodulatedSoftBits(const QVector<short> &soft_bits);

private:
  bool afc;

  QVector<double> spectrumcycbuff;
  int spectrumcycbuff_ptr;
  int spectrumnfft;

  QVector<cpx_type> bbcycbuff;
  QVector<cpx_type> bbtmpbuff;
  int bbcycbuff_ptr;
  int bbnfft;

  QVector<cpx_type> pointbuff;
  int pointbuff_ptr;

  QElapsedTimer timer;

  double Fs;
  double freq_center;
  double lockingbw;
  double fb;
  double signalthreshold;

  double SamplesPerSymbol;

  FIR *fir_re;
  FIR *fir_im;

  // st
  Delay<double> delays;
  Delay<double> delayt41;
  Delay<double> delayt42;
  Delay<double> delayt8;
  IIR st_iir_resonator;
  WaveTable st_osc;
  WaveTable st_osc_ref;

  // ct
  IIR ct_iir_loopfilter;

  WaveTable mixer_center;
  WaveTable mixer2;

  CoarseFreqEstimate *coarsefreqestimate;

  double mse;
  MSEcalc *msecalc;

  QVector<cpx_type> phasepointbuff;
  int phasepointbuff_ptr;

  AGC *agc;

  OQPSKEbNoMeasure *ebnomeasure;

  BaceConverter bc;
  QByteArray RxDataBytes;    // packed in bytes
  QVector<short> RxDataBits; // unpacked

  MovingAverage *marg;
  DelayThing<cpx_type> dt;

  double ee;

  bool dcd;
  JFastFir fir_pre;
  WaveTable mixer_fir_pre;

  int coarseCounter;
  bool cpuReduce;

public slots:
  void FreqOffsetEstimateSlot(double freq_offset_est);
  void CenterFreqChangedSlot(double freq_center);
  void DCDstatSlot(bool _dcd);
  void dataReceived(const QByteArray &audio, quint32 sampleRate);
};

#endif // OQPSKDEMODULATOR_H
