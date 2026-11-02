// SPDX-License-Identifier: GPL-3.0-or-later

#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <catch2/catch_test_macros.hpp>
#include "common/game_presets.h"
#include "common/scope_exit.h"
#include "network/network.h"

using namespace std::chrono_literals;

namespace {
bool WaitFor(const auto& predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    do {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(5ms);
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}
} // namespace

TEST_CASE("Room transport relays Switch 2 packets and enforces passwords and capacity", "[rooms]") {
    REQUIRE(Network::Init());
    SCOPE_EXIT {
        Network::Shutdown();
    };
    Network::Room room;
    Network::RoomMember alice;
    Network::RoomMember bob;
    Network::RoomMember rejected;
    SCOPE_EXIT {
        rejected.Leave();
        bob.Leave();
        alice.Leave();
        if (room.GetState() == Network::Room::State::Open) {
            room.Destroy();
        }
    };
    const AnnounceMultiplayerRoom::GameInfo game{.name = "Mario Kart World",
                                                 .id = Settings::MarioKartWorldProgramId};
    u16 port = 0;
    for (u16 candidate = 43000; candidate < 43064; ++candidate) {
        if (room.Create("Loopback test", "", "127.0.0.1", candidate, "test-password", 2, "", game,
                        std::make_unique<Network::VerifyUser::NullBackend>())) {
            port = candidate;
            break;
        }
    }
    REQUIRE(port != 0);
    alice.Join("Alice", "127.0.0.1", port, 0, Network::NoPreferredIP, "test-password");
    REQUIRE(WaitFor([&] { return alice.GetState() == Network::RoomMember::State::Joined; }));
    std::atomic<Network::RoomMember::Error> error{};
    const auto handle = rejected.BindOnError([&](const auto& value) { error.store(value); });
    SCOPE_EXIT {
        rejected.Unbind(handle);
    };
    rejected.Join("WrongPassword", "127.0.0.1", port, 0, Network::NoPreferredIP, "wrong");
    REQUIRE(WaitFor([&] { return error.load() == Network::RoomMember::Error::WrongPassword; }));
    rejected.Leave();
    bob.Join("Bobby", "127.0.0.1", port, 0, Network::NoPreferredIP, "test-password");
    REQUIRE(WaitFor([&] { return bob.GetState() == Network::RoomMember::State::Joined; }));
    error.store(Network::RoomMember::Error{});
    rejected.Join("RoomFull", "127.0.0.1", port, 0, Network::NoPreferredIP, "test-password");
    REQUIRE(WaitFor([&] { return error.load() == Network::RoomMember::Error::RoomIsFull; }));

    std::promise<Network::ProxyPacket> broadcast_promise;
    auto broadcast_future = broadcast_promise.get_future();
    std::atomic_bool broadcast_received{};
    const auto broadcast_handle = bob.BindOnProxyPacketReceived([&](const auto& packet) {
        if (packet.broadcast && !broadcast_received.exchange(true)) {
            broadcast_promise.set_value(packet);
        }
    });
    SCOPE_EXIT {
        bob.Unbind(broadcast_handle);
    };
    Network::ProxyPacket broadcast{
        .local_endpoint = {.family = Network::Domain::INET,
                           .ip = alice.GetFakeIpAddress(),
                           .portno = 7777},
        .remote_endpoint = {.family = Network::Domain::INET,
                            .ip = {255, 255, 255, 255},
                            .portno = 7777},
        .protocol = Network::Protocol::UDP,
        .broadcast = true,
        .data = {1, 2, 3, 4},
    };
    alice.SendProxyPacket(broadcast);
    REQUIRE(broadcast_future.wait_for(5s) == std::future_status::ready);
    const auto received_broadcast = broadcast_future.get();
    CHECK(received_broadcast.data == broadcast.data);
    CHECK(received_broadcast.local_endpoint.ip == alice.GetFakeIpAddress());

    std::promise<Network::ProxyPacket> reply_promise;
    auto reply_future = reply_promise.get_future();
    std::atomic_bool reply_received{};
    const auto reply_handle = alice.BindOnProxyPacketReceived([&](const auto& packet) {
        if (!reply_received.exchange(true)) {
            reply_promise.set_value(packet);
        }
    });
    SCOPE_EXIT {
        alice.Unbind(reply_handle);
    };
    auto reply = broadcast;
    reply.broadcast = false;
    reply.local_endpoint.ip = bob.GetFakeIpAddress();
    reply.remote_endpoint.ip = alice.GetFakeIpAddress();
    reply.data = {5, 6, 7};
    bob.SendProxyPacket(reply);
    REQUIRE(reply_future.wait_for(5s) == std::future_status::ready);
    CHECK(reply_future.get().data == reply.data);
}
