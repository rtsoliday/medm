#pragma once

#include <array>

#include <QByteArray>
#include <QObject>
#include <QString>

#include "pv_channel_manager.h"

class CompositeElement;

class DisplayWindow;

class CompositeRuntime : public QObject
{
  friend class DisplayWindow;
public:
  explicit CompositeRuntime(CompositeElement *element);
  ~CompositeRuntime() override;

  void start();
  void stop();

private:
  /* Channel state tracked per subscription */
  struct ChannelState
  {
    QString name;
    SubscriptionHandle subscription;
    bool connected = false;
    bool hasValue = false;
    double value = 0.0;
    short severity = 0;
  };

  void resetState();
  void initializeChannels();
  void cleanupChannels();
  void handleChannelConnection(int index, bool connected);
  void handleChannelValue(int index, const SharedChannelData &data);
  void evaluateVisibility();
  bool evaluateCalcExpression(double &result) const;

  CompositeElement *element_ = nullptr;
  std::array<ChannelState, 5> channels_{};
  QByteArray calcPostfix_;
  bool calcValid_ = false;
  bool started_ = false;
};
