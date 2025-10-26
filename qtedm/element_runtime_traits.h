// Element Runtime Interface Traits
//
// This file documents the compile-time interface requirements for element
// types used with runtime template base classes. These are not enforced at
// compile time but serve as documentation and can be validated with concepts
// in C++20.
//
// Element classes must provide these methods to work with their corresponding
// runtime base classes. The traits rely on compile-time duck typing - if an
// element provides the required methods, it will compile successfully.

#ifndef ELEMENT_RUNTIME_TRAITS_H
#define ELEMENT_RUNTIME_TRAITS_H

#include <QString>

namespace ElementTraits {

//=============================================================================
// RuntimeVisibilityInterface
//
// Required methods for elements that support runtime visibility control
//=============================================================================
//
// Required methods:
//   int visibilityMode() const
//     Returns: VisibilityMode enum value
//     Purpose: Determines how visibility is calculated
//
//   QString visibilityCalc() const
//     Returns: Calc expression string for visibility evaluation
//     Purpose: Used when visibilityMode() == kVisibilityCalc
//
//   void setRuntimeVisible(bool visible)
//     Parameters: visible - true to show, false to hide
//     Purpose: Updates element visibility based on runtime state
//
// Example element declaration:
//   class MyElement {
//   public:
//     int visibilityMode() const { return visibilityMode_; }
//     QString visibilityCalc() const { return visibilityCalc_; }
//     void setRuntimeVisible(bool visible);
//   };

//=============================================================================
// RuntimeChannelInterface
//
// Required methods for elements that connect to EPICS channels
//=============================================================================
//
// For multi-channel elements (graphic elements):
//   QString channel(int index) const
//     Parameters: index - channel index (0-based)
//     Returns: Channel name (PV name)
//     Purpose: Provides channel name for CA connection
//
// For single-channel elements (monitor elements):
//   QString channel() const
//     Returns: Channel name (PV name)
//     Purpose: Provides channel name for CA connection
//
// Common methods:
//   void setRuntimeConnected(bool connected)
//     Parameters: connected - true if any channel is connected
//     Purpose: Updates element's connection state indicator
//
//   void setRuntimeSeverity(short severity)
//     Parameters: severity - EPICS alarm severity (0=OK, 1=MINOR, 2=MAJOR, 3=INVALID)
//     Purpose: Updates element's alarm severity indicator
//
// Example multi-channel element:
//   class MyGraphicElement {
//   public:
//     QString channel(int index) const;
//     void setRuntimeConnected(bool connected);
//     void setRuntimeSeverity(short severity);
//   };
//
// Example single-channel element:
//   class MyMonitorElement {
//   public:
//     QString channel() const;
//     void setRuntimeConnected(bool connected);
//     void setRuntimeSeverity(short severity);
//   };

//=============================================================================
// RuntimeColorInterface
//
// Required methods for elements that support runtime color control
//=============================================================================
//
// Required methods:
//   int colorMode() const
//     Returns: ColorMode enum value
//     Purpose: Determines how color is selected at runtime
//
// Note: Graphic elements handle color internally through their colorMode,
// they do not require a setRuntimeColor method.
//
// Example element declaration:
//   class MyElement {
//   public:
//     int colorMode() const { return colorMode_; }
//   };

//=============================================================================
// RuntimeValueInterface
//
// Required methods for elements that display numeric values
//=============================================================================
//
// Required methods:
//   void setRuntimeValue(double value)
//     Parameters: value - numeric value from EPICS channel
//     Purpose: Updates element display with new value
//
// Example element declaration:
//   class MyElement {
//   public:
//     void setRuntimeValue(double value);
//   };

//=============================================================================
// RuntimeLimitsInterface
//
// Required methods for elements that need control info (limits, precision)
//=============================================================================
//
// Required methods:
//   void setRuntimeLimits(double low, double high)
//     Parameters:
//       low, high - display/control range limits
//     Purpose: Updates element's limits
//
//   void setRuntimePrecision(int precision)
//     Parameters:
//       precision - decimal places for formatting
//     Purpose: Updates element's formatting precision
//
// Example element declaration:
//   class MyElement {
//   public:
//     void setRuntimeLimits(double low, double high);
//     void setRuntimePrecision(int precision);
//   };

//=============================================================================
// GraphicElementInterface
//
// Combined interface for graphic elements (rectangle, oval, arc, etc.)
// Combines: RuntimeVisibilityInterface + RuntimeChannelInterface + RuntimeColorInterface
//=============================================================================
//
// A graphic element must provide all methods from:
//   - RuntimeVisibilityInterface (visibilityMode, visibilityCalc, setRuntimeVisible)
//   - RuntimeChannelInterface (channel, setRuntimeConnected, setRuntimeSeverity)
//   - RuntimeColorInterface (colorMode, setRuntimeColor)
//
// Used by: GraphicElementRuntimeBase<ElementType, ChannelCount>

//=============================================================================
// MonitorElementInterface
//
// Combined interface for monitor elements (bar, meter, scale)
// Combines: RuntimeChannelInterface (single-channel) + RuntimeValueInterface + RuntimeLimitsInterface
//=============================================================================
//
// A monitor element must provide all methods from:
//   - RuntimeChannelInterface single-channel variant (channel(), setRuntimeConnected, setRuntimeSeverity)
//   - RuntimeValueInterface (setRuntimeValue)
//   - RuntimeLimitsInterface (setRuntimeLimits, setRuntimePrecision)
//
// Used by: SingleChannelMonitorRuntimeBase<ElementType>

//=============================================================================
// Trait Validation Helpers (C++17 compatible)
//=============================================================================

// These helpers can be used to check if a type provides required methods.
// They use SFINAE (Substitution Failure Is Not An Error) to detect methods.

template <typename T>
class HasVisibilityInterface {
  template <typename U>
  static auto test(int) -> decltype(
      std::declval<const U>().visibilityMode(),
      std::declval<const U>().visibilityCalc(),
      std::declval<U>().setRuntimeVisible(true),
      std::true_type{});

