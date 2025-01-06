#ifndef LOGGER_H
#define LOGGER_H

#include <QDebug>

#define INF(fmt, ...) { \
  qDebug(fmt __VA_OPT__(,) __VA_ARGS__); \
}
  
#define DBG(fmt, ...) { \
  if (gMaxLogVerbosity) qDebug("\033[1;34m[DEBUG] " fmt "\033[0m" __VA_OPT__(,) __VA_ARGS__); \
}

#define CRIT(fmt, ...) {\
  qDebug("\033[1;31m[CRITICAL] " fmt "\033[0m" __VA_OPT__(,) __VA_ARGS__); \
}

#define FATAL(fmt, ...) {\
  qDebug("\033[1;31m[FATAL] " fmt "\033[0m" __VA_OPT__(,) __VA_ARGS__); \
}

extern bool gMaxLogVerbosity;

#endif
