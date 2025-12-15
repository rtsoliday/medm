/*
 * audit_log_viewer_dialog.cc
 *
 * Dialog for viewing audit logs for the current user.
 */

#include "audit_log_viewer_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTabWidget>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>

#ifdef Q_OS_UNIX
#include <pwd.h>
#include <unistd.h>
#endif

AuditLogViewerDialog::AuditLogViewerDialog(const QPalette &basePalette,
    const QFont &itemFont, QWidget *parent)
  : QDialog(parent)
  , itemFont_(itemFont)
  , basePalette_(basePalette)
{
  currentUser_ = getCurrentUser();

  setObjectName(QStringLiteral("qtedmAuditLogViewerDialog"));
  setWindowTitle(QStringLiteral("Audit Log Viewer - %1").arg(currentUser_));
  setModal(false);
  setAutoFillBackground(true);
  setPalette(basePalette);
  setBackgroundRole(QPalette::Window);
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  resize(800, 600);

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(12, 12, 12, 12);
  mainLayout->setSpacing(10);

  /* Create tab widget */
  tabWidget_ = new QTabWidget;
  tabWidget_->setFont(itemFont_);

  /* ===== Browse Tab ===== */
  auto *browseTab = new QWidget;
  auto *browseLayout = new QVBoxLayout(browseTab);
  browseLayout->setContentsMargins(8, 8, 8, 8);
  browseLayout->setSpacing(8);

  /* Log file selector */
  auto *selectorLayout = new QHBoxLayout;
  selectorLayout->setContentsMargins(0, 0, 0, 0);
  selectorLayout->setSpacing(8);

  auto *label = new QLabel(QStringLiteral("Log File:"));
  label->setFont(itemFont_);
  selectorLayout->addWidget(label);

  logFileCombo_ = new QComboBox;
  logFileCombo_->setFont(itemFont_);
  logFileCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  selectorLayout->addWidget(logFileCombo_);

  refreshButton_ = new QPushButton(QStringLiteral("Refresh"));
  refreshButton_->setFont(itemFont_);
  selectorLayout->addWidget(refreshButton_);

  browseLayout->addLayout(selectorLayout);

  /* Log content display */
  logContent_ = new QTextEdit;
  logContent_->setReadOnly(true);
  logContent_->setFont(QFont(QStringLiteral("Monospace"), itemFont_.pointSize()));
  logContent_->setAutoFillBackground(true);
  logContent_->setPalette(basePalette);
  logContent_->setBackgroundRole(QPalette::Base);
  logContent_->setLineWrapMode(QTextEdit::NoWrap);
  browseLayout->addWidget(logContent_);

  tabWidget_->addTab(browseTab, QStringLiteral("Browse Logs"));

  /* ===== Search Tab ===== */
  auto *searchTab = new QWidget;
  auto *searchLayout = new QVBoxLayout(searchTab);
  searchLayout->setContentsMargins(8, 8, 8, 8);
  searchLayout->setSpacing(8);

  /* Search criteria group */
  auto *criteriaGroup = new QGroupBox(QStringLiteral("Search Criteria"));
  criteriaGroup->setFont(itemFont_);
  auto *criteriaLayout = new QVBoxLayout(criteriaGroup);
  criteriaLayout->setSpacing(8);

  /* PV name search */
  auto *pvLayout = new QHBoxLayout;
  pvLayout->setSpacing(8);
  auto *pvLabel = new QLabel(QStringLiteral("PV Name:"));
  pvLabel->setFont(itemFont_);
  pvLayout->addWidget(pvLabel);
  pvSearchEdit_ = new QLineEdit;
  pvSearchEdit_->setFont(itemFont_);
  pvSearchEdit_->setPlaceholderText(QStringLiteral("Enter PV name or pattern (e.g., SR:*, *:status)"));
  pvLayout->addWidget(pvSearchEdit_);
  criteriaLayout->addLayout(pvLayout);

  /* Date range */
  auto *dateLayout = new QHBoxLayout;
  dateLayout->setSpacing(8);

  auto *startLabel = new QLabel(QStringLiteral("From:"));
  startLabel->setFont(itemFont_);
  dateLayout->addWidget(startLabel);

  startDateEdit_ = new QDateTimeEdit;
  startDateEdit_->setFont(itemFont_);
  startDateEdit_->setCalendarPopup(true);
  startDateEdit_->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
  startDateEdit_->setDateTime(QDateTime::currentDateTime().addDays(-7));
  dateLayout->addWidget(startDateEdit_);

  auto *endLabel = new QLabel(QStringLiteral("To:"));
  endLabel->setFont(itemFont_);
  dateLayout->addWidget(endLabel);

  endDateEdit_ = new QDateTimeEdit;
  endDateEdit_->setFont(itemFont_);
  endDateEdit_->setCalendarPopup(true);
  endDateEdit_->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
  endDateEdit_->setDateTime(QDateTime::currentDateTime());
  dateLayout->addWidget(endDateEdit_);

  dateLayout->addStretch();
  criteriaLayout->addLayout(dateLayout);

  /* Options */
  auto *optionsLayout = new QHBoxLayout;
  optionsLayout->setSpacing(8);

  caseSensitiveCheck_ = new QCheckBox(QStringLiteral("Case sensitive"));
  caseSensitiveCheck_->setFont(itemFont_);
  optionsLayout->addWidget(caseSensitiveCheck_);

  optionsLayout->addStretch();

  searchButton_ = new QPushButton(QStringLiteral("Search"));
  searchButton_->setFont(itemFont_);
  searchButton_->setDefault(true);
  optionsLayout->addWidget(searchButton_);

  criteriaLayout->addLayout(optionsLayout);

  searchLayout->addWidget(criteriaGroup);

  /* Search results */
  auto *resultsLabel = new QLabel(QStringLiteral("Search Results:"));
  resultsLabel->setFont(itemFont_);
  searchLayout->addWidget(resultsLabel);

  searchResults_ = new QTextEdit;
  searchResults_->setReadOnly(true);
  searchResults_->setFont(QFont(QStringLiteral("Monospace"), itemFont_.pointSize()));
  searchResults_->setAutoFillBackground(true);
  searchResults_->setPalette(basePalette);
  searchResults_->setBackgroundRole(QPalette::Base);
  searchResults_->setLineWrapMode(QTextEdit::NoWrap);
  searchLayout->addWidget(searchResults_);

  tabWidget_->addTab(searchTab, QStringLiteral("Search"));

  mainLayout->addWidget(tabWidget_);

  /* Close button */
  auto *buttonRow = new QHBoxLayout;
  buttonRow->setContentsMargins(0, 0, 0, 0);
  buttonRow->addStretch();
  closeButton_ = new QPushButton(QStringLiteral("Close"));
  closeButton_->setFont(itemFont_);
  buttonRow->addWidget(closeButton_);
  mainLayout->addLayout(buttonRow);

  /* Connections */
  connect(logFileCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
      this, [this](int) {
        loadSelectedLog();
      });

  connect(refreshButton_, &QPushButton::clicked, this, [this]() {
    refreshLogList();
  });

  connect(searchButton_, &QPushButton::clicked, this, [this]() {
    performSearch();
  });

  connect(pvSearchEdit_, &QLineEdit::returnPressed, this, [this]() {
    performSearch();
  });

  connect(closeButton_, &QPushButton::clicked, this, &QDialog::accept);

  /* Initial population */
  populateLogFiles();
}

