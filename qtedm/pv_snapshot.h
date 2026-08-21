#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>

enum class PvSnapshotValueKind
{
  kNumeric,
  kString,
  kEnum,
  kCharArray,
  kNumericArray,
  kStringArray,
};

struct PvSnapshotEntry
{
  QString provider;
  QString pvName;
  int fieldType = -1;
  QString exactType;
  PvSnapshotValueKind kind = PvSnapshotValueKind::kNumeric;
  QJsonValue value;
  QDateTime timestamp;
  QString units;
  bool hasLimits = false;
  double lowerLimit = 0.0;
  double upperLimit = 0.0;
  bool connected = false;
  bool writeAccess = false;
  QStringList enumStrings;

  QString displayValue() const;
};

struct PvSnapshotDocument
{
  int schemaVersion = 1;
  QDateTime createdAt;
  QString displayPath;
  QVector<PvSnapshotEntry> entries;
};

struct PvSnapshotLoadResult
{
  PvSnapshotDocument document;
  QString error;

  bool ok() const { return error.isEmpty(); }
};

struct PvSnapshotRestoreCheck
{
  bool allowed = false;
  QString reason;
};

class PvSnapshot
{
public:
  static constexpr int kSchemaVersion = 1;
  static constexpr int kMaximumEntries = 4096;
  static constexpr int kMaximumArrayElements = 16384;
  static constexpr qsizetype kMaximumFileBytes = 16 * 1024 * 1024;

  static QString ensureDefaultFileExtension(const QString &filePath);
  static bool save(const QString &filePath,
      const PvSnapshotDocument &document, QString *error = nullptr);
  static PvSnapshotLoadResult load(const QString &filePath);
  static PvSnapshotRestoreCheck canRestore(const PvSnapshotEntry &saved,
      const PvSnapshotEntry &current, bool observeOnly);
  static QString kindName(PvSnapshotValueKind kind);
  static bool kindFromName(const QString &name, PvSnapshotValueKind &kind);
};
