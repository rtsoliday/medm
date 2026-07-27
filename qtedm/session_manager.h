#pragma once

#include <QHash>
#include <QList>
#include <QRect>
#include <QString>
#include <QStringList>

struct QtEdmSessionWindow
{
  QString displayPath;
  QHash<QString, QString> macros;
  QRect geometry;
  QString screenName;
  QString activeTabId;
  bool editMode = false;
};

struct QtEdmSession
{
  static constexpr int kSchemaVersion = 1;

  QString name;
  QList<QtEdmSessionWindow> windows;
};

struct QtEdmSessionLoadResult
{
  QtEdmSession session;
  QStringList warnings;
  QString error;

  bool ok() const
  {
    return error.isEmpty();
  }
};

class SessionManager
{
public:
  explicit SessionManager(const QString &directoryOverride = QString());

  QString sessionsDirectory() const;
  QString sessionPath(const QString &name) const;
  QStringList sessionNames() const;

  bool save(const QtEdmSession &session, QString *error = nullptr) const;
  QtEdmSessionLoadResult load(const QString &name) const;

  static bool isValidSessionName(const QString &name);
  static QRect clampGeometryToScreens(const QRect &requested,
      const QString &preferredScreenName,
      const QList<QPair<QString, QRect>> &screens);

private:
  QString directoryOverride_;
};
