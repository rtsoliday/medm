#pragma once

#include <array>

#include <QObject>
#include <QString>

#include "channel_subscription.h"

class ExpressionChannelElement;

class ExpressionChannelRuntime : public QObject
{
  friend class DisplayWindow;

public:
  explicit ExpressionChannelRuntime(ExpressionChannelElement *element);
  ~ExpressionChannelRuntime() override;

  void start();
  void stop();

private:
  void handleChannelData(int index, const SharedChannelData &data);
  void handleChannelConnection(int index, bool connected);
  void evaluateAndMaybePublish();

  ExpressionChannelElement *element_ = nullptr;
  std::array<SubscriptionHandle, 4> subscriptions_{};
  std::array<double, 4> values_{{0.0, 0.0, 0.0, 0.0}};
  std::array<bool, 4> connected_{{false, false, false, false}};
  QString outputName_;
  std::array<char, 300> postfix_{};
  bool postfixValid_ = false;
  bool started_ = false;
  bool firstEvaluationPublished_ = false;
  bool hasLastPublishedResult_ = false;
  double lastPublishedResult_ = 0.0;
  bool hasLastEvaluatedResult_ = false;
  double lastEvaluatedResult_ = 0.0;
};
