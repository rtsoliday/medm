#pragma once

#include <QString>
#include <QStringList>

struct PvaInfoSnapshot
{
  QString pvName;
  bool connected = false;
  bool canRead = false;
  bool canWrite = false;
  int fieldType = -1;
  unsigned long elementCount = 0;
  QString host;
  QString value;
  bool hasValue = false;
  short severity = 0;
  double hopr = 0.0;
  double lopr = 0.0;
  bool hasLimits = false;
  int precision = -1;
  bool hasPrecision = false;
  QString units;
  bool hasUnits = false;
  QStringList states;
  bool hasStates = false;
};

bool getPvaInfoSnapshot(const QString &pvName, PvaInfoSnapshot &snapshot);