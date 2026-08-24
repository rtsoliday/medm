#include "remote_request_codec.h"

#include <QFile>

QByteArray encodeRemoteDisplayRequest(const QString &filePath,
    const QString &macros, const QString &geometry)
{
  QByteArray encoded;
  encoded.reserve(filePath.size() + macros.size() + geometry.size() + 4);
  encoded.append('(');
  encoded.append(QFile::encodeName(filePath));
  encoded.append(';');
  encoded.append(macros.toLocal8Bit());
  encoded.append(';');
  encoded.append(geometry.toLocal8Bit());
  encoded.append(')');
  return encoded;
}

QVector<QByteArray> chunkRemoteDisplayRequest(const QByteArray &encoded,
    int chunkSize)
{
  QVector<QByteArray> chunks;
  if (chunkSize <= 0 || encoded.isEmpty()) {
    return chunks;
  }
  for (int offset = 0; offset < encoded.size(); offset += chunkSize) {
    QByteArray chunk = encoded.mid(offset, chunkSize);
    chunk.resize(chunkSize, ' ');
    chunks.append(chunk);
  }
  return chunks;
}

QVector<RemoteDisplayRequest> RemoteDisplayRequestDecoder::consume(
    const QByteArray &chunk)
{
  QVector<RemoteDisplayRequest> requests;
  for (char ch : chunk) {
    if (ch == '(') {
      collecting_ = true;
      field_ = Field::kFilePath;
      filePath_.clear();
      macros_.clear();
      geometry_.clear();
      continue;
    }
    if (!collecting_) {
      continue;
    }
    if (ch == ';') {
      field_ = field_ == Field::kFilePath ? Field::kMacros
          : Field::kGeometry;
      continue;
    }
    if (ch == ')') {
      requests.append({
          QFile::decodeName(filePath_), QString::fromLocal8Bit(macros_),
          QString::fromLocal8Bit(geometry_)});
      collecting_ = false;
      field_ = Field::kFilePath;
      continue;
    }
    if (ch == '\0') {
      continue;
    }
    switch (field_) {
    case Field::kFilePath:
      filePath_.append(ch);
      break;
    case Field::kMacros:
      macros_.append(ch);
      break;
    case Field::kGeometry:
      geometry_.append(ch);
      break;
    }
  }
  return requests;
}

void RemoteDisplayRequestDecoder::reset()
{
  collecting_ = false;
  field_ = Field::kFilePath;
  filePath_.clear();
  macros_.clear();
  geometry_.clear();
}
