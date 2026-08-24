#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

struct RemoteDisplayRequest
{
  QString filePath;
  QString macros;
  QString geometry;
};

QByteArray encodeRemoteDisplayRequest(const QString &filePath,
    const QString &macros, const QString &geometry);

QVector<QByteArray> chunkRemoteDisplayRequest(const QByteArray &encoded,
    int chunkSize);

class RemoteDisplayRequestDecoder
{
public:
  QVector<RemoteDisplayRequest> consume(const QByteArray &chunk);
  void reset();

private:
  enum class Field {
    kFilePath,
    kMacros,
    kGeometry,
  };

  bool collecting_ = false;
  Field field_ = Field::kFilePath;
  QByteArray filePath_;
  QByteArray macros_;
  QByteArray geometry_;
};
