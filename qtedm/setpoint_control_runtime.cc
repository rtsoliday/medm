#include "setpoint_control_runtime.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <QDebug>

#include <cvtFast.h>
#include <db_access.h>

#include "audit_logger.h"
#include "channel_access_context.h"
#include "runtime_utils.h"
#include "setpoint_control_element.h"
#include "statistics_tracker.h"
#include "text_format_utils.h"

namespace {
using RuntimeUtils::isNumericFieldType;
using RuntimeUtils::kInvalidSeverity;
using TextFormatUtils::formatHex;
using TextFormatUtils::formatOctal;
using TextFormatUtils::kMaxTextField;
using TextFormatUtils::localCvtDoubleToExpNotationString;

} // namespace

SetpointControlRuntime::SetpointControlRuntime(SetpointControlElement *element)
  : QObject(element)
  , element_(element)
{
  if (element_) {
    setpointChannel_ = element_->setpointChannel().trimmed();
    readbackChannel_ = element_->readbackChannel().trimmed();
  }
}

SetpointControlRuntime::~SetpointControlRuntime()
{
  stop();
}

void SetpointControlRuntime::start()
{
  if (started_ || !element_) {
    return;
  }

  setpointChannel_ = element_->setpointChannel().trimmed();
  readbackChannel_ = element_->readbackChannel().trimmed();

  const bool needsCa = parsePvName(setpointChannel_).protocol == PvProtocol::kCa
      || parsePvName(readbackChannel_).protocol == PvProtocol::kCa;
  if (needsCa) {
    ChannelAccessContext &context = ChannelAccessContext::instance();
    context.ensureInitializedForProtocol(PvProtocol::kCa);
    if (!context.isInitialized()) {
      qWarning() << "Channel Access context not available";
      return;
    }
  }

  resetRuntimeState();
  started_ = true;
  StatisticsTracker::instance().registerDisplayObjectStarted();

  element_->setActivationCallback([this](const QString &text) {
    handleActivation(text);
  });

  auto &mgr = PvChannelManager::instance();
  if (!setpointChannel_.isEmpty()) {
    setpointSubscription_ = mgr.subscribe(
        setpointChannel_,
        DBR_TIME_DOUBLE,
        1,
        [this](const SharedChannelData &data) { handleSetpointData(data); },
        [this](bool connected, const SharedChannelData &data) {
          handleSetpointConnection(connected, data);
        },
        [this](bool canRead, bool canWrite) {
          handleAccessRights(canRead, canWrite);
        });
  }

  if (!readbackChannel_.isEmpty()) {
    readbackSubscription_ = mgr.subscribe(
        readbackChannel_,
        DBR_TIME_DOUBLE,
        1,
        [this](const SharedChannelData &data) { handleReadbackData(data); },
        [this](bool connected, const SharedChannelData &data) {
          handleReadbackConnection(connected, data);
        });
  }
}

void SetpointControlRuntime::stop()
{
  if (!started_) {
    return;
  }
  started_ = false;
  StatisticsTracker::instance().registerDisplayObjectStopped();
  setpointSubscription_.reset();
  readbackSubscription_.reset();
  if (element_) {
    element_->setActivationCallback(std::function<void(const QString &)>());
  }
  resetRuntimeState();
}

void SetpointControlRuntime::resetRuntimeState()
{
  setpointConnected_ = false;
  readbackConnected_ = false;
  setpointNumeric_ = false;
  readbackNumeric_ = false;
  writeAccess_ = false;
  channelPrecision_ = -1;
  controlLow_ = 0.0;
  controlHigh_ = 1.0;
  hasControlLimits_ = false;

  invokeOnElement([](SetpointControlElement *element) {
    element->clearRuntimeState();
  });
}

void SetpointControlRuntime::handleSetpointConnection(bool connected,
    const SharedChannelData &data)
{
  if (!started_) {
    return;
  }
  if (connected) {
    setpointConnected_ = true;
    setpointNumeric_ = isSupportedNumeric(data);
    if (!setpointNumeric_) {
      invokeOnElement([](SetpointControlElement *element) {
        element->setSetpointConnected(false);
        element->setRuntimeNotice(QStringLiteral("Setpoint PV is not numeric"));
      });
      return;
    }
    updateMetadataFromSetpoint(data);
    invokeOnElement([](SetpointControlElement *element) {
      element->setSetpointConnected(true);
    });
  } else {
    setpointConnected_ = false;
    setpointNumeric_ = false;
    writeAccess_ = false;
    invokeOnElement([](SetpointControlElement *element) {
      element->setSetpointConnected(false);
      element->setSetpointWriteAccess(false);
      element->setSetpointSeverity(kInvalidSeverity);
    });
  }
}