void AuditLogViewerDialog::showAndRaise()
{
  refreshLogList();
  show();
  raise();
  activateWindow();
}

QString AuditLogViewerDialog::getLogDirectory() const
{
  QString homeDir = QDir::homePath();
  return QDir(homeDir).filePath(QStringLiteral(".medm"));
}

QString AuditLogViewerDialog::getCurrentUser() const
{
#ifdef Q_OS_UNIX
  struct passwd *pw = getpwuid(getuid());
  if (pw && pw->pw_name) {
    return QString::fromLocal8Bit(pw->pw_name);
  }
#endif

  QString user = qEnvironmentVariable("USER");
  if (user.isEmpty()) {
    user = qEnvironmentVariable("USERNAME");
  }
  if (user.isEmpty()) {
    user = QStringLiteral("unknown");
  }
  return user;
}

QDateTime AuditLogViewerDialog::extractLogFileTimestamp(const QString &filePath) const
{
  QFileInfo info(filePath);
  QString baseName = info.baseName();
  QRegularExpression re(QStringLiteral("audit_(\\d{8})_(\\d{6})_(\\d+)"));
  QRegularExpressionMatch match = re.match(baseName);

  if (match.hasMatch()) {
    QString dateStr = match.captured(1);
    QString timeStr = match.captured(2);
    QDate date = QDate::fromString(dateStr, QStringLiteral("yyyyMMdd"));
    QTime time = QTime::fromString(timeStr, QStringLiteral("HHmmss"));
    return QDateTime(date, time);
  }

  return QDateTime();
}

