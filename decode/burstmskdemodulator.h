#ifndef BURSTMSKDEMODULATOR_H
#define BURSTMSKDEMODULATOR_H

#include "DSP.h"
#include <QObject>

#include <QVector>
#include <complex>

#include <QPointer>

#include <QElapsedTimer>

#include "fftrwrapper.h"

typedef FFTrWrapper<double> FFTr;
typedef std::complex<double> cpx_type;

class CoarseFreqEstimate;

class BurstMskDemodulator : public QObject {
  Q_OBJECT
public:
  enum ScatterPointType {
    SPT_constellation,
    SPT_phaseoffseterror,
    SPT_phaseoffsetest,
    SPT_None
  };
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
      lockingbw = 500;              // Hz
      fb = 125;                     // bps
      Fs = 8000;                    // Hz
      symbolspercycle = 16;
      signalthreshold = 0.6;
      zmqAudio = false;
    }
  };
  explicit BurstMskDemodulator(QObject *parent);
  ~BurstMskDemodulator();

  qint64 writeData(const char *data, qint64 len);
  void setSettings(Settings settings);
  void invalidatesettings();
  void setAFC(bool state);
  void setCPUReduce(bool state);
  void setScatterPointType(ScatterPointType type);
  double getCurrentFreq();

private:
  WaveTable mixer_center;
  WaveTable mixer2;

  int spectrumnfft, bbnfft;

  QVector<double> spectrumcycbuff;
  QVector<double> spectrumtmpbuff;
  int spectrumcycbuff_ptr;

  double Fs;
  double freq_center;
  double lockingbw;
  double fb;
  double symbolspercycle;
  double signalthreshold;

  double SamplesPerSymbol;

  FIR *matchedfilter_re;
  FIR *matchedfilter_im;

  AGC *agc;
  AGC *agc2;

  MSKEbNoMeasure *ebnomeasure;

  MovingAverage *pointmean;

  QVector<cpx_type> pointbuff;
  int pointbuff_ptr;

  QList<int> tixd;

  DiffDecode diffdecode;

  QVector<short> RxDataBits; // unpacked

  double mse;

  MovingAverage *msema;

  bool afc;

  int scatterpointtype;

  BaceConverter bc;

  QPointer<QIODevice> pdatasinkdevice;

  QElapsedTimer timer;

  // trident detector stuff

  // hilbert
  QJHilbertFilter hfir;
  QVector<cpx_type> hfirbuff;

  // delay lines
  Delay<cpx_type> bt_d1;
  Delay<double> bt_ma_diff;

  // MAs
  TMovingAverage<std::complex<double>> bt_ma1;
  MovingAverage *mav1;

  // Peak detection
  PeakDetector pdet;

  // delay for peak detection alignment
  DelayThing<std::complex<double>> d1;

  // delay for trident detection
  DelayThing<double> d2;

  // trident shape thing
  QVector<double> tridentbuffer;
  int tridentbuffer_ptr;
  int tridentbuffer_sz;

  int maxvalbin = 0;
  double trackfreq = 0;

  // fft for trident
  FFTr *fftr;
  QVector<cpx_type> out_base, out_top;
  QVector<double> out_abs_diff;
  QVector<double> in;
  double maxval;
  double vol_gain;
  int cntr;

  int startstopstart;
  int startstop;
  int numPoints;

  const cpx_type imag = cpx_type(0, 1);
  WaveTable st_osc;
  WaveTable st_osc_half;

  Delay<double> a1;

  double ee;
  cpx_type symboltone_averotator;
  cpx_type symboltone_rotator;
  double carrier_rotation_est;
  cpx_type pt_d;

  cpx_type rotator;
  double rotator_freq;

  // st
  IIR st_iir_resonator;

  Delay<double> delayt8;

  int endRotation;
  int startProcessing;
  bool dcd;

  DelayThing<cpx_type> delayedsmpl;

  bool cpuReduce;

signals:
  void ScatterPoints(const QVector<cpx_type> &buffer);
  void SymbolPhase(double phase_rad);
  void BBOverlapedBuffer(const QVector<cpx_type> &buffer);
  void OrgOverlapedBuffer(const QVector<double> &buffer);
  void Plottables(double freq_est, double freq_center, double bandwidth);
  void PeakVolume(double Volume);
  void RxData(QByteArray &data); // packed in bytes
  void MSESignal(double mse);
  void SignalStatus(bool gotasignal);
  void WarningTextSignal(const QString &str);
  void EbNoMeasurmentSignal(double EbNo);
  void SampleRateChanged(double Fs);
  void BitRateChanged(double fb, bool burstmode);
  void processDemodulatedSoftBits(const QVector<short> &soft_bits);

public slots:
  void CenterFreqChangedSlot(double freq_center);
  void DCDstatSlot(bool dcd);
  void dataReceived(const QByteArray &audio, quint32 sampleRate);
};

#endif // BURSTMSKDEMODULATOR_H
