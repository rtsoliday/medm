#include "archive_provider.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace {

constexpr int kMinimumPointLimit = 2;
constexpr int kMaximumPointLimit = 100000;
constexpr int kMinimumTimeoutMs = 100;
constexpr int kMaximumTimeoutMs = 120000;
constexpr qsizetype kMinimumResponseLimit = 1024;
constexpr qsizetype kMaximumResponseLimit = 64 * 1024 * 1024;

QVector<ArchiveSample> decimate(const QVector<ArchiveSample> &samples,
    int maximumPoints)
{
  const int limit = std::clamp(maximumPoints, kMinimumPointLimit,
      kMaximumPointLimit);
  if (samples.size() <= limit) {
    return samples;
  }

  QVector<ArchiveSample> output;
  output.reserve(limit);
  output.append(samples.first());
  const int interior = limit - 2;
  const qsizetype last = samples.size() - 1;
  for (int index = 1; index <= interior; ++index) {
    const double position = static_cast<double>(index)
        * static_cast<double>(last) / static_cast<double>(limit - 1);
    const qsizetype sampleIndex = std::clamp<qsizetype>(
        static_cast<qsizetype>(std::llround(position)), 1, last - 1);
    output.append(samples.at(sampleIndex));
  }
  output.append(samples.last());
  return output;
}

QUrl retrievalEndpoint(const QString &configured)
{
  QUrl url(configured.trimmed());
  if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) {
    return {};
  }
  const QString scheme = url.scheme().toLower();
  if (scheme != QStringLiteral("http")
      && scheme != QStringLiteral("https")) {
    return {};
  }
  QString path = url.path();
  while (path.endsWith(QLatin1Char('/'))) {
    path.chop(1);
  }
  if (!path.endsWith(QStringLiteral("/data/getData.json"))) {
    path += QStringLiteral("/data/getData.json");
  }
  url.setPath(path);
  url.setQuery(QString());
  url.setFragment({});
  return url;
}

} // namespace

ArchiveRequest::ArchiveRequest(QObject *parent)
  : QObject(parent)
{
}

ArchiveRequest::~ArchiveRequest()
{
  cancel();
}

void ArchiveRequest::cancel()
{
  if (cancelled_) {
    return;
  }
  cancelled_ = true;
  if (reply_) {
    reply_->abort();
  }
}

bool ArchiveRequest::isCancelled() const
{
  return cancelled_;
}

ArchiverApplianceProvider::ArchiverApplianceProvider(QObject *parent,
    QNetworkAccessManager *networkManager)
  : QObject(parent)
  , networkManager_(networkManager)
  , retrievalUrl_(qEnvironmentVariable("QTEDM_ARCHIVER_URL").trimmed())
{
  if (!networkManager_) {
    networkManager_ = new QNetworkAccessManager(this);
  }
}

QString ArchiverApplianceProvider::retrievalUrl() const
{
  return retrievalUrl_;
}

void ArchiverApplianceProvider::setRetrievalUrl(const QString &url)
{
  retrievalUrl_ = url.trimmed();
}

ArchiveRequest *ArchiverApplianceProvider::query(const ArchiveQuery &input,
    QObject *owner, Completion completion)
{
  auto *request = new ArchiveRequest(owner ? owner : this);
  ArchiveQuery query = input;
  query.maximumPoints = std::clamp(query.maximumPoints, kMinimumPointLimit,
      kMaximumPointLimit);
  query.timeoutMs = std::clamp(query.timeoutMs, kMinimumTimeoutMs,
      kMaximumTimeoutMs);
  query.maximumResponseBytes = std::clamp(query.maximumResponseBytes,
      kMinimumResponseLimit, kMaximumResponseLimit);

  const QUrl endpoint = retrievalEndpoint(retrievalUrl_);
  if (query.channel.trimmed().isEmpty() || !query.from.isValid()
      || !query.to.isValid() || query.from >= query.to || endpoint.isEmpty()) {
    QTimer::singleShot(0, request,
        [request, completion = std::move(completion), endpoint]() {
          ArchiveResult result;
          result.error = endpoint.isEmpty()
              ? QStringLiteral("QTEDM_ARCHIVER_URL is not configured or invalid.")
              : QStringLiteral("The archive query is invalid.");
          if (request->isCancelled()) {
            result.error = QStringLiteral("Archive query cancelled.");
            result.cancelled = true;
          }
          completion(result);
        });
    return request;
  }

  QUrl url = endpoint;
  QUrlQuery urlQuery;
  urlQuery.addQueryItem(QStringLiteral("pv"), query.channel.trimmed());
  urlQuery.addQueryItem(QStringLiteral("from"),
      query.from.toUTC().toString(Qt::ISODateWithMs));
  urlQuery.addQueryItem(QStringLiteral("to"),
      query.to.toUTC().toString(Qt::ISODateWithMs));
  url.setQuery(urlQuery);

  QNetworkRequest networkRequest(url);
  networkRequest.setRawHeader("Accept", "application/json");
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  networkRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
      QNetworkRequest::NoLessSafeRedirectPolicy);
