#pragma once

#include <functional>
#include <limits>

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;

struct ArchiveSample
{
  qint64 timestampMs = 0;
  double value = 0.0;
  short status = 0;
  short severity = 0;
};

struct ArchiveQuery
{
  QString channel;
  QDateTime from;
  QDateTime to;
  int maximumPoints = 5000;
  int timeoutMs = 10000;
  qsizetype maximumResponseBytes = 8 * 1024 * 1024;
};

struct ArchiveResult
{
  QVector<ArchiveSample> samples;
  QString error;
  bool cancelled = false;
  bool timedOut = false;
  bool oversized = false;

  bool ok() const
  {
    return error.isEmpty() && !cancelled && !timedOut && !oversized;
  }
};

class ArchiveRequest : public QObject
{
public:
  explicit ArchiveRequest(QObject *parent = nullptr);
  ~ArchiveRequest() override;

  void cancel();
  bool isCancelled() const;

private:
  friend class ArchiverApplianceProvider;
  QPointer<QNetworkReply> reply_;
  bool cancelled_ = false;
};

class ArchiveProvider
{
public:
  using Completion = std::function<void(const ArchiveResult &)>;

  virtual ~ArchiveProvider() = default;
  virtual ArchiveRequest *query(const ArchiveQuery &query, QObject *owner,
      Completion completion) = 0;
};

class ArchiverApplianceProvider : public QObject, public ArchiveProvider
{
public:
  explicit ArchiverApplianceProvider(QObject *parent = nullptr,
      QNetworkAccessManager *networkManager = nullptr);

  ArchiveRequest *query(const ArchiveQuery &query, QObject *owner,
      Completion completion) override;

  QString retrievalUrl() const;
  void setRetrievalUrl(const QString &url);

  static ArchiveResult parseJson(const QByteArray &payload,
      int maximumPoints, qsizetype maximumResponseBytes);
  static QVector<ArchiveSample> mergeAndDecimate(
      const QVector<ArchiveSample> &historical,
      const QVector<ArchiveSample> &live, int maximumPoints,
      qint64 minimumTimestampMs = std::numeric_limits<qint64>::min());

private:
  QNetworkAccessManager *networkManager_ = nullptr;
  QString retrievalUrl_;
};