  template <typename>
  static std::false_type test(...);

public:
  static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T>
class HasChannelInterface {
  template <typename U>
  static auto test(int) -> decltype(
      std::declval<const U>().channel(0),
      std::declval<U>().setRuntimeConnected(true),
      std::declval<U>().setRuntimeSeverity(short{0}),
      std::true_type{});

  template <typename>
  static std::false_type test(...);

public:
  static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T>
class HasSingleChannelInterface {
  template <typename U>
  static auto test(int) -> decltype(
      std::declval<const U>().channel(),
      std::declval<U>().setRuntimeConnected(true),
      std::declval<U>().setRuntimeSeverity(short{0}),
      std::true_type{});

  template <typename>
  static std::false_type test(...);

public:
  static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T>
class HasColorInterface {
  template <typename U>
  static auto test(int) -> decltype(
      std::declval<const U>().colorMode(),
      std::true_type{});

  template <typename>
  static std::false_type test(...);

public:
  static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T>
class HasValueInterface {
  template <typename U>
  static auto test(int) -> decltype(
      std::declval<U>().setRuntimeValue(0.0),
      std::true_type{});

  template <typename>
  static std::false_type test(...);

public:
  static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T>
class HasLimitsInterface {
  template <typename U>
  static auto test(int) -> decltype(
      std::declval<U>().setRuntimeLimits(0.0, 0.0),
      std::declval<U>().setRuntimePrecision(0),
      std::true_type{});

  template <typename>
  static std::false_type test(...);

public:
  static constexpr bool value = decltype(test<T>(0))::value;
};

// Combined trait validators
template <typename T>
constexpr bool isGraphicElement() {
  return HasVisibilityInterface<T>::value &&
         HasChannelInterface<T>::value &&
         HasColorInterface<T>::value;
}

template <typename T>
constexpr bool isMonitorElement() {
  return HasSingleChannelInterface<T>::value &&
         HasValueInterface<T>::value &&
         HasLimitsInterface<T>::value;
}

} // namespace ElementTraits

#endif // ELEMENT_RUNTIME_TRAITS_H
