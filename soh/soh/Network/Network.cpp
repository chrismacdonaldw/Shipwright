#include "Network.h"
#include <spdlog/spdlog.h>
#ifdef DIPTYCH_GAME_MODULE
#include <algorithm>
#include <random>
#endif

// MARK: - Public

void Network::Enable(const char* host, uint16_t port) {
    if (isEnabled) {
        return;
    }

#ifdef DIPTYCH_GAME_MODULE
    Disable();
#endif
    if (SDLNet_ResolveHost(&networkAddress, host, port) == -1) {
        SPDLOG_ERROR("[Network] SDLNet_ResolveHost: {}", SDLNet_GetError());
    }

#ifndef DIPTYCH_GAME_MODULE
    isEnabled = true;
#endif

    // First check if there is a thread running, if so, join it
    if (receiveThread.joinable()) {
        receiveThread.join();
    }

#ifdef DIPTYCH_GAME_MODULE
    isEnabled = true;
    try {
        receiveThread = std::thread(&Network::ReceiveFromServer, this);
    } catch (...) {
        isEnabled = false;
        throw;
    }
#else
    receiveThread = std::thread(&Network::ReceiveFromServer, this);
#endif
}

void Network::Disable() {
#ifdef DIPTYCH_GAME_MODULE
    isEnabled = false;
    if (receiveThread.joinable()) receiveThread.join();
#else
    if (!isEnabled) {
        return;
    }

    isEnabled = false;
    receiveThread.join();
#endif
}

void Network::OnIncomingData(char payload[512]) {
}

void Network::OnIncomingJson(nlohmann::json payload) {
}

void Network::OnConnected() {
}

void Network::OnDisconnected() {
}

void Network::ProcessOutgoingPackets() {
}

void Network::SendDataToRemote(const char* payload) {
    SPDLOG_DEBUG("[Network] Sending data: {}", payload);
#ifdef DIPTYCH_GAME_MODULE
    std::lock_guard lock(socketMutex);
    if (!networkSocket || !isConnected || !isEnabled) return;
    int remaining = static_cast<int>(strlen(payload) + 1);
    while (remaining > 0) {
        int sent = SDLNet_TCP_Send(networkSocket, payload, remaining);
        if (sent <= 0) { sendFailed = true; return; }
        payload += sent;
        remaining -= sent;
    }
#else
    SDLNet_TCP_Send(networkSocket, payload, static_cast<int>(strlen(payload) + 1));
#endif
}

void Network::SendJsonToRemote(nlohmann::json payload) {
    SendDataToRemote(payload.dump().c_str());
}

// MARK: - Private

