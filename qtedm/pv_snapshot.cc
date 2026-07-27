#include "pv_snapshot.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QSet>

namespace {

QJsonObject entryToJson(const PvSnapshotEntry &entry)
{
  QJsonObject object{
      {QStringLiteral("provider"), entry.provider},
      {QStringLiteral("pv"), entry.pvName},
      {QStringLiteral("fieldType"), entry.fieldType},
      {QStringLiteral("exactType"), entry.exactType},
      {QStringLiteral("kind"), PvSnapshot::kindName(entry.kind)},
      {QStringLiteral("value"), entry.value},
      {QStringLiteral("timestamp"),
          entry.timestamp.toUTC().toString(Qt::ISODateWithMs)},
      {QStringLiteral("units"), entry.units},
      {QStringLiteral("connected"), entry.connected},
      {QStringLiteral("writeAccess"), entry.writeAccess},
  };
  if (entry.hasLimits) {
    object[QStringLiteral("limits")] = QJsonObject{
        {QStringLiteral("low"), entry.lowerLimit},
        {QStringLiteral("high"), entry.upperLimit},
    };
  }
  if (!entry.enumStrings.isEmpty()) {
    object[QStringLiteral("enumStrings")] =
        QJsonArray::fromStringList(entry.enumStrings);
  }
  return object;
}

bool finiteJsonNumber(const QJsonValue &value)
{
  return value.isDouble() && std::isfinite(value.toDouble());
}

bool decodeCanonicalBase64(const QString &text, QByteArray &decoded)
{
  const QByteArray encoded = text.toLatin1();
  if (encoded.size() % 4 != 0) {
    return false;
  }
  decoded = QByteArray::fromBase64(encoded);
  return decoded.toBase64() == encoded;
}

QString exactBaseType(QString exactType)
{
  const qsizetype bracket = exactType.indexOf(QLatin1Char('['));
  if (bracket >= 0) {
    exactType.truncate(bracket);
  }
  return exactType.trimmed().toUpper();
}

bool numericValueFitsExactType(double value, const QString &exactType)
{
  const QString type = exactBaseType(exactType);
  double low = 0.0;
  double high = 0.0;
  bool integral = true;
  if (type == QStringLiteral("DBF_CHAR")
      || type == QStringLiteral("PVA_UBYTE")) {
    low = 0.0;
    high = 255.0;
  } else if (type == QStringLiteral("DBF_ENUM")) {
    low = 0.0;
    high = 65535.0;
  } else if (type == QStringLiteral("DBF_SHORT")
      || type == QStringLiteral("PVA_SHORT")) {
    low = -32768.0;
    high = 32767.0;
  } else if (type == QStringLiteral("PVA_USHORT")) {
    low = 0.0;
    high = 65535.0;
  } else if (type == QStringLiteral("DBF_LONG")
      || type == QStringLiteral("PVA_INT")) {
    low = -2147483648.0;
    high = 2147483647.0;
  } else if (type == QStringLiteral("PVA_UINT")) {
    low = 0.0;
    high = 4294967295.0;
  } else if (type == QStringLiteral("PVA_BYTE")) {
    low = -128.0;
    high = 127.0;
  } else if (type == QStringLiteral("PVA_LONG")) {
    low = -9007199254740991.0;
    high = 9007199254740991.0;
  } else if (type == QStringLiteral("PVA_ULONG")) {
    low = 0.0;
    high = 9007199254740991.0;
  } else {
    integral = false;
  }
  return !integral
      || (std::trunc(value) == value && value >= low && value <= high);
}

bool validateMetadata(const PvSnapshotEntry &entry, QString &error)
{
  const QString provider = entry.provider.trimmed().toLower();
  if ((provider != QStringLiteral("ca")
          && provider != QStringLiteral("pva")
          && provider != QStringLiteral("soft"))
      || entry.pvName.trimmed().isEmpty()
      || entry.exactType.trimmed().isEmpty()) {
    error = QStringLiteral("Snapshot entry is missing required provider, PV, "
                           "or exact-type data.");
    return false;
  }
  return true;
}

bool validateValue(const PvSnapshotEntry &entry, QString &error)
{
  if (!entry.connected && entry.value.isNull()) {
    return true;
  }
  switch (entry.kind) {
  case PvSnapshotValueKind::kNumeric:
  case PvSnapshotValueKind::kEnum:
    if (!finiteJsonNumber(entry.value)) {
      error = QStringLiteral("PV %1 has a non-numeric value.")
          .arg(entry.pvName);
      return false;
    }
    if (entry.kind == PvSnapshotValueKind::kEnum) {
      const double value = entry.value.toDouble();
      if (std::trunc(value) != value || value < 0.0 || value > 65535.0) {
        error = QStringLiteral("PV %1 has an invalid enum index.")
            .arg(entry.pvName);
        return false;
      }
    } else if (!numericValueFitsExactType(
                   entry.value.toDouble(), entry.exactType)) {
      error = QStringLiteral("PV %1 has a value outside its exact type.")
          .arg(entry.pvName);
      return false;
    }
    break;
  case PvSnapshotValueKind::kString:
  case PvSnapshotValueKind::kCharArray:
    if (!entry.value.isString()) {
      error = QStringLiteral("PV %1 has an invalid string value.")
          .arg(entry.pvName);
      return false;
    }
    if (entry.kind == PvSnapshotValueKind::kCharArray) {
      QByteArray bytes;
      if (!decodeCanonicalBase64(entry.value.toString(), bytes)) {
        error = QStringLiteral("PV %1 has invalid Base64 character data.")
            .arg(entry.pvName);
        return false;
      }
      if (bytes.size() > PvSnapshot::kMaximumArrayElements) {
        error = QStringLiteral("PV %1 exceeds the character-array limit.")
            .arg(entry.pvName);
        return false;
      }
    }
    break;
  case PvSnapshotValueKind::kNumericArray:
  case PvSnapshotValueKind::kStringArray: {
    if (!entry.value.isArray()
        || entry.value.toArray().size() > PvSnapshot::kMaximumArrayElements) {
      error = QStringLiteral("PV %1 has an invalid or oversized array.")
          .arg(entry.pvName);
      return false;
    }
    for (const QJsonValue &item : entry.value.toArray()) {
      if ((entry.kind == PvSnapshotValueKind::kNumericArray
              && !finiteJsonNumber(item))
          || (entry.kind == PvSnapshotValueKind::kStringArray
              && !item.isString())) {
        error = QStringLiteral("PV %1 has an invalid array element.")
            .arg(entry.pvName);
        return false;
      }
      if (entry.kind == PvSnapshotValueKind::kNumericArray
          && !numericValueFitsExactType(
              item.toDouble(), entry.exactType)) {
        error = QStringLiteral(
            "PV %1 has an array value outside its exact type.")
            .arg(entry.pvName);
        return false;
      }
    }
    break;
  }
  }
  return true;
}

bool entryFromJson(const QJsonObject &object, PvSnapshotEntry &entry,
    QString &error)
{
  entry.provider = object.value(QStringLiteral("provider")).toString();
  entry.pvName = object.value(QStringLiteral("pv")).toString().trimmed();
  entry.fieldType = object.value(QStringLiteral("fieldType")).toInt(-1);
  entry.exactType = object.value(QStringLiteral("exactType")).toString();
  if (!validateMetadata(entry, error)
      || !PvSnapshot::kindFromName(
          object.value(QStringLiteral("kind")).toString(), entry.kind)) {
    if (error.isEmpty()) {
      error = QStringLiteral("Snapshot entry has an invalid value kind.");
    }
    return false;
  }
  if (!object.value(QStringLiteral("connected")).isBool()
      || !object.value(QStringLiteral("writeAccess")).isBool()) {
    error = QStringLiteral("PV %1 has invalid connection or access metadata.")
        .arg(entry.pvName);
    return false;
  }
  entry.value = object.value(QStringLiteral("value"));
  const QJsonValue timestampValue =
      object.value(QStringLiteral("timestamp"));
  if (!timestampValue.isString()) {
    error = QStringLiteral("PV %1 has invalid timestamp metadata.")
        .arg(entry.pvName);
    return false;
  }
  const QString timestampText = timestampValue.toString();
  entry.timestamp = QDateTime::fromString(timestampText, Qt::ISODate);
  if (!timestampText.isEmpty() && !entry.timestamp.isValid()) {
    error = QStringLiteral("PV %1 has an invalid timestamp.")
        .arg(entry.pvName);
    return false;
  }
  entry.units = object.value(QStringLiteral("units")).toString();
  entry.connected = object.value(QStringLiteral("connected")).toBool();
  entry.writeAccess = object.value(QStringLiteral("writeAccess")).toBool();
  const QJsonValue limitsValue = object.value(QStringLiteral("limits"));
  if (!limitsValue.isUndefined()) {
    if (!limitsValue.isObject()) {
      error = QStringLiteral("PV %1 has invalid limit metadata.")
          .arg(entry.pvName);
      return false;
    }
    const QJsonObject limits = limitsValue.toObject();
    if (!finiteJsonNumber(limits.value(QStringLiteral("low")))
        || !finiteJsonNumber(limits.value(QStringLiteral("high")))) {
      error = QStringLiteral("PV %1 has non-numeric limits.")
          .arg(entry.pvName);
      return false;
    }
    entry.lowerLimit = limits.value(QStringLiteral("low")).toDouble();
    entry.upperLimit = limits.value(QStringLiteral("high")).toDouble();
    entry.hasLimits = true;
  }
  const QJsonValue statesValue =
      object.value(QStringLiteral("enumStrings"));
  if (!statesValue.isUndefined() && !statesValue.isArray()) {
    error = QStringLiteral("PV %1 has invalid enum metadata.")
        .arg(entry.pvName);
    return false;
  }
  const QJsonArray states = statesValue.toArray();
  for (const QJsonValue &state : states) {
    if (!state.isString()) {
      error = QStringLiteral("PV %1 has invalid enum metadata.")
          .arg(entry.pvName);
      return false;
    }
    entry.enumStrings.append(state.toString());
  }
  return validateValue(entry, error);
}

bool valueWithinLimits(const PvSnapshotEntry &entry)
{
  if (!entry.hasLimits) {
    return true;
  }
  const double low = std::min(entry.lowerLimit, entry.upperLimit);
  const double high = std::max(entry.lowerLimit, entry.upperLimit);
  if (entry.kind == PvSnapshotValueKind::kNumeric) {
    const double value = entry.value.toDouble();
    return value >= low && value <= high;
  }
  if (entry.kind == PvSnapshotValueKind::kNumericArray) {
    for (const QJsonValue &item : entry.value.toArray()) {
      const double value = item.toDouble();
      if (value < low || value > high) {
        return false;
      }
    }
  }
  return true;
}

} // namespace

