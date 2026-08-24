/*
 * audit_logger.cc
 *
 * Audit logging for control widget value changes in QtEDM.
 * Logs ca_put operations to files in ~/.medm directory.
 */

#include "audit_logger.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>
#include <QMutexLocker>

#ifdef Q_OS_UNIX
#include <pwd.h>
#include <unistd.h>
#endif

AuditLogger &AuditLogger::instance()
{
  static AuditLogger logger;
  return logger;
}

AuditLogger::AuditLogger()
  : enabled_(true)
  , initialized_(false)
  , sessionStart_(QDateTime::currentDateTime())
{
  currentUser_ = getCurrentUser();
}

AuditLogger::~AuditLogger()
{
  shutdown();
}

void AuditLogger::initialize(bool enabled)
{
  QMutexLocker lock(&mutex_);
  enabled_ = enabled;
  initialized_ = true;

  if (enabled_) {
    /* Pre-create the log directory if needed */
    QDir dir(getLogDirectory());
    if (!dir.exists()) {
      dir.mkpath(QStringLiteral("."));
    }
  }
}

void AuditLogger::shutdown()
{
  QMutexLocker lock(&mutex_);
  if (logFile_ && logFile_->isOpen()) {
    logFile_->close();
  }
  logFile_.reset();
}

QString AuditLogger::getLogDirectory() const
{
  const QString configured = qEnvironmentVariable("QTEDM_AUDIT_DIR").trimmed();
  if (!configured.isEmpty()) {
    return QDir(configured).absolutePath();
  }
  QString homeDir = QDir::homePath();
  return QDir(homeDir).filePath(QStringLiteral(".medm"));
}

QString AuditLogger::getLogFilePath() const
{
  QString dir = getLogDirectory();
  QString timestamp = sessionStart_.toString(QStringLiteral("yyyyMMdd_HHmmss"));
  /* Include PID to ensure unique filenames when multiple users run on
   * a shared group account simultaneously */
  qint64 pid = QCoreApplication::applicationPid();
  QString filename = QStringLiteral("audit_%1_%2.log").arg(timestamp).arg(pid);
  return QDir(dir).filePath(filename);
}

QString AuditLogger::getCurrentUser() const
{
#ifdef Q_OS_UNIX
  /* Try to get the real username */
  struct passwd *pw = getpwuid(getuid());
  if (pw && pw->pw_name) {
    return QString::fromLocal8Bit(pw->pw_name);
  }
#endif

  /* Fallback to environment variables */
  QString user = qEnvironmentVariable("USER");
  if (user.isEmpty()) {
    user = qEnvironmentVariable("USERNAME");
  }
  if (user.isEmpty()) {
    user = QStringLiteral("unknown");
  }
  return user;
}

bool AuditLogger::ensureLogFileOpen()
{
  if (logFile_ && logFile_->isOpen()) {
    return true;
  }

  QString logPath = getLogFilePath();
  QString logDir = getLogDirectory();

  /* Ensure directory exists */
  QDir dir(logDir);
  if (!dir.exists()) {
    if (!dir.mkpath(QStringLiteral("."))) {
      qWarning() << "AuditLogger: Failed to create directory" << logDir;
      return false;
    }
  }

  logFile_ = std::make_unique<QFile>(logPath);
  if (!logFile_->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    qWarning() << "AuditLogger: Failed to open log file" << logPath
               << logFile_->errorString();
    logFile_.reset();
    return false;
  }

  /* Write header for new file */
  QTextStream stream(logFile_.get());
  stream << "# QtEDM Audit Log\n";
  stream << "# Session started: " << sessionStart_.toString(Qt::ISODate) << "\n";
  stream << "# User: " << currentUser_ << "\n";
  stream << "# Format: timestamp|user|widgetType|pvName|value|displayFile\n";
  stream << "#\n";
  stream.flush();

  return true;
}

void AuditLogger::logPut(const QString &pvName,
                         const QString &value,
                         const QString &widgetType,
                         const QString &displayFile)
{
  if (!enabled_ || !initialized_) {
    return;
  }

  QMutexLocker lock(&mutex_);

  if (!ensureLogFileOpen()) {
    return;
  }

  QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
  QString display = displayFile.isEmpty() ? QStringLiteral("-") : displayFile;

  QTextStream stream(logFile_.get());
  stream << encodeLogField(timestamp) << "|"
         << encodeLogField(currentUser_) << "|"
         << encodeLogField(widgetType) << "|"
         << encodeLogField(pvName) << "|"
         << encodeLogField(value) << "|"
         << encodeLogField(display) << "\n";
  stream.flush();
}

QString AuditLogger::encodeLogField(const QString &value)
{
  QString encoded;
  encoded.reserve(value.size());
  for (QChar ch : value) {
    if (ch == QLatin1Char('\\')) {
      encoded.append(QStringLiteral("\\\\"));
    } else if (ch == QLatin1Char('|')) {
      encoded.append(QStringLiteral("\\|"));
    } else if (ch == QLatin1Char('\n')) {
      encoded.append(QStringLiteral("\\n"));
    } else if (ch == QLatin1Char('\r')) {
      encoded.append(QStringLiteral("\\r"));
    } else {
      encoded.append(ch);
    }
  }
  return encoded;
}

bool AuditLogger::decodeLogRecord(const QString &line, QStringList *fields)
{
  if (!fields) {
    return false;
  }

  fields->clear();
  QString field;
  bool escaped = false;
  for (QChar ch : line) {
    if (escaped) {
      const bool valueField = fields->size() == 4;
      if (valueField && ch == QLatin1Char('n')) {
        field.append(QLatin1Char('\n'));
      } else if (valueField && ch == QLatin1Char('r')) {
        field.append(QLatin1Char('\r'));
      } else if (ch == QLatin1Char('|') || ch == QLatin1Char('\\')) {
        field.append(ch);
      } else {
        field.append(QLatin1Char('\\'));
        field.append(ch);
      }
      escaped = false;
    } else if (ch == QLatin1Char('\\')) {
      escaped = true;
    } else if (ch == QLatin1Char('|')) {
      fields->append(field);
      field.clear();
    } else {
      field.append(ch);
    }
  }
  if (escaped) {
    field.append(QLatin1Char('\\'));
  }
  fields->append(field);

  if (fields->size() < 6) {
    fields->clear();
    return false;
  }
  if (fields->size() > 6) {
    const QString display = fields->mid(5).join(QLatin1Char('|'));
    *fields = fields->mid(0, 5);
    fields->append(display);
  }
  return true;
}

void AuditLogger::logPut(const QString &pvName,
                         double value,
                         const QString &widgetType,
                         const QString &displayFile)
{
  logPut(pvName, QString::number(value, 'g', 15), widgetType, displayFile);
}

void AuditLogger::logPut(const QString &pvName,
                         int value,
                         const QString &widgetType,
                         const QString &displayFile)
{
  logPut(pvName, QString::number(value), widgetType, displayFile);
}

void AuditLogger::logBlockedPut(const QString &pvName,
                                const QString &value,
                                const QString &reason)
{
  const QString blockedType = QStringLiteral("BLOCKED:%1").arg(reason);
  logPut(pvName, value, blockedType);
}
