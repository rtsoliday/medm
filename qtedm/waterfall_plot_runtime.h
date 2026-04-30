#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QVector>

#include <utility>

#include "waterfall_plot_properties.h"
#include "channel_subscription.h"

class WaterfallPlotElement;

class WaterfallPlotRuntime : public QObject
{
  friend class DisplayWindow;
public:
  explicit WaterfallPlotRuntime(WaterfallPlotElement *element);
  ~WaterfallPlotRuntime() override;

  void start();
  void stop();

private:
  enum class ChannelKind
  {
    kData,
    kCount,
    kTrigger,
    kErase,
  };

  struct ChannelState
  {
    QString name;
    SubscriptionHandle subscription;
    bool connected = false;
    short fieldType = -1;
    long elementCount = 0;
  };

  void resetState();
  void subscribeChannel(ChannelState &state, ChannelKind kind,
      chtype type, long elementCount);
  void handleConnection(ChannelState &state, ChannelKind kind, bool connected,
      const SharedChannelData &data);
  void handleValue(ChannelKind kind, const SharedChannelData &data);
  void handleDataValue(const SharedChannelData &data);
  void handleCountValue(const SharedChannelData &data);
  void handleTriggerValue(const SharedChannelData &data);
  void handleEraseValue(const SharedChannelData &data);
  void pushLatestWaveform();
  void scheduleElementUpdate();
  void flushElementUpdate();

  template <typename Func>
  void invokeOnElement(Func &&func);

  QPointer<WaterfallPlotElement> element_;
  ChannelState dataChannel_;
  ChannelState countChannel_;
  ChannelState triggerChannel_;
  ChannelState eraseChannel_;
  bool started_ = false;
  QVector<double> latestWaveform_;
  qint64 latestWaveformTimestampMs_ = 0;
  int countFromChannel_ = 0;
  WaterfallEraseMode eraseMode_ = WaterfallEraseMode::kIfNotZero;
  bool eraseValueKnown_ = false;
  double lastEraseValue_ = 0.0;
  QTimer repaintTimer_;
  bool repaintPending_ = false;
};

template <typename Func>
inline void WaterfallPlotRuntime::invokeOnElement(Func &&func)
{
  if (!element_) {
    return;
  }
  QPointer<WaterfallPlotElement> target = element_;
  QMetaObject::invokeMethod(element_.data(),
      [target, func = std::forward<Func>(func)]() mutable {
        if (!target) {
          return;
        }
        func(target.data());
      },
      Qt::QueuedConnection);
}