QString PvSnapshotEntry::displayValue() const
{
  if (value.isNull() || value.isUndefined()) {
    return QStringLiteral("Unavailable");
  }
  if (kind == PvSnapshotValueKind::kCharArray) {
    const QByteArray bytes = QByteArray::fromBase64(
        value.toString().toLatin1());
    return QStringLiteral("%1 byte(s): %2")
        .arg(bytes.size())
        .arg(QString::fromUtf8(bytes.left(48)));
  }
  if (value.isArray()) {
    const QJsonArray array = value.toArray();
    QStringList preview;
    const int count = static_cast<int>(
        std::min<qsizetype>(array.size(), 6));
    for (int index = 0; index < count; ++index) {
      preview.append(array.at(index).toVariant().toString());
    }
    return QStringLiteral("[%1%2] (%3)")
        .arg(preview.join(QStringLiteral(", ")))
        .arg(array.size() > count ? QStringLiteral(", …") : QString())
        .arg(array.size());
  }
  if (kind == PvSnapshotValueKind::kEnum) {
    const int index = value.toInt();
    if (index >= 0 && index < enumStrings.size()) {
      return QStringLiteral("%1 (%2)").arg(enumStrings.at(index)).arg(index);
    }
  }
  return value.toVariant().toString();
}

bool PvSnapshot::save(const QString &filePath,
    const PvSnapshotDocument &document, QString *error)
{
  if (filePath.trimmed().isEmpty()) {
    if (error) {
      *error = QStringLiteral("Snapshot file path is empty.");
    }
    return false;
  }
  if (document.entries.size() > kMaximumEntries) {
    if (error) {
      *error = QStringLiteral("Snapshot exceeds the %1 PV limit.")
          .arg(kMaximumEntries);
    }
    return false;
  }

  QJsonArray entries;
  for (const PvSnapshotEntry &entry : document.entries) {
    QString validationError;
    if (!validateMetadata(entry, validationError)
        || !validateValue(entry, validationError)) {
      if (error) {
        *error = validationError;
      }
      return false;
    }
    entries.append(entryToJson(entry));
  }
  const QJsonObject root{
      {QStringLiteral("schemaVersion"), kSchemaVersion},
      {QStringLiteral("createdAt"),
          document.createdAt.toUTC().toString(Qt::ISODateWithMs)},
      {QStringLiteral("displayPath"), document.displayPath},
      {QStringLiteral("entries"), entries},
  };
  const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
  if (bytes.size() > kMaximumFileBytes) {
    if (error) {
      *error = QStringLiteral("Snapshot exceeds the %1 byte limit.")
          .arg(kMaximumFileBytes);
    }
    return false;
  }
  QSaveFile file(filePath);
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()
      || !file.commit()) {
    if (error) {
      *error = QStringLiteral("Unable to save snapshot: %1")
          .arg(file.errorString());
    }
    return false;
  }
  if (error) {
    error->clear();
  }
  return true;
}

