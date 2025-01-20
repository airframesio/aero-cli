#ifndef SINK_H
#define SINK_H

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QIODevice>

class AudioSink : public QIODevice {
  Q_OBJECT

public:
  AudioSink(const QString &topic, const QAudioDevice device, QObject *parent = nullptr);
  AudioSink(const AudioSink &) = delete;
  AudioSink(AudioSink &&) noexcept = delete;
  ~AudioSink();

  AudioSink &operator=(const AudioSink &) = delete;
  AudioSink &operator=(AudioSink &&) noexcept = delete;

  qint64 readData(char *data, qint64 maxlen);
  qint64 writeData(const char *data, qint64 len);

private:
  QString topicName;
  QAudioDevice targetDevice;

  QAudioFormat targetFormat;
  QAudioSource *targetSource;
  
public slots:
  void begin();
  void end();
    
  void handleHup();
  void handleInterrupt();
  void handleTerminate();

signals:
  void completed();
};

#endif
