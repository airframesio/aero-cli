#ifndef JSON_H
#define JSON_H

#include "aerol.h"
#include "decode.h"
#include <QJsonDocument>

QString *toOutputFormat(OutputFormat fmt, const QString &station_id, bool disableReassembly, const ACARSItem &item);

#endif