PvSnapshotLoadResult PvSnapshot::load(const QString &filePath)
{
  PvSnapshotLoadResult result;
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    result.error = QStringLiteral("Unable to open snapshot: %1")
        .arg(file.errorString());
    return result;
  }
  if (file.size() > kMaximumFileBytes) {
    result.error = QStringLiteral("Snapshot exceeds the %1 byte limit.")
        .arg(kMaximumFileBytes);
    return result;
  }
  QJsonParseError parseError;
  const QJsonDocument json = QJsonDocument::fromJson(file.readAll(),
      &parseError);
  if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
    result.error = QStringLiteral("Malformed snapshot JSON: %1")
        .arg(parseError.errorString());
    return result;
  }
  const QJsonObject root = json.object();
  if (root.value(QStringLiteral("schemaVersion")).toInt(-1)
      != kSchemaVersion) {
    result.error = QStringLiteral("Unsupported snapshot schema version.");
    return result;
  }
  const QJsonArray entries = root.value(QStringLiteral("entries")).toArray();
  if (entries.size() > kMaximumEntries) {
    result.error = QStringLiteral("Snapshot exceeds the %1 PV limit.")
        .arg(kMaximumEntries);
    return result;
  }
  result.document.schemaVersion = kSchemaVersion;
  result.document.createdAt = QDateTime::fromString(
      root.value(QStringLiteral("createdAt")).toString(), Qt::ISODate);
  result.document.displayPath =
      root.value(QStringLiteral("displayPath")).toString();
  QSet<QString> seen;
  for (const QJsonValue &value : entries) {
    if (!value.isObject()) {
      result.error = QStringLiteral("Snapshot contains a non-object entry.");
      return result;
    }
    PvSnapshotEntry entry;
    if (!entryFromJson(value.toObject(), entry, result.error)) {
      return result;
    }
    const QString key = entry.provider.toLower() + QLatin1Char('|')
        + entry.pvName;
    if (seen.contains(key)) {
      result.error = QStringLiteral("Snapshot contains duplicate PV %1.")
          .arg(entry.pvName);
      return result;
    }
    seen.insert(key);
    result.document.entries.append(entry);
  }
  return result;
}

