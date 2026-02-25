// WebSockets.h
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>

class WebSockets
{
public:
    WebSockets();
    ~WebSockets();

    bool Start(uint16_t port = 8081);
    void Stop();

    bool IsRunning() const;
    bool IsSupported() const;

private:
    void RunServer(uint16_t port);

    std::atomic<bool> running{false};

#ifdef HAVE_UWEBSOCKETS
    namespace uWS { class Loop; }
    struct us_listen_socket_t;

    struct ThreadState
    {
        uWS::Loop* loop = nullptr;
        struct us_listen_socket_t* listenSocket = nullptr;
    };

    std::unique_ptr<std::thread> serverThread;
    std::atomic<bool> stopRequested{false};
    ThreadState threadState;
    std::mutex stateMutex;
    std::condition_variable stateCv;
    bool listenResultKnown = false;
    bool listenSucceeded = false;
#endif
};