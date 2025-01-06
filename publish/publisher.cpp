#include <QFileInfo>
#include <QSettings>
#include <SoapySDR/Constants.h>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>
#include <qglobal.h>
#include <sys/socket.h>
#include <unistd.h>

#include "logger.h"
#include "publisher.h"

Publisher::Publisher(const QString &deviceStr, bool enableBiast, bool enableDcc,
                     const QString &settingsPath, QObject *parent)
    : QObject(parent) {
  this->enableBiast = enableBiast;
  this->enableDcc = enableDcc;

  if (!loadSettings(settingsPath)) {
    FATAL("[ERROR] failed to parse and load settings");
  }

  device = SoapySDR::Device::make(deviceStr.toStdString());
  if (device == nullptr) {
    FATAL("[ERROR] failed to find device: %s", deviceStr.toStdString().c_str());
  }

  device->setGainMode(SOAPY_SDR_RX, 0, 1);
  device->setGain(SOAPY_SDR_RX, 0, tuner_gain);
  device->setFrequency(SOAPY_SDR_RX, 0, center_frequency);
  device->setSampleRate(SOAPY_SDR_RX, 0, Fs);
  device->setDCOffsetMode(SOAPY_SDR_RX, 0, enableDcc);
  device->writeSetting("biastee", enableBiast ? "true" : "false");
}

Publisher::~Publisher() {
  if (stream != nullptr) {
    device->deactivateStream(stream);
    device->closeStream(stream);
  }

  if (device != nullptr) {
    device->writeSetting("biastee", "false");
    SoapySDR::Device::unmake(device);
  }
}

bool Publisher::loadSettings(const QString &settingsPath) {
  QFileInfo info(settingsPath);
  if (!info.exists() || !info.isFile()) {
    CRIT("Provided settings file path either doesn't exist or isn't a file: %s",
         settingsPath.toStdString().c_str());
    return false;
  }

  QSettings settings(settingsPath, QSettings::IniFormat);

  Fs = settings.value("sample_rate").toInt();
  if (Fs == 0) {
    CRIT("Provided sample rate in settings file either doesn't exist or isn't "
         "an integer");
    return false;
  }

  if (!validSampleRates.contains(Fs)) {
    CRIT("Provided sample rate is not supported: %d", Fs);
    return false;
  }

  center_frequency = settings.value("center_frequency").toInt();

  QStringList vfo_str;

  center_frequency = settings.value("center_frequency").toInt();

  QString auto_start_tuner_serial =
      settings.value("auto_start_tuner_serial").toString();
  tuner_idx = settings.value("auto_start_tuner_idx").toInt();
  enableBiast =
      enableBiast || (settings.value("auto_start_biast").toInt() == 1);

  int gain = settings.value("tuner_gain").toInt();
  int remote_gain_idx = settings.value("remote_rtl_gain_idx").toInt();

  int mix_offset = settings.value("mix_offset").toInt();

  // usually 4 buffers per Fs but in some cases 5 due to multiple of 512
  int bufsplit = 4;

  if (double((int((2 * Fs) / 4)) % 512) > 0) {
    buflen = int((2 * Fs) / 5);
    bufsplit = 5;
  } else {
    buflen = int((2 * Fs) / 4);
  }

  if (gain > 0) {
    tuner_gain = gain;
  }
  if (remote_gain_idx > 0) {
    tuner_gain_idx = remote_gain_idx;
  }

  QString zmq_address = settings.value("zmq_address").toString();

  this->enableDcc =
      settings.value("correct_dc_bias").toString() == "1" ? true : false;

  int msize = settings.beginReadArray("main_vfos");

  // Read main vfos
  for (int i = 0; i < msize; ++i) {
    settings.setArrayIndex(i);

    vfo *pVFO = new vfo();
    int vfo_freq = settings.value("frequency").toInt();
    int vfo_out_rate = settings.value("out_rate").toInt();

    QString output_connect = settings.value("zmq_address").toString();
    QString out_topic = settings.value("zmq_topic").toString();

    int compscale = settings.value("compress_scale").toInt();

    if (compscale > 0) {
      pVFO->setScaleComp(compscale);
    }

    if (output_connect != "" && out_topic != "") {
      pVFO->setZmqAddress(output_connect);
      pVFO->setZmqTopic(out_topic);
    }

    pVFO->setFs(Fs);
    pVFO->setDecimationCount(
        Fs / vfo_out_rate == 1 ? 0 : int(log2(Fs / vfo_out_rate)));
    pVFO->setMixerFreq(center_frequency - vfo_freq);
    pVFO->setDemodUSB(false);
    pVFO->setCompressonStyle(1);
    pVFO->init(buflen / 2, false);
    pVFO->setVFOs(&VFOsub[i]);
    VFOmain.push_back(pVFO);
  }

  settings.endArray();
  int size = settings.beginReadArray("vfos");
  nVFO = size;
  vfo_str.push_back("Main");

  // read regular vfos
  for (int i = 0; i < size; ++i) {
    settings.setArrayIndex(i);

    vfo *pVFO = new vfo();
    int vfo_freq = settings.value("frequency").toInt() + mix_offset;
    int data_rate = settings.value("data_rate").toInt();
    int out_rate = settings.value("out_rate").toInt();

    if (out_rate == 0 && data_rate > 0) {
      switch (data_rate) {
      case 600:
        out_rate = 12000;
        break;
      case 1200:
        out_rate = 24000;
        break;
      default:
        out_rate = 48000;
        break;
      }
    }

    int filterbw = settings.value("filter_bandwidth").toInt();
    int main_vfo_freq = 0;
    int main_vfo_out_rate = Fs;
    int main_idx = 0;

    // find main VFO
    for (int a = 0; a < VFOmain.length(); a++) {
      int diff = std::abs((center_frequency - VFOmain.at(a)->getMixerFreq()) -
                          vfo_freq);
      if (diff < VFOmain.at(a)->getOutRate() && !VFOmain.at(a)->getDemodUSB()) {
        main_idx = a;
        main_vfo_freq = VFOmain.at(a)->getMixerFreq();
        main_vfo_out_rate = VFOmain.at(a)->getOutRate();
        break;
      }
    }

    pVFO->setZmqTopic(settings.value("topic").toString());
    pVFO->setZmqAddress(zmq_address);

    int lateDecimate = 0;
    if ((main_vfo_out_rate / 48000) == 5) {
      pVFO->setDecimationCount(int(log2(main_vfo_out_rate / (5 * out_rate))));
      lateDecimate = 5;
    } else if ((main_vfo_out_rate / 48000) == 6) {
      pVFO->setDecimationCount(int(log2(main_vfo_out_rate / (6 * out_rate))));
      lateDecimate = 6;
    }

    else {
      pVFO->setDecimationCount(int(log2(Fs / out_rate)) -
                               int(log2(Fs / main_vfo_out_rate)));
    }

    pVFO->setFilterBandwidth(filterbw);
    pVFO->setGain((float)settings.value("gain").toFloat() / 100);
    pVFO->setMixerFreq((center_frequency - main_vfo_freq) - vfo_freq);
    pVFO->setFs(main_vfo_out_rate);
    pVFO->setCompressonStyle(1);
    pVFO->init(main_vfo_out_rate / bufsplit, true, lateDecimate);

    VFOsub[main_idx].push_back(pVFO);

    vfo_str.push_back(settings.value("topic").toString());
  }

  settings.endArray();

  return true;
}

