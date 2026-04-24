#include "shared_channel_manager.h"

SubscriptionHandle::SubscriptionHandle(quint64 id, SubscriptionOwner *owner)
  : id_(id)
  , owner_(owner)
{
}

SubscriptionHandle::~SubscriptionHandle()
{
  reset();
}

SubscriptionHandle::SubscriptionHandle(SubscriptionHandle &&other) noexcept
  : id_(other.id_)
  , owner_(other.owner_)
{
  other.id_ = 0;
  other.owner_ = nullptr;
}

SubscriptionHandle &SubscriptionHandle::operator=(
    SubscriptionHandle &&other) noexcept
{
  if (this != &other) {
    reset();
    id_ = other.id_;
    owner_ = other.owner_;
    other.id_ = 0;
    other.owner_ = nullptr;
  }
  return *this;
}

void SubscriptionHandle::reset()
{
  if (id_ != 0 && owner_) {
    owner_->unsubscribe(id_);
  }
  id_ = 0;
  owner_ = nullptr;
}
