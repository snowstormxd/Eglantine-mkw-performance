// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QStandardItemModel>
#include "network/switch2_rooms.h"
#include "qt_common/game_list/game_list_p.h"

inline void AppendSwitch2Games(const QStandardItem* parent, QStandardItemModel& games) {
    for (int row = 0; row < parent->rowCount(); ++row) {
        const auto* item = parent->child(row);
        if (!item) {
            continue;
        }
        if (Network::IsSwitch2RoomGame(item->data(GameListItemPath::ProgramIdRole).toULongLong())) {
            games.appendRow(item->clone());
        } else {
            AppendSwitch2Games(item, games);
        }
    }
}