void SetpointControlRuntime::handleSetpointData(const SharedChannelData &data)
{
  if (!started_ || !data.isNumeric) {
    return;
  }
  setpointNumeric_ = true;
  updateMetadataFromSetpoint(data);
  const int precision = resolvedPrecision();
  const double value = data.numericValue;
  const QString text = formatNumeric(value, precision);
  const short severity = data.severity;

  auto &stats = StatisticsTracker::instance();
  stats.registerCaEvent();
  stats.registerUpdateRequest(true);
  stats.registerUpdateExecuted();

  invokeOnElement([value, text, severity](SetpointControlElement *element) {
    element->setSetpointConnected(true);
    element->setSetpointSeverity(severity);
    element->setSetpointValue(value, text);
  });
}

void SetpointControlRuntime::handleReadbackConnection(bool connected,
    const SharedChannelData &data)
{
  if (!started_) {
    return;
  }
  if (connected) {
    readbackConnected_ = true;
    readbackNumeric_ = isSupportedNumeric(data);
    invokeOnElement([numeric = readbackNumeric_](SetpointControlElement *element) {
      element->setReadbackConnected(numeric);
      if (!numeric) {
        element->setRuntimeNotice(QStringLiteral("Readback PV is not numeric"));
      }
    });
  } else {
    readbackConnected_ = false;
    readbackNumeric_ = false;
    invokeOnElement([](SetpointControlElement *element) {
      element->setReadbackConnected(false);
      element->setReadbackSeverity(kInvalidSeverity);
    });
  }
}

void SetpointControlRuntime::handleReadbackData(const SharedChannelData &data)
{
  if (!started_ || !data.isNumeric) {
    return;
  }
  readbackNumeric_ = true;
  const bool useReadbackChannelPrecision = element_
      && element_->precisionSource() == PvLimitSource::kChannel
      && data.hasPrecision;
  const int precision = useReadbackChannelPrecision
      ? std::clamp<int>(data.precision, 0, 17)
      : resolvedPrecision();
  const double value = data.numericValue;
  const QString text = formatNumeric(value, precision);
  const short severity = data.severity;

  auto &stats = StatisticsTracker::instance();
  stats.registerCaEvent();
  stats.registerUpdateRequest(true);
  stats.registerUpdateExecuted();

  invokeOnElement([value, text, severity](SetpointControlElement *element) {
    element->setReadbackConnected(true);
    element->setReadbackSeverity(severity);
    element->setReadbackValue(value, text);
  });
}

void SetpointControlRuntime::handleAccessRights(bool /*canRead*/, bool canWrite)
{
  if (!started_) {
    return;
  }
  writeAccess_ = canWrite;
  invokeOnElement([canWrite](SetpointControlElement *element) {
    element->setSetpointWriteAccess(canWrite);
  });
}

void SetpointControlRuntime::handleActivation(const QString &text)
{
  if (!started_ || !setpointConnected_ || !writeAccess_ || !setpointNumeric_) {
    return;
  }

  double value = 0.0;
  if (!parseNumericInput(text, value)) {
    invokeOnElement([](SetpointControlElement *element) {
      element->setRuntimeNotice(QStringLiteral("Invalid numeric input"));
    });
    return;
  }

  QString message;
  if (!validateAgainstLimits(value, message)) {
    invokeOnElement([message](SetpointControlElement *element) {
      element->setRuntimeNotice(message);
    });
    return;
  }

  if (!PvChannelManager::instance().putValue(setpointChannel_, value)) {
    invokeOnElement([](SetpointControlElement *element) {
      element->setRuntimeNotice(QStringLiteral("Write failed"));
    });
    return;
  }

  AuditLogger::instance().logPut(setpointChannel_, value,
      QStringLiteral("SetpointControl"));
  const QString displayText = formatNumeric(value, resolvedPrecision());
  invokeOnElement([value, displayText](SetpointControlElement *element) {
    element->acceptAppliedValue(value, displayText);
  });
}

