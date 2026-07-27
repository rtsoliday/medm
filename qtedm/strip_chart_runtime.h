#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

#include <array>
#include <utility>

#include "strip_chart_properties.h"
#include "archive_provider.h"
#include "channel_subscription.h"
#include "runtime_utils.h"

class StripChartElement;

class DisplayWindow;

class StripChartRuntime : public QObject
{
  friend class DisplayWindow;
public:
  explicit StripChartRuntime(StripChartElement *element);
  ~StripChartRuntime() override;

  void start();
  void stop();

private:
  struct PenState
  {
    QString channelName;
    SubscriptionHandle subscription;
    bool connected = false;
    bool readAccessKnown = false;
    bool canRead = false;
    short fieldType = -1;
    long elementCount = 1;
    QPointer<ArchiveRequest> archiveRequest;
    QVector<ArchiveSample> historicalSamples;
    QVector<ArchiveSample> liveSamples;
    quint64 archiveGeneration = 0;
    bool archiveComplete = false;
    bool archiveFailed = false;
  };

  void subscribePen(int index);
  void requestArchive(int index);
  void applyMergedHistory(int index);
  void handleAccessRightsEvent(int index, bool canRead, bool canWrite);
  void handleConnectionEvent(int index, bool connected,
      const SharedChannelData &data);
  void handleValueEvent(int index, const SharedChannelData &data);
  void resetPen(int index);
  void unsubscribePen(int index);

  template <typename Func>
  void invokeOnElement(Func &&func);

  QPointer<StripChartElement> element_;
  std::array<PenState, kStripChartPenCount> pens_{};
  ArchiveProvider *archiveProvider_ = nullptr;
  QString archiveProviderError_;
  bool started_ = false;
};

template <typename Func>
inline void StripChartRuntime::invokeOnElement(Func &&func)
{
  RuntimeUtils::invokeOnObject(element_, std::forward<Func>(func));
}
