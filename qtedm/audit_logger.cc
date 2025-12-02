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

  /* Escape pipe characters in value */
  QString safeValue = value;
  safeValue.replace(QLatin1Char('|'), QStringLiteral("\\|"));
  safeValue.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
  safeValue.replace(QLatin1Char('\r'), QStringLiteral("\\r"));

  QTextStream stream(logFile_.get());
  stream << timestamp << "|"
         << currentUser_ << "|"
         << widgetType << "|"
         << pvName << "|"
         << safeValue << "|"
         << display << "\n";
  stream.flush();
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
