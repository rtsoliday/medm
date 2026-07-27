#include "session_manager.h"

#include <algorithm>
#include <limits>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

QJsonObject macrosToJson(const QHash<QString, QString> &macros)
{
  QJsonObject object;
  QStringList keys = macros.keys();
  keys.sort();
  for (const QString &key : keys) {
    object.insert(key, macros.value(key));
  }
  return object;
}

QHash<QString, QString> macrosFromJson(const QJsonValue &value,
    QStringList *warnings, int windowIndex)
{
  QHash<QString, QString> macros;
  if (value.isUndefined() || value.isNull()) {
    return macros;
  }
  if (!value.isObject()) {
    if (warnings) {
      warnings->append(QStringLiteral(
          "Window %1 has invalid macros; they were ignored.")
          .arg(windowIndex + 1));
    }
    return macros;
  }
  const QJsonObject object = value.toObject();
  for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
    if (it.value().isString()) {
      macros.insert(it.key(), it.value().toString());
    } else if (warnings) {
      warnings->append(QStringLiteral(
          "Window %1 macro %2 is not a string; it was ignored.")
          .arg(windowIndex + 1).arg(it.key()));
    }
  }
  return macros;
}

QJsonObject geometryToJson(const QRect &geometry)
{
  return {
      {QStringLiteral("x"), geometry.x()},
      {QStringLiteral("y"), geometry.y()},
      {QStringLiteral("width"), geometry.width()},
      {QStringLiteral("height"), geometry.height()},
  };
}

QRect geometryFromJson(const QJsonValue &value, bool *valid)
{
  if (valid) {
    *valid = false;
  }
  if (!value.isObject()) {
    return QRect();
  }
  const QJsonObject object = value.toObject();
  const QJsonValue x = object.value(QStringLiteral("x"));
  const QJsonValue y = object.value(QStringLiteral("y"));
  const QJsonValue width = object.value(QStringLiteral("width"));
  const QJsonValue height = object.value(QStringLiteral("height"));
  if (!x.isDouble() || !y.isDouble() || !width.isDouble()
      || !height.isDouble()) {
    return QRect();
  }
  const QRect result(x.toInt(), y.toInt(), width.toInt(), height.toInt());
  if (result.width() <= 0 || result.height() <= 0) {
    return QRect();
  }
  if (valid) {
    *valid = true;
  }
  return result;
}

}  // namespace

SessionManager::SessionManager(const QString &directoryOverride)
  : directoryOverride_(directoryOverride)
{
}

QString SessionManager::sessionsDirectory() const
{
  if (!directoryOverride_.isEmpty()) {
    return QDir::cleanPath(directoryOverride_);
  }
  return QDir(QStandardPaths::writableLocation(
      QStandardPaths::AppConfigLocation)).filePath(QStringLiteral("sessions"));
}

QString SessionManager::sessionPath(const QString &name) const
{
  if (!isValidSessionName(name)) {
    return QString();
  }
  return QDir(sessionsDirectory()).filePath(
      name.trimmed() + QStringLiteral(".qtedm-session.json"));
}

QStringList SessionManager::sessionNames() const
{
  QDir directory(sessionsDirectory());
  const QString suffix = QStringLiteral(".qtedm-session.json");
  QStringList result;
  const QStringList files = directory.entryList(
      {QStringLiteral("*.qtedm-session.json")}, QDir::Files, QDir::Name);
  for (const QString &file : files) {
    if (file.endsWith(suffix)) {
      result.append(file.left(file.size() - suffix.size()));
    }
  }
  return result;
}

bool SessionManager::save(const QtEdmSession &session, QString *error) const
{
  const QString name = session.name.trimmed();
  const QString path = sessionPath(name);
  if (path.isEmpty()) {
    if (error) {
      *error = QStringLiteral(
          "Session names may contain letters, numbers, spaces, '.', '_', "
          "and '-' and must not be a path.");
    }
    return false;
  }
  if (session.windows.isEmpty()) {
    if (error) {
      *error = QStringLiteral("There are no display windows to save.");
    }
    return false;
  }

  QJsonArray windows;
  for (const QtEdmSessionWindow &window : session.windows) {
    QJsonObject object;
    object.insert(QStringLiteral("displayPath"), window.displayPath);
    object.insert(QStringLiteral("macros"), macrosToJson(window.macros));
    object.insert(QStringLiteral("geometry"), geometryToJson(window.geometry));
    object.insert(QStringLiteral("screen"), window.screenName);
    object.insert(QStringLiteral("activeTab"), window.activeTabId);
    object.insert(QStringLiteral("mode"),
        window.editMode ? QStringLiteral("edit") : QStringLiteral("execute"));
    windows.append(object);
  }
  QJsonObject root;
  root.insert(QStringLiteral("schemaVersion"),
      QtEdmSession::kSchemaVersion);
  root.insert(QStringLiteral("name"), name);
  root.insert(QStringLiteral("windows"), windows);

  QDir directory(sessionsDirectory());
  if (!directory.mkpath(QStringLiteral("."))) {
    if (error) {
      *error = QStringLiteral("Failed to create session directory: %1")
          .arg(directory.absolutePath());
    }
    return false;
  }

  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    if (error) {
      *error = QStringLiteral("Failed to open session file: %1").arg(path);
    }
    return false;
  }
  if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0
      || !file.commit()) {
    if (error) {
      *error = QStringLiteral("Failed to write session file: %1").arg(path);
    }
    return false;
  }
  return true;
}

