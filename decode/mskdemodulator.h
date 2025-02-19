#ifndef MSKDEMODULATOR_H
#define MSKDEMODULATOR_H

#include "DSP.h"
#include <QObject>
#include <QVector>
#include <QPointer>
#include <QElapsedTimer>

class CoarseFreqEstimate;

class MskDemodulator : public QObject {
  Q_OBJECT
public:
  struct Settings {
    int coarsefreqest_fft_power;
    double freq_center;
    double lockingbw;
    double fb;
    double Fs;
    int symbolspercycle;
    double signalthreshold;
    bool zmqAudio;

    Settings() {
      coarsefreqest_fft_power = 13; // 2^coarsefreqest_fft_power
      freq_center = 1000;           // Hz
      lockingbw = 900;              // Hz
      fb = 600;                     // bps
      Fs = 48000;                   // Hz
      symbolspercycle = 16;
      signalthreshold = 0.5;
      zmqAudio = false;
    }
  };
  explicit MskDemodulator(QObject *parent);
  ~MskDemodulator();

  qint64 writeData(const char *data, qint64 len);
  void setSettings(Settings settings);
  void invalidatesettings();
  void setAFC(bool state);
  void setCPUReduce(bool state);
  double getCurrentFreq();

private:
  WaveTable mixer_center;
  WaveTable mixer2;
  WaveTable st_osc;

  int spectrumnfft, bbnfft;

  QVector<cpx_type> bbcycbuff;
  QVector<cpx_type> bbtmpbuff;
  int bbcycbuff_ptr;

  QVector<double> spectrumcycbuff;
  QVector<double> spectrumtmpbuff;
  int spectrumcycbuff_ptr;

  CoarseFreqEstimate *coarsefreqestimate;

  double Fs;
  double freq_center;
  double lockingbw;
  double fb;
  double signalthreshold;

  double SamplesPerSymbol;

  FIR *matchedfilter_re;
  FIR *matchedfilter_im;

  AGC *agc;

  MSKEbNoMeasure *ebnomeasure;

  MovingAverage *pointmean;

  int lastindex;

  QVector<cpx_type> pointbuff;
  int pointbuff_ptr;

  DiffDecode diffdecode;

  QVector<short> RxDataBits; // unpacked

  double mse;

  MovingAverage *msema;

  bool afc;

  QVector<cpx_type> singlepointphasevector;

  BaceConverter bc;

  QElapsedTimer timer;

  double ee;
  cpx_type pt_d;

  IIR st_iir_resonator;

  MovingAverage *marg;
  DelayThing<cpx_type> dt;

  DelayThing<cpx_type> delayedsmpl;
  Delay<double> delayt8;

  bool dcd;

  double correctionfactor;

  int coarseCounter;
  bool cpuReduce;

  Settings last_applied_settings;

signals:
  void ScatterPoints(const QVector<cpx_type> &buffer);
  void SymbolPhase(double phase_rad);
  void BBOverlapedBuffer(const QVector<cpx_type> &buffer);
  void OrgOverlapedBuffer(const QVector<double> &buffer);
  void Plottables(double freq_est, double freq_center, double bandwidth);
  void PeakVolume(double Volume);
  void processDemodulatedSoftBits(const QVector<short> &soft_bits);
  void RxData(const QByteArray &data); // packed in bytes
  void MSESignal(double mse);
  void SignalStatus(bool gotasignal);
  void WarningTextSignal(const QString &str);
  void EbNoMeasurmentSignal(double EbNo);
  void SampleRateChanged(double Fs);
  void BitRateChanged(double fb, bool burstmode);
public slots:
  void FreqOffsetEstimateSlot(double freq_offset_est);
  void CenterFreqChangedSlot(double freq_center);
  void DCDstatSlot(bool dcd);
  void dataReceived(const QByteArray &audio, quint32 sampleRate);
};

#endif // MSKDEMODULATOR_H