void Network::ReceiveFromServer() {
#ifdef DIPTYCH_GAME_MODULE
    uint32_t retryDelay = 1000;
    uint64_t connectedAt = 0;
    std::mt19937 random(std::random_device{}());
    auto waitToReconnect = [&]() {
        std::uniform_int_distribution<uint32_t> jitter(retryDelay * 4 / 5,
                                                       std::min(retryDelay * 6 / 5, 30000u));
        uint32_t remaining = jitter(random);
        retryDelay = std::min(retryDelay * 2, 30000u);
        while (isEnabled && remaining > 0) {
            const auto slice = std::min(remaining, 25u);
            SDL_Delay(slice);
            remaining -= slice;
        }
    };
#endif
    while (isEnabled) {
        while (!isConnected && isEnabled) {
            SPDLOG_TRACE("[Network] Attempting to make connection to server...");
#ifdef DIPTYCH_GAME_MODULE
            auto openedSocket = SDLNet_TCP_Open(&networkAddress);
            {
                std::lock_guard lock(socketMutex);
                networkSocket = openedSocket;
                sendFailed = false;
            }
            if (!networkSocket) waitToReconnect();
#else
            networkSocket = SDLNet_TCP_Open(&networkAddress);
#endif

            if (networkSocket) {
                isConnected = true;
#ifdef DIPTYCH_GAME_MODULE
                connectedAt = SDL_GetTicks64();
#endif
                receivedData.clear();
                SPDLOG_INFO("[Network] Connection to server established!");

                OnConnected();
                break;
            }
        }

        SDLNet_SocketSet socketSet = SDLNet_AllocSocketSet(1);
        if (networkSocket) {
            SDLNet_TCP_AddSocket(socketSet, networkSocket);
        }

        // Listen to socket messages
        while (isConnected && networkSocket && isEnabled) {
            // we check first if socket has data, to not block in the TCP_Recv
#ifdef DIPTYCH_GAME_MODULE
            if (sendFailed) break;
            int socketsReady = SDLNet_CheckSockets(socketSet, 1);
#else
            int socketsReady = SDLNet_CheckSockets(socketSet, 0);
#endif

            if (socketsReady == -1) {
                SPDLOG_ERROR("[Network] SDLNet_CheckSockets: {}", SDLNet_GetError());
                break;
            }

            // Always process outgoing packets
            ProcessOutgoingPackets();

            if (socketsReady == 0) {
                // No incoming data
                continue;
            }

            char remoteDataReceived[512];
            memset(remoteDataReceived, 0, sizeof(remoteDataReceived));
            int len = SDLNet_TCP_Recv(networkSocket, &remoteDataReceived, sizeof(remoteDataReceived));
            if (!len || !networkSocket || len == -1) {
                SPDLOG_ERROR("[Network] SDLNet_TCP_Recv: {}", SDLNet_GetError());
                break;
            }

            HandleRemoteData(remoteDataReceived);

            receivedData.append(remoteDataReceived, len);

            // Proess all complete packets
            size_t delimiterPos = receivedData.find('\0');
            while (delimiterPos != std::string::npos) {
                // Extract the complete packet until the delimiter
                std::string packet = receivedData.substr(0, delimiterPos);
                // Remove the packet (including the delimiter) from the received data
                receivedData.erase(0, delimiterPos + 1);
#ifdef DIPTYCH_GAME_MODULE
                if (packet.size() > 65536) { sendFailed = true; break; }
#endif
                HandleRemoteJson(packet);
                // Find the next delimiter
                delimiterPos = receivedData.find('\0');
            }
#ifdef DIPTYCH_GAME_MODULE
            if (receivedData.size() > 65536) break;
#endif
        }

        if (socketSet) {
            SDLNet_FreeSocketSet(socketSet);
        }

        if (isConnected) {
#ifdef DIPTYCH_GAME_MODULE
            if (SDL_GetTicks64() - connectedAt >= 30000) retryDelay = 1000;
            {
                std::lock_guard lock(socketMutex);
                isConnected = false;
                SDLNet_TCP_Close(networkSocket);
                networkSocket = nullptr;
            }
#else
            SDLNet_TCP_Close(networkSocket);
            networkSocket = nullptr;
            isConnected = false;
#endif
            receivedData.clear();
            OnDisconnected();
            SPDLOG_INFO("[Network] Ending receiving thread...");
#ifdef DIPTYCH_GAME_MODULE
            if (isEnabled) waitToReconnect();
#endif
        }
    }
}

void Network::HandleRemoteData(char payload[512]) {
    OnIncomingData(payload);
}

void Network::HandleRemoteJson(std::string payload) {
    SPDLOG_DEBUG("[Network] Received json: {}", payload);
    nlohmann::json jsonPayload;
    try {
        jsonPayload = nlohmann::json::parse(payload);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Network] Failed to parse json: \n{}\n{}\n", payload, e.what());
        return;
    }

    try {
        OnIncomingJson(jsonPayload);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Network] Exception handling incoming JSON: {}", e.what());
    } catch (...) { SPDLOG_ERROR("[Network] Unknown exception handling incoming JSON"); }
}