PvSnapshotRestoreCheck PvSnapshot::canRestore(
    const PvSnapshotEntry &saved, const PvSnapshotEntry &current,
    bool observeOnly)
{
  if (observeOnly) {
    return {false, QStringLiteral("Observe-only mode blocks restores.")};
  }
  if (!saved.connected || saved.value.isNull()) {
    return {false, QStringLiteral("Snapshot has no connected saved value.")};
  }
  if (!current.connected) {
    return {false, QStringLiteral("PV is disconnected.")};
  }
  if (!current.writeAccess) {
    return {false, QStringLiteral("PV is not writable.")};
  }
  if (saved.provider.compare(current.provider, Qt::CaseInsensitive) != 0
      || saved.fieldType != current.fieldType
      || saved.exactType != current.exactType
      || saved.kind != current.kind) {
    return {false, QStringLiteral("Provider or exact PV type changed.")};
  }
  if (saved.kind == PvSnapshotValueKind::kEnum
      && saved.enumStrings != current.enumStrings) {
    return {false, QStringLiteral("Enum choices changed.")};
  }
  if (saved.kind == PvSnapshotValueKind::kNumericArray
      || saved.kind == PvSnapshotValueKind::kStringArray) {
    if (saved.kind == PvSnapshotValueKind::kStringArray) {
      return {false,
          QStringLiteral("String-array restores are not supported.")};
    }
    if (saved.value.toArray().size() > kMaximumArrayElements) {
      return {false, QStringLiteral("Saved array exceeds the restore limit.")};
    }
  }
  if (saved.kind == PvSnapshotValueKind::kCharArray
      && QByteArray::fromBase64(saved.value.toString().toLatin1()).size()
          > kMaximumArrayElements) {
    return {false, QStringLiteral("Saved character array exceeds the restore limit.")};
  }
  PvSnapshotEntry rangeCheck = saved;
  rangeCheck.hasLimits = current.hasLimits;
  rangeCheck.lowerLimit = current.lowerLimit;
  rangeCheck.upperLimit = current.upperLimit;
  if (!valueWithinLimits(rangeCheck)) {
    return {false, QStringLiteral("Saved value is outside current limits.")};
  }
  return {true, QStringLiteral("Ready")};
}

