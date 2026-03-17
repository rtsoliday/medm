#include "choice_button_runtime.h"

#include <algorithm>

#include <QByteArray>
#include <QDebug>

#include <db_access.h>

#include "audit_logger.h"
#include "channel_access_context.h"
#include "choice_button_element.h"
#include "pv_channel_manager.h"
#include "statistics_tracker.h"

namespace {
constexpr short kInvalidSeverity = 3;
}

ChoiceButtonRuntime::ChoiceButtonRuntime(ChoiceButtonElement *element)
  : QObject(element)
  , element_(element)
{
  if (element_) {
     channelName_ = element_->channel().trimmed();
  }
}

ChoiceButtonRuntime::~ChoiceButtonRuntime()
{
  stop();
}

void ChoiceButtonRuntime::start()
{
  if (started_ || !element_) {
    return;
  }

  const QString initialChannel = element_->channel().trimmed();
  const bool needsCa = parsePvName(initialChannel).protocol == PvProtocol::kCa;
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

  channelName_ = element_->channel().trimmed();
  element_->setActivationCallback([this](int value) {
    handleActivation(value);
  });

  if (channelName_.isEmpty()) {
    return;
  }

  auto &mgr = PvChannelManager::instance();
  subscription_ = mgr.subscribe(
      channelName_,
      DBR_TIME_ENUM,
      1,
      [this](const SharedChannelData &data) { handleChannelData(data); },
      [this](bool connected, const SharedChannelData &) {
        handleChannelConnection(connected);
      },
      [this](bool canRead, bool canWrite) { handleAccessRights(canRead, canWrite); });
}

void ChoiceButtonRuntime::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  StatisticsTracker::instance().registerDisplayObjectStopped();
  subscription_.reset();
  if (element_) {
    element_->setActivationCallback(std::function<void(int)>());
  }
  resetRuntimeState();
}

void ChoiceButtonRuntime::resetRuntimeState()
{
  connected_ = false;
  lastSeverity_ = 0;
  lastValue_ = -1;
  lastReadAccessKnown_ = false;
  lastReadAccess_ = false;
  lastWriteAccess_ = false;
  lastValueOutOfRange_ = false;
  enumStrings_.clear();

  if (element_) {
    invokeOnElement([](ChoiceButtonElement *element) {
      element->setRuntimeConnected(false);
      element->setRuntimeReadAccessKnown(false);
      element->setRuntimeReadAccess(false);
      element->setRuntimeWriteAccess(false);
      element->setRuntimeSeverity(0);
      element->setRuntimeValue(-1);
      element->setRuntimeLabels(QStringList());
    });
  }
}

void ChoiceButtonRuntime::handleChannelConnection(bool connected)
{
  auto &stats = StatisticsTracker::instance();

  if (connected) {
    const bool wasConnected = connected_;
    connected_ = true;
    if (!wasConnected) {
      stats.registerChannelConnected();
    }
    enumStrings_.clear();
    lastValue_ = -1;
    lastValueOutOfRange_ = false;
    if (element_) {
      const bool readAccessKnown = lastReadAccessKnown_;
      const bool readAccess = lastReadAccess_;
      const bool writeAccess = lastWriteAccess_;
      invokeOnElement([readAccessKnown, readAccess, writeAccess](
                          ChoiceButtonElement *element) {
        element->setRuntimeConnected(true);
        element->setRuntimeReadAccessKnown(readAccessKnown);
        element->setRuntimeReadAccess(readAccessKnown && readAccess);
        element->setRuntimeWriteAccess(readAccessKnown && writeAccess);
      });
    }
  } else {
    const bool wasConnected = connected_;
    connected_ = false;
    if (wasConnected) {
      stats.registerChannelDisconnected();
    }
    lastReadAccessKnown_ = false;
    lastReadAccess_ = false;
    lastWriteAccess_ = false;
    lastValueOutOfRange_ = false;
    if (element_) {
      invokeOnElement([](ChoiceButtonElement *element) {
        element->setRuntimeConnected(false);
        element->setRuntimeReadAccessKnown(false);
        element->setRuntimeReadAccess(false);
        element->setRuntimeWriteAccess(false);
        element->setRuntimeSeverity(kInvalidSeverity);
        element->setRuntimeValue(-1);
      });
    }
  }
}

void ChoiceButtonRuntime::handleChannelData(const SharedChannelData &data)
{
  if (!started_) {
    return;
  }

  if (!data.isEnum) {
    return;
  }

  short severity = data.severity;
  short enumValue = static_cast<short>(data.enumValue);

  {
    auto &stats = StatisticsTracker::instance();
    stats.registerCaEvent();
    stats.registerUpdateRequest(true);
    stats.registerUpdateExecuted();
  }

  if (severity != lastSeverity_) {
    lastSeverity_ = severity;
    if (element_) {
      invokeOnElement([severity](ChoiceButtonElement *element) {
        element->setRuntimeSeverity(severity);
      });
    }
  }

  if (!data.enumStrings.isEmpty() && enumStrings_ != data.enumStrings) {
    enumStrings_ = data.enumStrings;
    const QStringList labelsCopy = enumStrings_;
    invokeOnElement([labelsCopy](ChoiceButtonElement *element) {
      element->setRuntimeLabels(labelsCopy);
    });
  }

  const int stateCount = enumStrings_.size();
  const bool valueOutOfRange = stateCount > 0
      && (enumValue < 0 || enumValue >= stateCount);

  if (valueOutOfRange
      && (!lastValueOutOfRange_ || enumValue != lastValue_)) {
    qWarning() << "Choice Button PV" << channelName_
               << "reported enum value" << enumValue
               << "outside available state range 0-" << (stateCount - 1);
  }

  if (!valueOutOfRange && (enumValue != lastValue_ || lastValueOutOfRange_)) {
    lastValue_ = enumValue;
    lastValueOutOfRange_ = false;
    if (element_) {
      invokeOnElement([enumValue](ChoiceButtonElement *element) {
        element->setRuntimeValue(enumValue);
      });
    }
    return;
  }

  if (valueOutOfRange) {
    lastValue_ = enumValue;
    lastValueOutOfRange_ = true;
  }
}

void ChoiceButtonRuntime::handleAccessRights(bool canRead, bool canWrite)
{
  if (!started_) {
    return;
  }
  if (lastReadAccessKnown_ && canRead == lastReadAccess_
      && canWrite == lastWriteAccess_) {
    return;
  }
  lastReadAccessKnown_ = true;
  lastReadAccess_ = canRead;
  lastWriteAccess_ = canWrite;
  if (element_) {
    invokeOnElement([canRead, canWrite](ChoiceButtonElement *element) {
      element->setRuntimeReadAccessKnown(true);
      element->setRuntimeReadAccess(canRead);
      element->setRuntimeWriteAccess(canWrite);
      if (!canRead) {
        element->setRuntimeValue(-1);
        element->setRuntimeLabels(QStringList());
      }
    });
  }
  if (!canRead) {
    enumStrings_.clear();
    lastValue_ = -1;
    lastValueOutOfRange_ = false;
  }
}

void ChoiceButtonRuntime::handleActivation(int value)
{
  if (!started_ || !connected_ || !lastWriteAccess_) {
    return;
  }
  if (value < 0) {
    return;
  }

  dbr_enum_t toSend = static_cast<dbr_enum_t>(value);
  if (!PvChannelManager::instance().putValue(channelName_, toSend)) {
    qWarning() << "Failed to write choice button value" << value << "to"
               << channelName_;
    return;
  }
  AuditLogger::instance().logPut(channelName_, value,
      QStringLiteral("ChoiceButton"));
}