void SetpointControlRuntime::updateMetadataFromSetpoint(
    const SharedChannelData &data)
{
  if (data.hasPrecision) {
    channelPrecision_ = data.precision;
  }
  if (data.hasControlInfo) {
    controlLow_ = data.lopr;
    controlHigh_ = data.hopr;
    hasControlLimits_ = std::isfinite(controlLow_)
        && std::isfinite(controlHigh_);
    const double low = controlLow_;
    const double high = controlHigh_;
    const int precision = data.hasPrecision ? data.precision : resolvedPrecision();
    const QString units = data.hasUnits ? data.units : QString();
    invokeOnElement([low, high, precision, units](SetpointControlElement *element) {
      element->setSetpointMetadata(low, high, precision, units);
    });
  }
}

int SetpointControlRuntime::resolvedPrecision() const
{
  if (!element_) {
    return 0;
  }
  if (element_->precisionSource() == PvLimitSource::kChannel
      && channelPrecision_ >= 0) {
    return std::clamp(channelPrecision_, 0, 17);
  }
  return std::clamp(element_->precisionDefault(), 0, 17);
}

QString SetpointControlRuntime::formatNumeric(double value, int precision) const
{
  if (!element_) {
    return QString();
  }

  unsigned short epicsPrecision = static_cast<unsigned short>(
      std::clamp(precision, 0, 17));
  char buffer[kMaxTextField];
  buffer[0] = '\0';

  switch (element_->format()) {
  case TextMonitorFormat::kExponential:
    std::snprintf(buffer, sizeof(buffer), "%.*e", epicsPrecision, value);
    break;
  case TextMonitorFormat::kEngineering:
    localCvtDoubleToExpNotationString(value, buffer, epicsPrecision);
    break;
  case TextMonitorFormat::kCompact:
    cvtDoubleToCompactString(value, buffer, epicsPrecision);
    break;
  case TextMonitorFormat::kTruncated:
    cvtLongToString(static_cast<long>(value), buffer);
    break;
  case TextMonitorFormat::kHexadecimal:
    return formatHex(static_cast<long>(std::llround(value)));
  case TextMonitorFormat::kOctal:
    return formatOctal(static_cast<long>(std::llround(value)));
  case TextMonitorFormat::kDecimal:
  case TextMonitorFormat::kString:
  case TextMonitorFormat::kSexagesimal:
  case TextMonitorFormat::kSexagesimalHms:
  case TextMonitorFormat::kSexagesimalDms:
  default:
    cvtDoubleToString(value, buffer, epicsPrecision);
    break;
  }

  return QString::fromLatin1(buffer);
}

bool SetpointControlRuntime::parseNumericInput(const QString &text,
    double &value) const
{
  const QString trimmed = text.trimmed();
  if (trimmed.isEmpty()) {
    return false;
  }

  bool ok = false;
  const TextMonitorFormat format = element_ ? element_->format()
                                            : TextMonitorFormat::kDecimal;
  switch (format) {
  case TextMonitorFormat::kHexadecimal:
    value = static_cast<double>(trimmed.toLongLong(&ok, 16));
    return ok;
  case TextMonitorFormat::kOctal:
    value = static_cast<double>(trimmed.toLongLong(&ok, 8));
    return ok;
  default:
    break;
  }

  value = trimmed.toDouble(&ok);
  if (ok) {
    return true;
  }
  if (trimmed.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
    value = static_cast<double>(trimmed.mid(2).toLongLong(&ok, 16));
    return ok;
  }
  return false;
}

bool SetpointControlRuntime::validateAgainstLimits(double value,
    QString &message) const
{
  if (!element_) {
    return true;
  }
  const PvLimits &limits = element_->limits();
  const bool hasLow = limits.lowSource != PvLimitSource::kChannel
      || hasControlLimits_;
  const bool hasHigh = limits.highSource != PvLimitSource::kChannel
      || hasControlLimits_;
  const double low = element_->displayLowLimit();
  const double high = element_->displayHighLimit();
  if (hasLow && std::isfinite(low) && value < low) {
    message = QStringLiteral("Below low limit");
    return false;
  }
  if (hasHigh && std::isfinite(high) && value > high) {
    message = QStringLiteral("Above high limit");
    return false;
  }
  return true;
}

bool SetpointControlRuntime::isSupportedNumeric(
    const SharedChannelData &data) const
{
  return isNumericFieldType(data.nativeFieldType)
      && data.nativeElementCount <= 1;
}
