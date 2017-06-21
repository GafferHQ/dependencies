// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/heartbeat_manager.h"

#include "base/callback.h"
#include "base/metrics/histogram.h"
#include "base/power_monitor/power_monitor.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/base/network_change_notifier.h"

namespace gcm {

namespace {
// The default heartbeat when on a mobile or unknown network .
const int kCellHeartbeatDefaultMs = 1000 * 60 * 28;  // 28 minutes.
// The default heartbeat when on WiFi (also used for ethernet).
const int kWifiHeartbeatDefaultMs = 1000 * 60 * 15;  // 15 minutes.
// The default heartbeat ack interval.
const int kHeartbeatAckDefaultMs = 1000 * 60 * 1;  // 1 minute.
// Minimum allowed client default heartbeat interval.
const int kMinClientHeartbeatIntervalMs = 1000 * 60 * 2;  // 2 minutes.

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
// The period at which to check if the heartbeat time has passed. Used to
// protect against platforms where the timer is delayed by the system being
// suspended.  Only needed on linux because the other OSes provide a standard
// way to be notified of system suspend and resume events.
const int kHeartbeatMissedCheckMs = 1000 * 60 * 5;  // 5 minutes.
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

}  // namespace

HeartbeatManager::HeartbeatManager()
    : waiting_for_ack_(false),
      heartbeat_interval_ms_(0),
      server_interval_ms_(0),
      client_interval_ms_(0),
      heartbeat_timer_(new base::Timer(true /* retain_user_task */,
                                       false /* is_repeating */)),
      weak_ptr_factory_(this) {
  // Listen for system suspend and resume events.
  base::PowerMonitor* monitor = base::PowerMonitor::Get();
  if (monitor)
    monitor->AddObserver(this);
}

HeartbeatManager::~HeartbeatManager() {
  // Stop listening for system suspend and resume events.
  base::PowerMonitor* monitor = base::PowerMonitor::Get();
  if (monitor)
    monitor->RemoveObserver(this);
}

void HeartbeatManager::Start(
    const base::Closure& send_heartbeat_callback,
    const ReconnectCallback& trigger_reconnect_callback) {
  DCHECK(!send_heartbeat_callback.is_null());
  DCHECK(!trigger_reconnect_callback.is_null());
  send_heartbeat_callback_ = send_heartbeat_callback;
  trigger_reconnect_callback_ = trigger_reconnect_callback;

  // Kicks off the timer.
  waiting_for_ack_ = false;
  RestartTimer();
}

void HeartbeatManager::Stop() {
  heartbeat_expected_time_ = base::Time();
  heartbeat_interval_ms_ = 0;
  heartbeat_timer_->Stop();
  waiting_for_ack_ = false;
}

void HeartbeatManager::OnHeartbeatAcked() {
  if (!heartbeat_timer_->IsRunning())
    return;

  DCHECK(!send_heartbeat_callback_.is_null());
  DCHECK(!trigger_reconnect_callback_.is_null());
  waiting_for_ack_ = false;
  RestartTimer();
}

void HeartbeatManager::UpdateHeartbeatConfig(
    const mcs_proto::HeartbeatConfig& config) {
  if (!config.IsInitialized() ||
      !config.has_interval_ms() ||
      config.interval_ms() <= 0) {
    return;
  }
  DVLOG(1) << "Updating server heartbeat interval to " << config.interval_ms();
  server_interval_ms_ = config.interval_ms();
}

base::TimeTicks HeartbeatManager::GetNextHeartbeatTime() const {
  if (heartbeat_timer_->IsRunning())
    return heartbeat_timer_->desired_run_time();
  else
    return base::TimeTicks();
}

void HeartbeatManager::UpdateHeartbeatTimer(scoped_ptr<base::Timer> timer) {
  bool was_running = heartbeat_timer_->IsRunning();
  base::TimeDelta remaining_delay =
      heartbeat_timer_->desired_run_time() - base::TimeTicks::Now();
  base::Closure timer_task(heartbeat_timer_->user_task());

  heartbeat_timer_->Stop();
  heartbeat_timer_ = timer.Pass();

  if (was_running)
    heartbeat_timer_->Start(FROM_HERE, remaining_delay, timer_task);
}

void HeartbeatManager::OnResume() {
  CheckForMissedHeartbeat();
}

void HeartbeatManager::OnHeartbeatTriggered() {
  // Reset the weak pointers used for heartbeat checks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (waiting_for_ack_) {
    LOG(WARNING) << "Lost connection to MCS, reconnecting.";
    ResetConnection(ConnectionFactory::HEARTBEAT_FAILURE);
    return;
  }

  waiting_for_ack_ = true;
  RestartTimer();
  send_heartbeat_callback_.Run();
}

