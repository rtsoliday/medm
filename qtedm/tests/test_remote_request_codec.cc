#include <QtTest/QtTest>

#include "remote_request_codec.h"

class TestRemoteRequestCodec : public QObject
{
  Q_OBJECT

private slots:
  void encodesAndDecodesAcrossFixedSizeMessages();
  void decoderIgnoresNoiseAndHandlesMultipleRequests();
  void rejectsInvalidChunkSizesAndResetsPartialState();
};

void TestRemoteRequestCodec::encodesAndDecodesAcrossFixedSizeMessages()
{
  const QByteArray encoded = encodeRemoteDisplayRequest(
      QStringLiteral("/tmp/a long display.adl"),
      QStringLiteral("A=1,B=two"), QStringLiteral("800x600+10+20"));
  QVERIFY(encoded.startsWith('('));
  QVERIFY(encoded.endsWith(')'));

  const QVector<QByteArray> chunks = chunkRemoteDisplayRequest(encoded, 20);
  QVERIFY(chunks.size() > 1);
  RemoteDisplayRequestDecoder decoder;
  QVector<RemoteDisplayRequest> decoded;
  for (const QByteArray &chunk : chunks) {
    QCOMPARE(chunk.size(), 20);
    decoded += decoder.consume(chunk);
  }
  QCOMPARE(decoded.size(), 1);
  QCOMPARE(decoded.front().filePath,
      QStringLiteral("/tmp/a long display.adl"));
  QCOMPARE(decoded.front().macros, QStringLiteral("A=1,B=two"));
  QCOMPARE(decoded.front().geometry, QStringLiteral("800x600+10+20"));
}

void TestRemoteRequestCodec::decoderIgnoresNoiseAndHandlesMultipleRequests()
{
  RemoteDisplayRequestDecoder decoder;
  QByteArray bytes("ignored");
  bytes += encodeRemoteDisplayRequest(QStringLiteral("first.adl"),
      QString(), QStringLiteral("+1+2"));
  bytes += QByteArray("padding");
  bytes += encodeRemoteDisplayRequest(QStringLiteral("second.adl"),
      QStringLiteral("P=3"), QString());
  const QVector<RemoteDisplayRequest> decoded = decoder.consume(bytes);
  QCOMPARE(decoded.size(), 2);
  QCOMPARE(decoded.at(0).filePath, QStringLiteral("first.adl"));
  QCOMPARE(decoded.at(0).geometry, QStringLiteral("+1+2"));
  QCOMPARE(decoded.at(1).filePath, QStringLiteral("second.adl"));
  QCOMPARE(decoded.at(1).macros, QStringLiteral("P=3"));
}

void TestRemoteRequestCodec::rejectsInvalidChunkSizesAndResetsPartialState()
{
  const QByteArray encoded = encodeRemoteDisplayRequest(
      QStringLiteral("partial.adl"), QString(), QString());
  QVERIFY(chunkRemoteDisplayRequest(encoded, 0).isEmpty());
  QVERIFY(chunkRemoteDisplayRequest(QByteArray(), 20).isEmpty());

  RemoteDisplayRequestDecoder decoder;
  QVERIFY(decoder.consume(encoded.left(5)).isEmpty());
  decoder.reset();
  QVERIFY(decoder.consume(encoded.mid(5)).isEmpty());
  const QVector<RemoteDisplayRequest> decoded = decoder.consume(encoded);
  QCOMPARE(decoded.size(), 1);
  QCOMPARE(decoded.front().filePath, QStringLiteral("partial.adl"));
}

QTEST_MAIN(TestRemoteRequestCodec)

#include "test_remote_request_codec.moc"
