// WebSockets.cpp
#include "WebSockets.h"

#include <cstdio>
#include <chrono>
#include <string_view>
#include <utility>

#ifdef HAVE_UWEBSOCKETS
#    include <uwebsockets/App.h>
#    include <libusockets.h>
#endif

WebSockets::WebSockets() = default;

WebSockets::~WebSockets()
{
    Stop();
}

bool WebSockets::Start(uint16_t port)
{
#ifndef HAVE_UWEBSOCKETS
    (void)port;
    std::printf("[WebSockets] uWebSockets não está disponível neste build.\n");
    return false;
#else
    if (running.load())
        return true;

    stopRequested = false;
    listenResultKnown = false;
    listenSucceeded = false;

    serverThread = std::make_unique<std::thread>(&WebSockets::RunServer, this, port);

    // Espera a tentativa de bind para saber se iniciou com sucesso.
    std::unique_lock<std::mutex> lock(stateMutex);
    stateCv.wait_for(lock, std::chrono::milliseconds(500), [this]() { return listenResultKnown; });
    return listenResultKnown && listenSucceeded;
#endif
}

void WebSockets::Stop()
{
#ifdef HAVE_UWEBSOCKETS
    stopRequested = true;

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (threadState.loop)
        {
            auto loop = threadState.loop;
            loop->defer([this]() {
                if (threadState.listenSocket)
                {
                    us_listen_socket_close(0, threadState.listenSocket);
                    threadState.listenSocket = nullptr;
                }
                if (threadState.loop)
                    threadState.loop->stop();
            });
        }
    }

    if (serverThread && serverThread->joinable())
    {
        serverThread->join();
    }

    serverThread.reset();
    running = false;
    threadState = {};
#endif
}

bool WebSockets::IsRunning() const
{
    return running.load();
}

bool WebSockets::IsSupported() const
{
#ifdef HAVE_UWEBSOCKETS
    return true;
#else
    return false;
#endif
}

void WebSockets::RunServer(uint16_t port)
{
#ifdef HAVE_UWEBSOCKETS
    uWS::App app;
    threadState.loop = app.getLoop();

    using AppType = uWS::TemplatedApp<false>;
    using Behavior = AppType::WebSocketBehavior;

    Behavior behavior{};
    behavior.open = [](auto* ws) {
        (void)ws;
        std::printf("[WebSockets] conexão aberta\n");
    };
    behavior.message = [](auto* ws, std::string_view msg, uWS::OpCode opCode) {
        ws->send(msg, opCode);
    };
    behavior.close = [](auto* ws, int /*code*/, std::string_view /*message*/) {
        (void)ws;
        std::printf("[WebSockets] conexão fechada\n");
    };

    app.ws<>("/*", std::move(behavior))
        .listen(port, [this, port](auto* token) {
            std::lock_guard<std::mutex> lock(stateMutex);
            listenResultKnown = true;
            listenSucceeded = token != nullptr;
            if (token)
            {
                threadState.listenSocket = token;
                running = true;
                std::printf("Servidor iniciado, IP (0.0.0.0:%u)\n", port);
            }
            else
            {
                running = false;
                std::printf("[WebSockets] Falha ao iniciar na porta %u\n", port);
                if (threadState.loop)
                {
                    threadState.loop->defer([loop = threadState.loop]() {
                        if (loop)
                        {
                            loop->stop();
                        }
                    });
                }
            }
            stateCv.notify_all();
        });

    app.get("/", [](auto* res, auto* /*req*/) {
        res->writeStatus("200 OK")->writeHeader("Content-Type", "text/plain")->end("uWebSockets ativo");
    });

    app.run();

    running = false;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        stateCv.notify_all();
    }
#else
    (void)port;
#endif
}