QtEdmSessionLoadResult SessionManager::load(const QString &name) const
{
  QtEdmSessionLoadResult result;
  const QString trimmedName = name.trimmed();
  result.session.name = trimmedName;
  const QString path = sessionPath(trimmedName);
  if (path.isEmpty()) {
    result.error = QStringLiteral("Invalid session name: %1").arg(name);
    return result;
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    result.error = QStringLiteral("Cannot open session: %1").arg(path);
    return result;
  }
  QJsonParseError parseError;
  const QJsonDocument document =
      QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError
      || !document.isObject()) {
    result.error = QStringLiteral("Invalid session JSON: %1")
        .arg(parseError.errorString());
    return result;
  }

  const QJsonObject root = document.object();
  if (root.value(QStringLiteral("schemaVersion")).toInt(-1)
      != QtEdmSession::kSchemaVersion) {
    result.error = QStringLiteral("Unsupported session schema version.");
    return result;
  }
  const QJsonValue windowsValue = root.value(QStringLiteral("windows"));
  if (!windowsValue.isArray()) {
    result.error = QStringLiteral("Session does not contain a windows array.");
    return result;
  }

  const QJsonArray windows = windowsValue.toArray();
  for (int index = 0; index < windows.size(); ++index) {
    if (!windows.at(index).isObject()) {
      result.warnings.append(QStringLiteral(
          "Window %1 is invalid and was omitted.").arg(index + 1));
      continue;
    }
    const QJsonObject object = windows.at(index).toObject();
    QtEdmSessionWindow window;
    window.displayPath =
        object.value(QStringLiteral("displayPath")).toString().trimmed();
    if (window.displayPath.isEmpty()) {
      result.warnings.append(QStringLiteral(
          "Window %1 has no display path and was omitted.").arg(index + 1));
      continue;
    }
    window.macros = macrosFromJson(object.value(QStringLiteral("macros")),
        &result.warnings, index);
    bool geometryValid = false;
    window.geometry = geometryFromJson(
        object.value(QStringLiteral("geometry")), &geometryValid);
    if (!geometryValid) {
      result.warnings.append(QStringLiteral(
          "Window %1 has invalid geometry; a safe default will be used.")
          .arg(index + 1));
      window.geometry = QRect(0, 0, 640, 480);
    }
    window.screenName = object.value(QStringLiteral("screen")).toString();
    window.activeTabId =
        object.value(QStringLiteral("activeTab")).toString();
    const QString mode =
        object.value(QStringLiteral("mode")).toString().trimmed().toLower();
    window.editMode = mode == QStringLiteral("edit");
    if (mode != QStringLiteral("edit")
        && mode != QStringLiteral("execute") && !mode.isEmpty()) {
      result.warnings.append(QStringLiteral(
          "Window %1 has an unknown mode; execute mode will be used.")
          .arg(index + 1));
    }
    result.session.windows.append(window);
  }
  if (result.session.windows.isEmpty()) {
    result.error = QStringLiteral("Session contains no restorable windows.");
  }
  return result;
}

bool SessionManager::isValidSessionName(const QString &name)
{
  const QString trimmed = name.trimmed();
  if (trimmed.isEmpty() || trimmed == QStringLiteral(".")
      || trimmed == QStringLiteral("..") || trimmed.size() > 64) {
    return false;
  }
  static const QRegularExpression pattern(
      QStringLiteral(R"(^[A-Za-z0-9][A-Za-z0-9._ -]*$)"));
  return pattern.match(trimmed).hasMatch();
}

QRect SessionManager::clampGeometryToScreens(const QRect &requested,
    const QString &preferredScreenName,
    const QList<QPair<QString, QRect>> &screens)
{
  QRect safeRequested = requested;
  if (safeRequested.width() <= 0 || safeRequested.height() <= 0) {
    safeRequested = QRect(0, 0, 640, 480);
  }
  if (screens.isEmpty()) {
    return safeRequested;
  }

  QRect targetScreen;
  for (const auto &screen : screens) {
    if (!preferredScreenName.isEmpty()
        && screen.first == preferredScreenName && screen.second.isValid()) {
      targetScreen = screen.second;
      break;
    }
  }
  if (!targetScreen.isValid()) {
    int bestArea = -1;
    for (const auto &screen : screens) {
      const QRect intersection = screen.second.intersected(safeRequested);
      const int area = intersection.width() * intersection.height();
      if (area > bestArea && screen.second.isValid()) {
        bestArea = area;
        targetScreen = screen.second;
      }
    }
  }
  if (!targetScreen.isValid()) {
    targetScreen = screens.first().second;
  }

  const int width = std::clamp(safeRequested.width(), 100,
      std::max(100, targetScreen.width()));
  const int height = std::clamp(safeRequested.height(), 100,
      std::max(100, targetScreen.height()));
  const int maxX = targetScreen.right() - width + 1;
  const int maxY = targetScreen.bottom() - height + 1;
  const int x = std::clamp(safeRequested.x(), targetScreen.left(),
      std::max(targetScreen.left(), maxX));
  const int y = std::clamp(safeRequested.y(), targetScreen.top(),
      std::max(targetScreen.top(), maxY));
  return QRect(x, y, width, height);
}