#endif
  QNetworkReply *reply = networkManager_->get(networkRequest);
  request->reply_ = reply;

  struct RequestState {
    QByteArray payload;
    bool completed = false;
    bool timedOut = false;
    bool oversized = false;
  };
  auto state = std::make_shared<RequestState>();
  auto *timer = new QTimer(request);
  timer->setSingleShot(true);
  timer->start(query.timeoutMs);

  QObject::connect(timer, &QTimer::timeout, request,
      [request, state]() {
        if (state->completed || request->isCancelled()) {
          return;
        }
        state->timedOut = true;
        if (request->reply_) {
          request->reply_->abort();
        }
      });

  QObject::connect(reply, &QIODevice::readyRead, request,
      [request, reply, state, limit = query.maximumResponseBytes]() {
        if (state->completed || request->isCancelled()) {
          reply->readAll();
          return;
        }
        state->payload.append(reply->readAll());
        if (state->payload.size() > limit) {
          state->oversized = true;
          reply->abort();
        }
      });

  QObject::connect(reply, &QNetworkReply::finished, request,
      [request, reply, timer, state, query,
          completion = std::move(completion)]() {
        if (state->completed) {
          return;
        }
        state->completed = true;
        timer->stop();
        if (!state->oversized && !request->isCancelled()) {
          state->payload.append(reply->readAll());
          if (state->payload.size() > query.maximumResponseBytes) {
            state->oversized = true;
          }
        }

        ArchiveResult result;
        if (request->isCancelled()) {
          result.cancelled = true;
          result.error = QStringLiteral("Archive query cancelled.");
        } else if (state->timedOut) {
          result.timedOut = true;
          result.error = QStringLiteral("Archive query timed out.");
        } else if (state->oversized) {
          result.oversized = true;
          result.error = QStringLiteral("Archive response exceeded %1 bytes.")
              .arg(query.maximumResponseBytes);
        } else if (reply->error() != QNetworkReply::NoError) {
          result.error = QStringLiteral("Archive request failed: %1")
              .arg(reply->errorString());
        } else {
          result = parseJson(state->payload, query.maximumPoints,
              query.maximumResponseBytes);
        }
        request->reply_.clear();
        reply->deleteLater();
        completion(result);
      });
  return request;
}

ArchiveResult ArchiverApplianceProvider::parseJson(const QByteArray &payload,
    int maximumPoints, qsizetype maximumResponseBytes)
{
  ArchiveResult result;
  if (payload.size() > maximumResponseBytes) {
    result.oversized = true;
    result.error = QStringLiteral("Archive response exceeded %1 bytes.")
        .arg(maximumResponseBytes);
    return result;
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
    result.error = QStringLiteral("Malformed archive JSON: %1")
        .arg(parseError.errorString());
    return result;
  }

  const QJsonArray roots = document.array();
  for (const QJsonValue &rootValue : roots) {
    if (!rootValue.isObject()) {
      result.error = QStringLiteral(
          "Malformed archive JSON: response entry is not an object.");
      result.samples.clear();
      return result;
    }
    const QJsonValue dataValue = rootValue.toObject()
        .value(QStringLiteral("data"));
    if (!dataValue.isArray()) {
      result.error = QStringLiteral(
          "Malformed archive JSON: response entry has no data array.");
      result.samples.clear();
      return result;
    }
    const QJsonArray data = dataValue.toArray();
    for (const QJsonValue &sampleValue : data) {
      const QJsonObject sampleObject = sampleValue.toObject();
      const QJsonValue value = sampleObject.value(QStringLiteral("val"));
      if (!value.isDouble()) {
        continue;
      }
      const double numeric = value.toDouble();
      const qint64 seconds = sampleObject.value(QStringLiteral("secs"))
          .toVariant().toLongLong();
      const qint64 nanos = sampleObject.value(QStringLiteral("nanos"))
          .toVariant().toLongLong();
      if (!std::isfinite(numeric) || seconds < 0 || nanos < 0
          || nanos >= 1000000000LL
          || seconds > std::numeric_limits<qint64>::max() / 1000LL) {
        continue;
      }
      ArchiveSample sample;
      sample.timestampMs = seconds * 1000LL + nanos / 1000000LL;
      sample.value = numeric;
      sample.status = static_cast<short>(
          sampleObject.value(QStringLiteral("status")).toInt());
      sample.severity = static_cast<short>(
          sampleObject.value(QStringLiteral("severity")).toInt());
      result.samples.append(sample);
    }
  }

  std::sort(result.samples.begin(), result.samples.end(),
      [](const ArchiveSample &left, const ArchiveSample &right) {
        return left.timestampMs < right.timestampMs;
      });
  result.samples.erase(std::unique(result.samples.begin(), result.samples.end(),
      [](const ArchiveSample &left, const ArchiveSample &right) {
        return left.timestampMs == right.timestampMs;
      }), result.samples.end());
  result.samples = decimate(result.samples, maximumPoints);
  return result;
}

QVector<ArchiveSample> ArchiverApplianceProvider::mergeAndDecimate(
    const QVector<ArchiveSample> &historical,
    const QVector<ArchiveSample> &live, int maximumPoints,
    qint64 minimumTimestampMs)
{
  QVector<ArchiveSample> merged;
  merged.reserve(historical.size() + live.size());
  for (const ArchiveSample &sample : historical) {
    if (sample.timestampMs >= minimumTimestampMs) {
      merged.append(sample);
    }
  }
  for (const ArchiveSample &sample : live) {
    if (sample.timestampMs >= minimumTimestampMs) {
      merged.append(sample);
    }
  }
  std::stable_sort(merged.begin(), merged.end(),
      [](const ArchiveSample &left, const ArchiveSample &right) {
        return left.timestampMs < right.timestampMs;
      });
  QVector<ArchiveSample> unique;
  unique.reserve(merged.size());
  for (const ArchiveSample &sample : merged) {
    if (!unique.isEmpty()
        && unique.last().timestampMs == sample.timestampMs) {
      unique.last() = sample;
    } else {
      unique.append(sample);
    }
  }
  return decimate(unique, maximumPoints);
}
