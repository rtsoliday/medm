#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <type_traits>

#include <QByteArray>
#include <QObject>
#include <QString>

#include <cadef.h>

#include "display_properties.h"
#include "element_runtime_traits.h"

class DisplayWindow;

/* Base class template for graphic element runtimes that support dynamic
 * visibility and color through EPICS Channel Access.
 *
 * This class consolidates common runtime behavior shared across geometric
 * graphic elements (rectangle, oval, arc, line, polygon, polyline) to
 * eliminate code duplication while preserving type safety.
 *
 * Template Parameters:
 *   ElementType: The element widget class (e.g., RectangleElement)
 *   ChannelCount: Number of channels to support (default 5)
 *
 * Element Type Requirements:
 *   The ElementType must provide these methods:
 *   - QString channel(int index) const
 *   - TextColorMode colorMode() const
 *   - TextVisibilityMode visibilityMode() const
 *   - QString visibilityCalc() const
 *   - void setRuntimeConnected(bool connected)
 *   - void setRuntimeSeverity(short severity)
 *   - void setRuntimeVisible(bool visible)
 */
template <typename ElementType, size_t ChannelCount = 5>
class GraphicElementRuntimeBase : public QObject
{
  friend class DisplayWindow;

  // Compile-time interface validation
  // TODO: Re-enable after debugging trait detection
  /*
  static_assert(ElementTraits::HasVisibilityInterface<ElementType>::value,
                "ElementType must provide RuntimeVisibilityInterface methods");
  static_assert(ElementTraits::HasChannelInterface<ElementType>::value,
                "ElementType must provide RuntimeChannelInterface methods");
  static_assert(ElementTraits::HasColorInterface<ElementType>::value,
                "ElementType must provide RuntimeColorInterface methods");
  */

public:
  explicit GraphicElementRuntimeBase(ElementType *element);
  ~GraphicElementRuntimeBase() override;

  void start();
  void stop();

protected:
  /* Channel runtime structure - tracks state for each EPICS channel */
  struct ChannelRuntime
  {
    GraphicElementRuntimeBase *owner = nullptr;
    int index = 0;
    QString name;
    chid channelId = nullptr;
    evid subscriptionId = nullptr;
    chtype subscriptionType = DBR_TIME_DOUBLE;
    short fieldType = -1;
    long elementCount = 1;
    bool connected = false;
    bool hasValue = false;
    bool controlInfoRequested = false;
    bool hasControlInfo = false;
    double value = 0.0;
    short severity = 0;
    short status = 0;
    double hopr = 0.0;
    double lopr = 0.0;
    short precision = -1;
  };

  ElementType *element() { return element_; }
  const ElementType *element() const { return element_; }

  const std::array<ChannelRuntime, ChannelCount> &channels() const
  {
    return channels_;
  }

private:

  void resetState();
  void initializeChannels();
  void cleanupChannels();
  void subscribeChannel(ChannelRuntime &channel);
  void unsubscribeChannel(ChannelRuntime &channel);
  void requestControlInfo(ChannelRuntime &channel);
  void handleChannelConnection(ChannelRuntime &channel,
      const connection_handler_args &args);
  void handleChannelValue(ChannelRuntime &channel,
      const event_handler_args &args);
  void handleChannelControlInfo(ChannelRuntime &channel,
      const event_handler_args &args);
  void evaluateState();
  bool evaluateCalcExpression(double &result) const;

  static void channelConnectionCallback(struct connection_handler_args args);
  static void valueEventCallback(struct event_handler_args args);
  static void controlInfoCallback(struct event_handler_args args);

  ElementType *element_ = nullptr;
  std::array<ChannelRuntime, ChannelCount> channels_{};
  QByteArray calcPostfix_;
  bool calcValid_ = false;
  bool channelsNeeded_ = true;
  bool started_ = false;
};

