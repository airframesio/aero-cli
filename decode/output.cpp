#include "output.h"
#include "decode.h"
#include <QByteArray>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>

template <typename T>
QString upperHex(T a, int fieldWidth, int base, QChar fillChar) {
  return QString("%1").arg(a, fieldWidth, base, fillChar).toUpper();
}

QString *toOutputFormat(OutputFormat fmt, const QString &station_id,
                        bool disableReassembly, const ACARSItem &item) {
  switch (fmt) {
  case OutputFormat::Jaero:
  case OutputFormat::JsonDump:
  case OutputFormat::Text:
    break;
  default:
    return nullptr;
  }

  QByteArray TAKstr;
  TAKstr += item.TAK;
  if (item.TAK == 0x15)
    TAKstr = ((QString) "!").toLatin1();

  uchar label1 = ' ';
  if (item.LABEL.size() > 1) {
    label1 = item.LABEL[1];
    if ((uchar)item.LABEL[1] == 127)
      label1 = 'd';
  }

  QDateTime time = QDateTime::currentDateTimeUtc();

  if (fmt == OutputFormat::JsonDump || fmt == OutputFormat::Jaero) {
    QJsonObject root;
    QString message = item.message;
    message.replace('\r', '\n');
    message.replace("\n\n", "\n");
    if (message.right(1) == "\n")
      message.chop(1);
    if (message.left(1) == "\n")
      message.remove(0, 1);
    message.replace("\n", "\n\t");

    if (fmt == OutputFormat::JsonDump) {
      QJsonObject app;
      app["name"] = QCoreApplication::applicationName();
      app["ver"] = QCoreApplication::applicationVersion();
      root["app"] = QJsonValue(app);

      QJsonObject isu;

      QJsonObject aes;
      aes["type"] = "Aircraft Earth Station";
      aes["addr"] = upperHex(item.isuitem.AESID, 6, 16, QChar('0'));

      QJsonObject ges;
      ges["type"] = "Ground Earth Station";
      ges["addr"] = upperHex(item.isuitem.GESID, 2, 16, QChar('0'));

      if (!item.nonacars) {
        QJsonObject acars;
        acars["mode"] = (QString)item.MODE;
        acars["ack"] = (QString)TAKstr;
        acars["blk_id"] = QString((QChar)item.BI);
        acars["label"] =
            QString("%1%2").arg(QChar(item.LABEL[0])).arg(QChar(label1));
        acars["reg"] = (QString)item.PLANEREG;

        if (!message.isEmpty()) {
          if (item.downlink) {
            acars["msg_num"] = message.mid(0, 3);
            acars["msg_num_seq"] = message.mid(3, 1);
            acars["flight"] = message.mid(4, 6);
            acars["msg_text"] = message.mid(4 + 6);
          } else {
            acars["msg_text"] = message;
          }

          if (!item.parsed.isEmpty()) {
            for (auto it = item.parsed.constBegin();
                 it != item.parsed.constEnd(); it++) {
              acars.insert(it.key(), it.value());
            }
          }
        }

        isu["acars"] = QJsonValue(acars);
      }

      isu["refno"] = upperHex(item.isuitem.REFNO, 2, 16, QChar('0'));
      isu["qno"] = upperHex(item.isuitem.QNO, 2, 16, QChar('0'));
      isu["src"] = QJsonValue(item.downlink ? aes : ges);
      isu["dst"] = QJsonValue(item.downlink ? ges : aes);

      QJsonObject t;
      QDateTime ts = time.toUTC();
      t["sec"] = ts.toSecsSinceEpoch();
      t["usec"] = (ts.toMSecsSinceEpoch() % 1000) * 1000;
      root["t"] = QJsonValue(t);

      root["isu"] = QJsonValue(isu);
      root["station"] = station_id;
    } else if (fmt == OutputFormat::Jaero) {
      // add common things
      root["TIME"] = time.toSecsSinceEpoch();
      root["TIME_UTC"] = time.toUTC().toString("yyyy-MM-dd hh:mm:ss");
      root["NAME"] = QCoreApplication::applicationName();
      root["NONACARS"] = item.nonacars;
      root["AESID"] = upperHex(item.isuitem.AESID, 6, 16, QChar('0'));
      root["GESID"] = upperHex(item.isuitem.GESID, 2, 16, QChar('0'));
      root["QNO"] = upperHex(item.isuitem.QNO, 2, 16, QChar('0'));
      root["REFNO"] = upperHex(item.isuitem.REFNO, 2, 16, QChar('0'));
      root["REG"] = (QString)item.PLANEREG;

      // add acars message things
      if (!item.nonacars) {
        root["MODE"] = (QString)item.MODE;
        root["TAK"] = (QString)TAKstr;
        root["LABEL"] =
            QString("%1%2").arg(QChar(item.LABEL[0])).arg(QChar(label1));
        root["BI"] = QString((QChar)item.BI);
      }
    }

    return new QString(QJsonDocument(root).toJson(QJsonDocument::Compact));
  } else if (fmt == OutputFormat::Text) {
    QString *out = new QString;
    QString message = item.message;
    message.replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
        .replace("\a", "\\a");

    *out += QString("%1 AES:%2 GES:%3")
                .arg(time.toUTC().toString("yyyy-MM-ddThh:mm:ssZ"))
                .arg(upperHex(item.isuitem.AESID, 6, 16, QChar('0')))
                .arg(upperHex(item.isuitem.GESID, 6, 16, QChar('0')));

    if (!item.nonacars) {
      *out += QString(" [%1] ACK=%2 BLK=%3 ")
                  .arg(item.PLANEREG, 7)
                  .arg(QString(TAKstr), 1)
                  .arg(QString((QChar)item.BI));

      if (disableReassembly) {
        *out += QString("M=%1 ").arg(item.moretocome ? "1" : "0");
      }

      *out += QString("LBL=%1%2 ").arg(QChar(item.LABEL[0])).arg(QChar(label1));

      if (!message.isEmpty()) {
        if (item.downlink) {
          *out += QString("MSN=%1 FLT=%2 %3")
                      .arg(message.mid(0, 4))
                      .arg(message.mid(4, 6))
                      .arg(message.mid(10));
        } else {
          *out += QString("%1").arg(message);
        }
      }
    }
    return out;
  } else {
    // NOTE: unreachable
    return nullptr;
  }
}
