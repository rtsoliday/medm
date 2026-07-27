#pragma once

#include <QPointer>
#include <QTimer>

#include "heatmap_runtime.h"
#include "ntndarray_image_decoder.h"
#include "pva_ntndarray_source.h"

class NtNdArrayImageElement;
class NtNdArrayDecodeTask;

class NtNdArrayImageRuntime : public HeatmapRuntime
{
  friend class NtNdArrayDecodeTask;
  friend class TestDisplayImportNdArray;
public:
  explicit NtNdArrayImageRuntime(NtNdArrayImageElement *element);
  ~NtNdArrayImageRuntime() override;

  void start() override;
  void stop() override;

private:
  void pollSource();
  void submitFrame(const NtNdArrayFrame &frame);
  void startDecode(const NtNdArrayFrame &frame);
  void decodeFinished(quint64 generation,
      const NtNdArrayDecodedFrame &decoded);
  void updateElementStatus(const QString &error = QString());

  QPointer<NtNdArrayImageElement> imageElement_;
  QTimer pollTimer_;
  PvaNtNdArraySource *source_ = nullptr;
  bool started_ = false;
  bool connected_ = false;
  bool decodeBusy_ = false;
  bool hasPendingFrame_ = false;
  NtNdArrayFrame pendingFrame_;
  quint64 generation_ = 0;
  quint64 droppedFrames_ = 0;
  QString lastError_;
};
