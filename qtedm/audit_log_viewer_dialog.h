/*
 * audit_log_viewer_dialog.h
 *
 * Dialog for viewing audit logs for the current user.
 */

#ifndef AUDIT_LOG_VIEWER_DIALOG_H
#define AUDIT_LOG_VIEWER_DIALOG_H

#include <QDialog>
#include <QFont>
#include <QPalette>
#include <QString>
#include <QStringList>
#include <QDateTime>

class QComboBox;
class QTextEdit;
class QPushButton;
class QLineEdit;
class QDateTimeEdit;
class QTabWidget;
class QCheckBox;

class AuditLogViewerDialog : public QDialog
{
public:
  AuditLogViewerDialog(const QPalette &basePalette, const QFont &itemFont,
      QWidget *parent = nullptr);

  void showAndRaise();

private:
  void populateLogFiles();
  void loadSelectedLog();
  void refreshLogList();
  void performSearch();

  QString getLogDirectory() const;
  QString getCurrentUser() const;
  QStringList findUserLogFiles() const;
  QStringList findLogFilesInRange(const QDateTime &start, const QDateTime &end) const;
  QDateTime extractLogFileTimestamp(const QString &filePath) const;

  /* Single log view widgets */
  QComboBox *logFileCombo_ = nullptr;
  QTextEdit *logContent_ = nullptr;
  QPushButton *refreshButton_ = nullptr;

  /* Search tab widgets */
  QLineEdit *pvSearchEdit_ = nullptr;
  QDateTimeEdit *startDateEdit_ = nullptr;
  QDateTimeEdit *endDateEdit_ = nullptr;
  QCheckBox *caseSensitiveCheck_ = nullptr;
  QPushButton *searchButton_ = nullptr;
  QTextEdit *searchResults_ = nullptr;

  /* Common widgets */
  QTabWidget *tabWidget_ = nullptr;
  QPushButton *closeButton_ = nullptr;
  QFont itemFont_;
  QPalette basePalette_;
  QString currentUser_;
};

#endif /* AUDIT_LOG_VIEWER_DIALOG_H */
