/*
 * audit_logger.h
 *
 * Audit logging for control widget value changes in QtEDM.
 * Logs ca_put operations to files in ~/.medm directory.
 *
 * Logging is enabled by default and can be disabled via:
 *   - Command line option: -nolog
 *   - Environment variable: QTEDM_NOLOG=1
 */

#ifndef AUDIT_LOGGER_H
#define AUDIT_LOGGER_H

#include <QString>
#include <QFile>
#include <QMutex>
#include <QDateTime>
#include <memory>

class AuditLogger
{
public:
  static AuditLogger &instance();

  /* Initialize the logger. Call once at startup after parsing command line. */
  void initialize(bool enabled);

  /* Check if logging is enabled */
  bool isEnabled() const { return enabled_; }

  /* Log a value write operation
   * @param pvName     The process variable name
   * @param value      The value written (as string representation)
   * @param widgetType The type of control widget (e.g., "TextEntry", "Slider")
   * @param displayFile The .adl file containing the widget (optional)
   */
  void logPut(const QString &pvName,
              const QString &value,
              const QString &widgetType,
              const QString &displayFile = QString());

  /* Convenience overloads for numeric types */
  void logPut(const QString &pvName,
              double value,
              const QString &widgetType,
              const QString &displayFile = QString());

  void logPut(const QString &pvName,
              int value,
              const QString &widgetType,
              const QString &displayFile = QString());

  /* Shutdown and close log file */
  void shutdown();

private:
  AuditLogger();
  ~AuditLogger();

  /* Non-copyable */
  AuditLogger(const AuditLogger &) = delete;
  AuditLogger &operator=(const AuditLogger &) = delete;

  bool ensureLogFileOpen();
  QString getLogFilePath() const;
  QString getLogDirectory() const;
  QString getCurrentUser() const;

  bool enabled_ = true;
  bool initialized_ = false;
  std::unique_ptr<QFile> logFile_;
  QMutex mutex_;
  QString currentUser_;
  QDateTime sessionStart_;
};

#endif /* AUDIT_LOGGER_H */