void HeartbeatManager::RestartTimer() {
  if (!waiting_for_ack_) {
    // Recalculate the timer interval based network type.
    // Server interval takes precedence over client interval, even if the latter
    // is less.
    if (server_interval_ms_ != 0) {
      // If a server interval is set, it overrides any local one.
      heartbeat_interval_ms_ = server_interval_ms_;
    } else if (HasClientHeartbeatInterval()) {
      // Client interval might have been adjusted up, which should only take
      // effect during a reconnection.
      if (client_interval_ms_ < heartbeat_interval_ms_ ||
          heartbeat_interval_ms_ == 0) {
        heartbeat_interval_ms_ = client_interval_ms_;
      }
    } else {
      heartbeat_interval_ms_ = GetDefaultHeartbeatInterval();
    }
    DVLOG(1) << "Sending next heartbeat in "
             << heartbeat_interval_ms_ << " ms.";
  } else {
    heartbeat_interval_ms_ = kHeartbeatAckDefaultMs;
    DVLOG(1) << "Resetting timer for ack with "
             << heartbeat_interval_ms_ << " ms interval.";
  }

  heartbeat_expected_time_ =
      base::Time::Now() +
      base::TimeDelta::FromMilliseconds(heartbeat_interval_ms_);
  heartbeat_timer_->Start(FROM_HERE,
                         base::TimeDelta::FromMilliseconds(
                             heartbeat_interval_ms_),
                         base::Bind(&HeartbeatManager::OnHeartbeatTriggered,
                                    weak_ptr_factory_.GetWeakPtr()));

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Windows, Mac, Android, iOS, and Chrome OS all provide a way to be notified
  // when the system is suspending or resuming.  The only one that does not is
  // Linux so we need to poll to check for missed heartbeats.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&HeartbeatManager::CheckForMissedHeartbeat,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kHeartbeatMissedCheckMs));
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)
}

void HeartbeatManager::CheckForMissedHeartbeat() {
  // If there's no heartbeat pending, return without doing anything.
  if (heartbeat_expected_time_.is_null())
    return;

  // If the heartbeat has been missed, manually trigger it.
  if (base::Time::Now() > heartbeat_expected_time_) {
    UMA_HISTOGRAM_LONG_TIMES("GCM.HeartbeatMissedDelta",
                             base::Time::Now() - heartbeat_expected_time_);
    OnHeartbeatTriggered();
    return;
  }

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Otherwise check again later.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&HeartbeatManager::CheckForMissedHeartbeat,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kHeartbeatMissedCheckMs));
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)
}

int HeartbeatManager::GetDefaultHeartbeatInterval() {
  // For unknown connections, use the longer cellular heartbeat interval.
  int heartbeat_interval_ms = kCellHeartbeatDefaultMs;
  if (net::NetworkChangeNotifier::GetConnectionType() ==
          net::NetworkChangeNotifier::CONNECTION_WIFI ||
      net::NetworkChangeNotifier::GetConnectionType() ==
          net::NetworkChangeNotifier::CONNECTION_ETHERNET) {
    heartbeat_interval_ms = kWifiHeartbeatDefaultMs;
  }
  return heartbeat_interval_ms;
}

int HeartbeatManager::GetMaxClientHeartbeatIntervalMs() {
  return GetDefaultHeartbeatInterval();
}

int HeartbeatManager::GetMinClientHeartbeatIntervalMs() {
  // Returning a constant. This should be adjusted for connection type, like the
  // default/max interval.
  return kMinClientHeartbeatIntervalMs;
}

void HeartbeatManager::SetClientHeartbeatIntervalMs(int interval_ms) {
  if ((interval_ms != 0 && !IsValidClientHeartbeatInterval(interval_ms)) ||
      interval_ms == client_interval_ms_) {
    return;
  }

  client_interval_ms_ = interval_ms;
  // Only reset connection if the new heartbeat interval is shorter. If it is
  // longer, the connection will reset itself at some point and interval will be
  // fixed.
  if (client_interval_ms_ > 0 && client_interval_ms_ < heartbeat_interval_ms_) {
    ResetConnection(ConnectionFactory::NEW_HEARTBEAT_INTERVAL);
  }
}

int HeartbeatManager::GetClientHeartbeatIntervalMs() {
  return client_interval_ms_;
}

bool HeartbeatManager::HasClientHeartbeatInterval() {
  return client_interval_ms_ != 0;
}

bool HeartbeatManager::IsValidClientHeartbeatInterval(int interval) {
  int max_heartbeat_interval = GetDefaultHeartbeatInterval();
  return kMinClientHeartbeatIntervalMs <= interval &&
      interval <= max_heartbeat_interval;
}

void HeartbeatManager::ResetConnection(
    ConnectionFactory::ConnectionResetReason reason) {
  Stop();
  trigger_reconnect_callback_.Run(reason);
}

}  // namespace gcm
