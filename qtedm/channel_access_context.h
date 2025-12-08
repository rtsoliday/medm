#pragma once

#include <QHash>
#include <QObject>

class QSocketNotifier;
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
  void handleFdActivity(int fd);
  void maybeReportPollStats(const char *trigger);

  /* CA file descriptor registration callback */
  static void fdRegistrationCallback(void *user, int fd, int opened);

  bool initialized_ = false;
  QTimer *pollTimer_ = nullptr;

  /* Map of file descriptors to socket notifiers for immediate CA event processing */
  QHash<int, QSocketNotifier *> socketNotifiers_;

  /* Diagnostic counters for tracking poll activity */
  int timerPollCount_ = 0;
  int fdPollCount_ = 0;
  qint64 lastPollReportTime_ = 0;
};
