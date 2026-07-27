#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct PvaInfoSnapshot
{
  QString pvName;
  bool connected = false;
  bool canRead = false;
  bool canWrite = false;
  int fieldType = -1;
  unsigned long elementCount = 0;
  QString nativeDataType;
  QString host;
  QString value;
  double numericValue = 0.0;
  unsigned int enumValue = 0;
  bool isNumeric = false;
  bool isEnum = false;
  bool hasValue = false;
  QVector<double> arrayValues;
  bool isArray = false;
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