QStringList AuditLogViewerDialog::findUserLogFiles() const
{
  QStringList result;
  QDir logDir(getLogDirectory());

  if (!logDir.exists()) {
    return result;
  }

  /* Find all audit log files */
  QStringList filters;
  filters << QStringLiteral("audit_*.log");
  QFileInfoList files = logDir.entryInfoList(filters, QDir::Files, QDir::Time);

  /* Filter to show only logs that contain entries for the current user */
  for (const QFileInfo &fileInfo : files) {
    QFile file(fileInfo.absoluteFilePath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QTextStream stream(&file);
      bool hasUserEntries = false;
      while (!stream.atEnd()) {
        QString line = stream.readLine();
        /* Skip comments */
        if (line.startsWith(QLatin1Char('#'))) {
          /* Check if this is "# User: username" header line */
          if (line.contains(QStringLiteral("# User: ") + currentUser_)) {
            hasUserEntries = true;
            break;
          }
          continue;
        }
        /* Check if data line contains current user */
        QStringList parts = line.split(QLatin1Char('|'));
        if (parts.size() >= 2 && parts.at(1) == currentUser_) {
          hasUserEntries = true;
          break;
        }
      }
      file.close();

      if (hasUserEntries) {
        result.append(fileInfo.absoluteFilePath());
      }
    }
  }

  return result;
}

QStringList AuditLogViewerDialog::findLogFilesInRange(const QDateTime &start,
    const QDateTime &end) const
{
  QStringList result;
  QDir logDir(getLogDirectory());

  if (!logDir.exists()) {
    return result;
  }

  QStringList filters;
  filters << QStringLiteral("audit_*.log");
  QFileInfoList files = logDir.entryInfoList(filters, QDir::Files, QDir::Time);

  for (const QFileInfo &fileInfo : files) {
    QDateTime fileTime = extractLogFileTimestamp(fileInfo.absoluteFilePath());
    if (fileTime.isValid()) {
      /* Include files whose session started before the end date
       * We'll filter individual entries by timestamp during search */
      if (fileTime <= end) {
        result.append(fileInfo.absoluteFilePath());
      }
    } else {
      /* If we can't parse the timestamp, include it anyway */
      result.append(fileInfo.absoluteFilePath());
    }
  }

  return result;
}

void AuditLogViewerDialog::populateLogFiles()
{
  logFileCombo_->clear();

  QStringList logFiles = findUserLogFiles();

  if (logFiles.isEmpty()) {
    logFileCombo_->addItem(QStringLiteral("(No audit logs found for user %1)")
        .arg(currentUser_));
    logFileCombo_->setEnabled(false);
    logContent_->clear();
    logContent_->setPlainText(QStringLiteral(
        "No audit log files found for user '%1'.\n\n"
        "Audit logs are stored in: %2\n\n"
        "Logs are created when control widgets write values to PVs.")
        .arg(currentUser_)
        .arg(getLogDirectory()));
    return;
  }

  logFileCombo_->setEnabled(true);

  for (const QString &filePath : logFiles) {
    QFileInfo info(filePath);
    /* Extract timestamp from filename: audit_YYYYMMDD_HHMMSS_PID.log */
    QString baseName = info.baseName();
    QRegularExpression re(QStringLiteral("audit_(\\d{8})_(\\d{6})_(\\d+)"));
    QRegularExpressionMatch match = re.match(baseName);

    QString displayName;
    if (match.hasMatch()) {
      QString dateStr = match.captured(1);
      QString timeStr = match.captured(2);
      QString pidStr = match.captured(3);

      QDate date = QDate::fromString(dateStr, QStringLiteral("yyyyMMdd"));
      QTime time = QTime::fromString(timeStr, QStringLiteral("HHmmss"));

      displayName = QStringLiteral("%1 %2 (PID %3)")
          .arg(date.toString(QStringLiteral("yyyy-MM-dd")))
          .arg(time.toString(QStringLiteral("HH:mm:ss")))
          .arg(pidStr);
    } else {
      displayName = info.fileName();
    }

    logFileCombo_->addItem(displayName, filePath);
  }

  /* Select the most recent log (first in list since sorted by time) */
  if (logFileCombo_->count() > 0) {
    logFileCombo_->setCurrentIndex(0);
  }
}

