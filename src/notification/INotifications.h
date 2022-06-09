// Copyright (c) 2018-2022 The Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#ifndef POCKETNET_CORE_INOTIFICATIONS_H
#define POCKETNET_CORE_INOTIFICATIONS_H

#include "notification/INotificationProtocol.h"
#include "notification/NotificationClient.h"

#include "notification/NotificationTypedefs.h"

#include <memory>


class INotifications
{
public:
    virtual std::shared_ptr<INotificationProtocol> GetProtocol() const = 0;
    virtual std::shared_ptr<NotificationQueue> GetNotificationQueue() const = 0;
    virtual void Start(int threads) = 0;
    virtual void Stop() = 0;
    virtual UniValue CollectStats() const = 0;
};

#endif // POCKETNET_CORE_INOTIFICATIONS_H