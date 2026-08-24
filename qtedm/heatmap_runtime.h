#pragma once

#include <atomic>
#include <QObject>
#include <QPointer>
#include <QString>

#include "channel_subscription.h"
#include "runtime_utils.h"

class HeatmapElement;

class HeatmapRuntime : public QObject
{
  friend class DisplayWindow;
public:
  /* Interaction hint only. Data producers and runtimes must continue to
   * accept the latest values while a pause is active. */
  class UpdatePause
  {
  public:
    UpdatePause();
    ~UpdatePause();

    UpdatePause(const UpdatePause &) = delete;
    UpdatePause &operator=(const UpdatePause &) = delete;
  };

  explicit HeatmapRuntime(HeatmapElement *element);
  ~HeatmapRuntime() override;

  virtual void start();
  virtual void stop();

  static bool isGlobalUpdatesPaused();

private:
  struct ChannelState
  {
    QString name;
    SubscriptionHandle subscription;
    bool connected = false;
    short fieldType = -1;
    long elementCount = 0;
  };

  void resetRuntimeState();
  void subscribeDataChannel();
  void subscribeDimensionChannel(ChannelState &state, const QString &name);
  void handleDataConnection(bool connected, const SharedChannelData &data);
  void handleDataValue(const SharedChannelData &data);
  void handleDimensionConnection(ChannelState &state, bool connected);
  void handleDimensionValue(ChannelState &state, int value);

  template <typename Func>
  void invokeOnElement(Func &&func);

  QPointer<HeatmapElement> element_;
  ChannelState dataChannel_;
  ChannelState xDimensionChannel_;
  ChannelState yDimensionChannel_;
  bool started_ = false;
  short lastSeverity_ = -1;
  int runtimeXDimension_ = 0;
  int runtimeYDimension_ = 0;

  static void acquireGlobalUpdatePause();
  static void releaseGlobalUpdatePause();

  static std::atomic<unsigned int> globalUpdatePauseCount_;
};

template <typename Func>
inline void HeatmapRuntime::invokeOnElement(Func &&func)
{
  RuntimeUtils::invokeOnObject(element_, std::forward<Func>(func));
}