void AuditLogViewerDialog::loadSelectedLog()
{
  QString filePath = logFileCombo_->currentData().toString();
  if (filePath.isEmpty()) {
    return;
  }

  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    logContent_->setPlainText(QStringLiteral("Error: Could not open file:\n%1")
        .arg(filePath));
    return;
  }

  QString content;
  QTextStream stream(&file);

  /* Header */
  content.append(QStringLiteral("=== Audit Log for user: %1 ===\n")
      .arg(currentUser_));
  content.append(QStringLiteral("File: %1\n\n").arg(filePath));
  content.append(QStringLiteral("%1 %2 %3 %4 %5\n")
      .arg(QStringLiteral("Timestamp").leftJustified(25))
      .arg(QStringLiteral("Widget").leftJustified(15))
      .arg(QStringLiteral("PV Name").leftJustified(20))
      .arg(QStringLiteral("Value").leftJustified(30))
      .arg(QStringLiteral("Display")));
  content.append(QString(100, QLatin1Char('-')) + QStringLiteral("\n"));

  int entryCount = 0;
  while (!stream.atEnd()) {
    QString line = stream.readLine();

    /* Skip comment lines but show session header */
    if (line.startsWith(QLatin1Char('#'))) {
      if (line.startsWith(QStringLiteral("# Session started:"))) {
        content.append(QStringLiteral("\n%1\n").arg(line));
      }
      continue;
    }

    /* Parse data line: timestamp|user|widgetType|pvName|value|displayFile */
    QStringList parts = line.split(QLatin1Char('|'));
    if (parts.size() >= 6) {
      QString user = parts.at(1);
      /* Only show entries for the current user */
      if (user == currentUser_) {
        QString timestamp = parts.at(0);
        QString widgetType = parts.at(2);
        QString pvName = parts.at(3);
        QString value = parts.at(4);
        QString displayFile = parts.at(5);

        /* Unescape value */
        value.replace(QStringLiteral("\\|"), QStringLiteral("|"));
        value.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
        value.replace(QStringLiteral("\\r"), QStringLiteral("\r"));

        /* Truncate long values for display */
        if (value.length() > 25) {
          value = value.left(22) + QStringLiteral("...");
        }

        content.append(QStringLiteral("%1 %2 %3 %4 %5\n")
            .arg(timestamp.leftJustified(25))
            .arg(widgetType.leftJustified(15))
            .arg(pvName.left(20).leftJustified(20))
            .arg(value.leftJustified(30))
            .arg(displayFile == QStringLiteral("-") ? QString() : displayFile));

        entryCount++;
      }
    }
  }

  file.close();

  content.append(QString(100, QLatin1Char('-')) + QStringLiteral("\n"));
  content.append(QStringLiteral("Total entries for %1: %2\n")
      .arg(currentUser_)
      .arg(entryCount));

  logContent_->setPlainText(content);
  /* Scroll to bottom to show most recent entries */
  logContent_->moveCursor(QTextCursor::End);
}

void AuditLogViewerDialog::refreshLogList()
{
  QString currentPath = logFileCombo_->currentData().toString();
  populateLogFiles();

  /* Try to restore previous selection */
  if (!currentPath.isEmpty()) {
    int index = logFileCombo_->findData(currentPath);
    if (index >= 0) {
      logFileCombo_->setCurrentIndex(index);
    }
  }
}