QString PvSnapshot::kindName(PvSnapshotValueKind kind)
{
  switch (kind) {
  case PvSnapshotValueKind::kNumeric:
    return QStringLiteral("numeric");
  case PvSnapshotValueKind::kString:
    return QStringLiteral("string");
  case PvSnapshotValueKind::kEnum:
    return QStringLiteral("enum");
  case PvSnapshotValueKind::kCharArray:
    return QStringLiteral("char-array");
  case PvSnapshotValueKind::kNumericArray:
    return QStringLiteral("numeric-array");
  case PvSnapshotValueKind::kStringArray:
    return QStringLiteral("string-array");
  }
  return {};
}

bool PvSnapshot::kindFromName(const QString &name,
    PvSnapshotValueKind &kind)
{
  const QString normalized = name.trimmed().toLower();
  if (normalized == QStringLiteral("numeric")) {
    kind = PvSnapshotValueKind::kNumeric;
  } else if (normalized == QStringLiteral("string")) {
    kind = PvSnapshotValueKind::kString;
  } else if (normalized == QStringLiteral("enum")) {
    kind = PvSnapshotValueKind::kEnum;
  } else if (normalized == QStringLiteral("char-array")) {
    kind = PvSnapshotValueKind::kCharArray;
  } else if (normalized == QStringLiteral("numeric-array")) {
    kind = PvSnapshotValueKind::kNumericArray;
  } else if (normalized == QStringLiteral("string-array")) {
    kind = PvSnapshotValueKind::kStringArray;
  } else {
    return false;
  }
  return true;
}
