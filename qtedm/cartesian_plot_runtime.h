#pragma once

#include <QObject>
#include <QPointer>
#include <QVector>
#include <QString>
#include <QPointF>

#include <array>

#include <cadef.h>

#ifndef MEDM_CARTESIAN_PLOT_DEBUG
#  define MEDM_CARTESIAN_PLOT_DEBUG 0
#endif

#include "display_properties.h"

class CartesianPlotElement;

class DisplayWindow;

class CartesianPlotRuntime : public QObject
{
  friend class DisplayWindow;
public:
  explicit CartesianPlotRuntime(CartesianPlotElement *element);
  ~CartesianPlotRuntime() override;

  void start();
  void stop();

private:
  enum class ChannelKind
  {
    kTraceX,
    kTraceY,
    kTrigger,
    kErase,
    kCount,
  };

  struct ChannelState
  {
    QString name;
    chid channelId = nullptr;
    evid subscriptionId = nullptr;
    bool connected = false;
    short fieldType = -1;
    long elementCount = 0;
  };

  struct TraceState
  {
    ChannelState x;
    ChannelState y;
    CartesianPlotTraceMode mode = CartesianPlotTraceMode::kNone;
    QVector<QPointF> scalarPoints;
    QVector<double> xScalarValues;
    QVector<double> yScalarValues;
    QVector<double> xVector;
    QVector<double> yVector;
    QVector<QPointF> vectorPoints;
    double lastXScalar = 0.0;
    double lastYScalar = 0.0;
    bool hasXScalar = false;
    bool hasYScalar = false;
    bool pendingTrigger = false;
    int yAxisIndex = 1;
  };

  struct ChannelContext
  {
    CartesianPlotRuntime *runtime = nullptr;
    int traceIndex = -1;
    ChannelKind kind = ChannelKind::kTraceX;
  };

  void resetState();
  void createTraceChannel(int index, ChannelKind kind,
      ChannelState &state, ChannelContext &context);
  void createAuxiliaryChannel(ChannelKind kind, ChannelState &state,
      ChannelContext &context);
  void subscribeChannel(ChannelState &state, ChannelContext &context);
  void unsubscribeChannel(ChannelState &state);
  void handleConnection(const ChannelContext &context,
      const connection_handler_args &args);
  void handleValue(const ChannelContext &context,
      const event_handler_args &args);
  void handleControlInfo(const ChannelContext &context,
      const event_handler_args &args);

  void handleTraceConnection(int index, bool isX,
      const connection_handler_args &args);
  void handleTraceValue(int index, bool isX,
      const event_handler_args &args);
  void handleTraceControlInfo(int index, bool isX,
      const event_handler_args &args);
  void handleTriggerValue(const event_handler_args &args);
  void handleEraseValue(const event_handler_args &args);
  void handleCountValue(const event_handler_args &args);

  void updateTraceMode(int index);
  void clearTraceData(int index, bool notifyElement);
  void emitTraceData(int index, const QVector<QPointF> &points,
      CartesianPlotTraceMode mode, bool connected);
  void processTraceUpdate(int index, bool forceAppend);
  void appendXYScalarPoint(TraceState &trace);
  void appendXScalarPoint(TraceState &trace);
  void appendYScalarPoint(TraceState &trace);
  void rebuildVectorPoints(TraceState &trace);

  QVector<QPointF> buildPointsForMode(const TraceState &trace) const;
  QVector<QPointF> buildXYScalarPoints(const TraceState &trace) const;
  QVector<QPointF> buildXScalarPoints(const TraceState &trace) const;
  QVector<QPointF> buildYScalarPoints(const TraceState &trace) const;
  QVector<QPointF> buildVectorPoints(const TraceState &trace) const;

  int effectiveCapacity(int preferredCount = 0,
      bool allowConfiguredCount = true) const;
  bool traceConnected(const TraceState &trace) const;
  bool isTriggerEnabled() const;

  template <typename Func>
  void invokeOnElement(Func &&func);

  void logConfiguredAxisState();

  static QVector<double> extractValues(const event_handler_args &args);

  static void channelConnectionCallback(struct connection_handler_args args);
  static void valueEventCallback(struct event_handler_args args);
  static void controlInfoCallback(struct event_handler_args args);

  QPointer<CartesianPlotElement> element_;
  std::array<TraceState, kCartesianPlotTraceCount> traces_{};
  ChannelState triggerChannel_;
  ChannelState eraseChannel_;
  ChannelState countChannel_;
  std::array<ChannelContext, kCartesianPlotTraceCount> xContexts_{};
  std::array<ChannelContext, kCartesianPlotTraceCount> yContexts_{};
  ChannelContext triggerContext_{};
  ChannelContext eraseContext_{};
  ChannelContext countContext_{};
  bool started_ = false;
  bool eraseOldest_ = false;
  CartesianPlotEraseMode eraseMode_ = CartesianPlotEraseMode::kIfNotZero;
  int configuredCount_ = 1;
  int countFromChannel_ = 0;
  bool configuredAxesLogged_ = false;
};

template <typename Func>
inline void CartesianPlotRuntime::invokeOnElement(Func &&func)
{
  if (!element_) {
    return;
  }
  QPointer<CartesianPlotElement> target = element_;
  QMetaObject::invokeMethod(element_.data(),
      [target, func = std::forward<Func>(func)]() mutable {
        if (!target) {
          return;
        }
        func(target.data());
      },
      Qt::QueuedConnection);
}
