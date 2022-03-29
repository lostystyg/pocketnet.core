// Copyright (c) 2018-2022 The Pocketnet developers
// Distributed under the Apache 2.0 software license, see the accompanying
// https://www.apache.org/licenses/LICENSE-2.0

#ifndef POCKETNET_CORE_SIGNALING_PROCESSOR_H
#define POCKETNET_CORE_SIGNALING_PROCESSOR_H

#include "protectedmap.h"
#include "websocket/ws.h"
#include <string>

namespace webrtc::signaling
{
    using Connection = SimpleWeb::SocketServer<SimpleWeb::WS>::Connection;
    using Message = SimpleWeb::SocketServer<SimpleWeb::WS>::InMessage;

    class SignalingProcessor
    {
    public:
        void NewConnection(std::shared_ptr<Connection> conn);
        void ClosedConnection(const std::shared_ptr<Connection>& conn);
        void ProcessMessage(const std::shared_ptr<Connection>& connection, std::shared_ptr<Message> in_message);
        void Stop();
    private:
        ProtectedMap<std::string, std::shared_ptr<Connection>> m_connections;
    };
} // webrtc::signaling

#endif // POCKETNET_CORE_SIGNALING_PROCESSOR_H