// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>
#include "common/game_presets.h"
#include "network/switch2_rooms.h"

TEST_CASE("Switch 2 public room IDs resolve only compatible directory entries", "[rooms]") {
    AnnounceMultiplayerRoom::Room room;
    room.information.preferred_game.id = Settings::MarioKartWorldProgramId;
    room.information.port = Network::DefaultRoomPort;
    room.net_version = Network::network_version;
    room.ip = "127.0.0.1";
    room.verify_uid = "12345678-1234-1234-1234-123456789abc";
    room.id = "directory-id";
    AnnounceMultiplayerRoom::RoomList rooms{room};
    CHECK(Network::FindSwitch2Room(rooms, Network::PublicRoomCode(room.verify_uid)) == &rooms[0]);
    CHECK(Network::FindSwitch2Room(rooms, room.verify_uid) == &rooms[0]);
    CHECK(Network::FindSwitch2Room(rooms, room.id) == &rooms[0]);
    CHECK_FALSE(Network::FindSwitch2Room(rooms, "unknown"));
    CHECK_FALSE(Network::FindSwitch2Room(rooms, "eglantine-room:"));
    CHECK_FALSE(Network::FindSwitch2Room(rooms, "https://example.com"));
    CHECK_FALSE(Network::FindSwitch2Room(rooms, std::string(129, 'a')));
    rooms[0].information.preferred_game.id = 0x0100152000022000ULL;
    CHECK_FALSE(Network::FindSwitch2Room(rooms, room.verify_uid));
    rooms[0] = room;
    ++rooms[0].net_version;
    CHECK_FALSE(Network::FindSwitch2Room(rooms, room.verify_uid));
    rooms[0] = room;
    rooms[0].information.port = 0;
    CHECK_FALSE(Network::FindSwitch2Room(rooms, room.verify_uid));
    rooms[0] = room;
    rooms[0].ip.clear();
    CHECK_FALSE(Network::FindSwitch2Room(rooms, room.verify_uid));
    CHECK_FALSE(Network::IsSwitch2RoomGame(0));
}
