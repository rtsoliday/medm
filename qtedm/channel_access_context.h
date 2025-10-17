#pragma once

#include <QObject>

class QTimer;

class ChannelAccessContext : public QObject
{
public:
  static ChannelAccessContext &instance();

  void ensureInitialized();
  bool isInitialized() const;

private:
  ChannelAccessContext();
  ~ChannelAccessContext() override;

  void initialize();
  void pollOnce();

  bool initialized_ = false;
  QTimer *pollTimer_ = nullptr;
};
