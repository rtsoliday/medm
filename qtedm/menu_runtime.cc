#include "menu_runtime.h"

#include <algorithm>
#include <functional>

#include <QByteArray>
#include <QDebug>

#include <db_access.h>

#include "audit_logger.h"
#include "channel_access_context.h"
#include "menu_element.h"
#include "pv_channel_manager.h"
#include "statistics_tracker.h"

namespace {
constexpr short kInvalidSeverity = 3;
}

MenuRuntime::MenuRuntime(MenuElement *element)
  : QObject(element)
  , element_(element)
{
  if (element_) {
    channelName_ = element_->channel().trimmed();
  }
}

MenuRuntime::~MenuRuntime()
{
  stop();
}

void MenuRuntime::start()
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

void MenuRuntime::stop()
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

void MenuRuntime::resetRuntimeState()
{
  connected_ = false;
  lastSeverity_ = 0;
  lastValue_ = -1;
  lastWriteAccess_ = false;
  enumStrings_.clear();

  if (element_) {
    invokeOnElement([](MenuElement *element) {
      element->setRuntimeConnected(false);
      element->setRuntimeWriteAccess(false);
      element->setRuntimeSeverity(0);
      element->setRuntimeValue(-1);
      element->setRuntimeLabels(QStringList());
    });
  }
}

void MenuRuntime::handleChannelConnection(bool connected)
{
  auto &stats = StatisticsTracker::instance();

  if (connected) {
    const bool wasConnected = connected_;
    connected_ = true;
    if (!wasConnected) {
      stats.registerChannelConnected();
    }
    if (element_) {
      invokeOnElement([](MenuElement *element) {
        element->setRuntimeConnected(true);
      });
    }
  } else {
    const bool wasConnected = connected_;
    connected_ = false;
    if (wasConnected) {
      stats.registerChannelDisconnected();
    }
    lastWriteAccess_ = false;
    if (element_) {
      invokeOnElement([](MenuElement *element) {
        element->setRuntimeConnected(false);
        element->setRuntimeWriteAccess(false);
        element->setRuntimeSeverity(kInvalidSeverity);
        element->setRuntimeValue(-1);
      });
    }
  }
}

void MenuRuntime::handleChannelData(const SharedChannelData &data)
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
      invokeOnElement([severity](MenuElement *element) {
        element->setRuntimeSeverity(severity);
      });
    }
  }

  if (!data.enumStrings.isEmpty() && enumStrings_ != data.enumStrings) {
    enumStrings_ = data.enumStrings;
    const QStringList labelsCopy = enumStrings_;
    invokeOnElement([labelsCopy](MenuElement *element) {
      element->setRuntimeLabels(labelsCopy);
    });
  }

  if (enumValue != lastValue_) {
    lastValue_ = enumValue;
    if (element_) {
      invokeOnElement([enumValue](MenuElement *element) {
        element->setRuntimeValue(enumValue);
      });
    }
  }
}

void MenuRuntime::handleAccessRights(bool /*canRead*/, bool canWrite)
{
  if (!started_) {
    return;
  }
  if (canWrite == lastWriteAccess_) {
    return;
  }
  lastWriteAccess_ = canWrite;
  if (element_) {
    invokeOnElement([canWrite](MenuElement *element) {
      element->setRuntimeWriteAccess(canWrite);
    });
  }
}

void MenuRuntime::handleActivation(int value)
{
  if (!started_ || !connected_ || !lastWriteAccess_) {
    return;
  }
  if (value < 0) {
    return;
  }

  dbr_enum_t toSend = static_cast<dbr_enum_t>(value);
  if (!PvChannelManager::instance().putValue(channelName_, toSend)) {
    qWarning() << "Failed to write menu value" << value << "to"
               << channelName_;
    return;
  }
  AuditLogger::instance().logPut(channelName_, value,
      QStringLiteral("Menu"));
}
