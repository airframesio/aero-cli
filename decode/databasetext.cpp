#include "databasetext.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

DataBaseText *dbtext = NULL;

DataBaseTextUser::DataBaseTextUser(QObject *parent) : QObject(parent) {
  if (dbtext == NULL)
    dbtext = new DataBaseText(parent);
  refcounter = 0;
  map.clear();
}

DataBaseTextUser::~DataBaseTextUser() { clear(); }

void DataBaseTextUser::clear() {
  QList<DBase *> list = map.values();
  for (int i = 0; i < list.size(); i++) {
    delete list[i];
  }
  map.clear();
}

DBase *DataBaseTextUser::getuserdata(int ref) {
  int cnt = map.count(ref);
  if (cnt == 0)
    return NULL;
  else if (cnt > 1) {
    qDebug() << "Error more than one entry in DataBaseTextUser map";
    map.clear();
    return NULL;
  }
  DBase *obj = map.value(ref);
  map.remove(ref);
  return obj;
}

// QMetaObject::invokeMethod(sender,member, Qt::QueuedConnection,Q_ARG(bool, false),Q_ARG(int, userdata),Q_ARG(const QStringList&, values));

void DataBaseTextUser::request(const QString &dirname, const QString &AEStext,
                               DBase *userdata) {
  QStringList values;
  
  if (userdata == NULL) {
    values.push_back("userdata is NULL");
    emit result(false, -1, values);

    return;
  }
  if (refcounter < 0)
    refcounter = 0;
  refcounter++;
  refcounter %= DataBaseTextUser_MAX_PACKETS_IN_QUEUE;
  DBase *obj;
  while ((obj = map.take(refcounter)) != NULL)
    delete obj;
  map.insert(refcounter, userdata);
  emit result(false, refcounter, values);
}

//------------

DataBaseText::DataBaseText(QObject *parent) : QObject(parent) {
  workerThread.start();
}

DataBaseText::~DataBaseText() {
  workerThread.quit();
  workerThread.wait();
}
