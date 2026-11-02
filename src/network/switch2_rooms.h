// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include "network/room.h"

namespace Network {

// Retail Switch 2 application IDs use the 04 namespace; original Switch games use 01.
inline bool IsSwitch2RoomGame(u64 program_id) {
    return (program_id >> 56) == 0x04;
}

inline bool IsCompatibleSwitch2Room(const AnnounceMultiplayerRoom::Room& room) {
    return IsSwitch2RoomGame(room.information.preferred_game.id) &&
           room.net_version == network_version && !room.ip.empty() && room.information.port != 0;
}

inline std::string PublicRoomCode(std::string_view uid) {
    return "eglantine-room:" + std::string{uid};
}

inline bool IsValidPublicRoomId(std::string_view id) {
    return !id.empty() && id.size() <= 128 && std::ranges::all_of(id, [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
               c == '-' || c == '_';
    });
}

inline const AnnounceMultiplayerRoom::Room* FindSwitch2Room(
    const AnnounceMultiplayerRoom::RoomList& rooms, std::string_view code) {
    constexpr std::string_view prefix = "eglantine-room:";
    if (code.starts_with(prefix)) {
        code.remove_prefix(prefix.size());
    }
    if (!IsValidPublicRoomId(code)) {
        return nullptr;
    }
    const auto found = std::ranges::find_if(rooms, [code](const auto& room) {
        return IsCompatibleSwitch2Room(room) &&
               (room.verify_uid == code || (!room.id.empty() && room.id == code));
    });
    return found == rooms.end() ? nullptr : &*found;
}

} // namespace Network