void AuditLogViewerDialog::performSearch()
{
  QString searchPattern = pvSearchEdit_->text().trimmed();
  if (searchPattern.isEmpty()) {
    searchResults_->setPlainText(
        QStringLiteral("Please enter a PV name or pattern to search for.\n\n"
            "You can use wildcards:\n"
            "  * matches any characters (e.g., SR:* matches SR:BPM:X, SR:MAG:I)\n"
            "  ? matches a single character"));
    return;
  }

  QDateTime startTime = startDateEdit_->dateTime();
  QDateTime endTime = endDateEdit_->dateTime();
  bool caseSensitive = caseSensitiveCheck_->isChecked();

  if (startTime > endTime) {
    searchResults_->setPlainText(
        QStringLiteral("Error: Start date must be before end date."));
    return;
  }

  /* Convert wildcard pattern to regex */
  QString regexPattern = QRegularExpression::escape(searchPattern);
  regexPattern.replace(QStringLiteral("\\*"), QStringLiteral(".*"));
  regexPattern.replace(QStringLiteral("\\?"), QStringLiteral("."));
  regexPattern = QStringLiteral("^") + regexPattern + QStringLiteral("$");

  QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
  if (!caseSensitive) {
    options |= QRegularExpression::CaseInsensitiveOption;
  }
  QRegularExpression pvRegex(regexPattern, options);

  if (!pvRegex.isValid()) {
    searchResults_->setPlainText(
        QStringLiteral("Error: Invalid search pattern."));
    return;
  }

  /* Find all log files that might contain entries in the date range */
  QStringList logFiles = findLogFilesInRange(startTime, endTime);

  QString content;
  content.append(QStringLiteral("=== Search Results ===\n"));
  content.append(QStringLiteral("PV Pattern: %1\n").arg(searchPattern));
  content.append(QStringLiteral("Time Range: %1 to %2\n")
      .arg(startTime.toString(QStringLiteral("yyyy-MM-dd HH:mm")))
      .arg(endTime.toString(QStringLiteral("yyyy-MM-dd HH:mm"))));
  content.append(QStringLiteral("User: %1\n").arg(currentUser_));
  content.append(QStringLiteral("Case Sensitive: %1\n\n")
      .arg(caseSensitive ? QStringLiteral("Yes") : QStringLiteral("No")));

  content.append(QStringLiteral("%1 %2 %3 %4 %5\n")
      .arg(QStringLiteral("Timestamp").leftJustified(25))
      .arg(QStringLiteral("Widget").leftJustified(15))
      .arg(QStringLiteral("PV Name").leftJustified(30))
      .arg(QStringLiteral("Value").leftJustified(30))
      .arg(QStringLiteral("Display")));
  content.append(QString(110, QLatin1Char('-')) + QStringLiteral("\n"));

  int totalMatches = 0;
  int filesSearched = 0;

  for (const QString &filePath : logFiles) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      continue;
    }

    filesSearched++;
    QTextStream stream(&file);

    while (!stream.atEnd()) {
      QString line = stream.readLine();

      /* Skip comments */
      if (line.startsWith(QLatin1Char('#'))) {
        continue;
      }

      /* Parse data line: timestamp|user|widgetType|pvName|value|displayFile */
      QStringList parts = line.split(QLatin1Char('|'));
      if (parts.size() >= 6) {
        QString user = parts.at(1);
        /* Only show entries for the current user */
        if (user != currentUser_) {
          continue;
        }

        QString timestampStr = parts.at(0);
        QDateTime entryTime = QDateTime::fromString(timestampStr, Qt::ISODate);

        /* Check if entry is within time range */
        if (!entryTime.isValid() || entryTime < startTime || entryTime > endTime) {
          continue;
        }

        QString pvName = parts.at(3);

        /* Check if PV matches the pattern */
        if (!pvRegex.match(pvName).hasMatch()) {
          continue;
        }

        /* We have a match! */
        QString widgetType = parts.at(2);
        QString value = parts.at(4);
        QString displayFile = parts.at(5);

        /* Unescape value */
        value.replace(QStringLiteral("\\|"), QStringLiteral("|"));
        value.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
        value.replace(QStringLiteral("\\r"), QStringLiteral("\r"));

        /* Truncate long values for display */
        if (value.length() > 25) {
          value = value.left(22) + QStringLiteral("...");
        }

        content.append(QStringLiteral("%1 %2 %3 %4 %5\n")
            .arg(timestampStr.leftJustified(25))
            .arg(widgetType.leftJustified(15))
            .arg(pvName.left(30).leftJustified(30))
            .arg(value.leftJustified(30))
            .arg(displayFile == QStringLiteral("-") ? QString() : displayFile));

        totalMatches++;
      }
    }

    file.close();
  }

  content.append(QString(110, QLatin1Char('-')) + QStringLiteral("\n"));
  content.append(QStringLiteral("Found %1 matching entries in %2 log files.\n")
      .arg(totalMatches)
      .arg(filesSearched));

  searchResults_->setPlainText(content);
  searchResults_->moveCursor(QTextCursor::Start);
}