void Publisher::run() {
  running = true;
  mainReader = QtConcurrent::run([this] { return readerThread(); });
}

void Publisher::readerThread() {
  int flags = 0;
  int samplesRead = 0;
  long long timeNs = 0;
  float *samplesBuf = (float *) ::malloc(buflen * sizeof(float));

  void *sampleBuffers[] = {samplesBuf};
  SoapySDR::Kwargs streamArgs{{"buffers", std::to_string(24)},
                              {"bufflen", std::to_string(buflen)}};

  if (samplesBuf == nullptr) {
    CRIT("Memory allocation failed for samplesBuf: out of memory when "
         "attempting to allocate %lu bytes",
         buflen * sizeof(float));
    goto Exit;
  }

  stream = device->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32,
                               std::vector<size_t>(), streamArgs);
  if (stream == nullptr) {
    CRIT("SoapySDR could not setup stream");
    goto Exit;
  }

  if (device->activateStream(stream)) {
    CRIT("SoapySDR could not activate stream");
    goto Exit;
  }

  while (running) {
    samplesRead =
        device->readStream(stream, sampleBuffers, buflen, flags, timeNs, 1e7);
    if (samplesRead <= 0) {
      CRIT("SoapySDR could not read stream from SDR");
      break;
    }

    demodData(samplesBuf, samplesRead << 1);
  }

Exit:
  if (samplesBuf != nullptr) {
    ::free(samplesBuf);
  }

  emit completed();
}

void Publisher::demodData(const float *data, int len) {
  demodSamples.resize(len / 2);

  for (int i = 0; i < len / 2; ++i) {
    // convert to complex float
    cpx_typef curr = cpx_typef(data[2 * i], data[2 * i + 1]);

    if (enableDcc) {
      static cpx_typef avept = 0;
      avept = avept * (1.0f - 0.000001f) + 0.000001f * curr;
      curr -= avept;
    }

    demodSamples[i] = curr;
  }

  for (int a = 0; a < VFOmain.length(); a++) {
    vfo *pvfo = VFOmain.at(a);

    pvfo->process(demodSamples);
  }
}


void Publisher::handleHup() {
  DBG("Got SIGHUP signal from EventNotifier");
  
  // TODO: do debug dump for debugging?
}

void Publisher::handleInterrupt() {
  DBG("Got SIGINT signal from EventNotifier");
  running = false; 
}

void Publisher::handleTerm() {
  DBG("Got SIGTERM signal from EventNotifier");
  running = false;
}
