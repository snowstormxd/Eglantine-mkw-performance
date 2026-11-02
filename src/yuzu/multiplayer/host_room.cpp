// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2017 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <future>
#include <QColor>
#include <QImage>
#include <QLabel>
#include <QList>
#include <QLocale>
#include <QMessageBox>
#include <QMetaType>
#include <QTime>
#include <QtConcurrentRun>
#include "common/game_presets.h"
#include "common/logging.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/internal_network/network_interface.h"
#include "network/announce_multiplayer_session.h"
#include "qt_common/config/uisettings.h"
#include "qt_common/game_list/game_list_p.h"
#include "ui_host_room.h"
#include "yuzu/main_window.h"
#include "yuzu/multiplayer/game_list.h"
#include "yuzu/multiplayer/host_room.h"
#include "yuzu/multiplayer/message.h"
#include "yuzu/multiplayer/state.h"
#include "yuzu/multiplayer/validation.h"
#ifdef ENABLE_WEB_SERVICE
#include "web_service/verify_user_jwt.h"
#endif

HostRoomWindow::HostRoomWindow(QWidget* parent, QStandardItemModel* list,
                               std::shared_ptr<Core::AnnounceMultiplayerSession> session,
                               Core::System& system_)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint),
      ui(std::make_unique<Ui::HostRoom>()), announce_multiplayer_session(session), system{system_} {
    ui->setupUi(this);
    setWindowTitle(tr("Host a Switch 2 room"));
    auto* help =
        new QLabel(tr("Choose a Switch 2 game, name your room, and set its connection limit. "
                      "Public rooms appear in Eden's directory. Internet guests need access "
                      "to the room's UDP port; this does not configure your router."),
                   this);
    help->setWordWrap(true);
    ui->verticalLayout_3->insertWidget(0, help);
    ui->password->setEchoMode(QLineEdit::Password);
    connect(&watcher, &QFutureWatcher<QString>::finished, this, [this] {
        ui->host->setEnabled(true);
        ui->host->setText(tr("Host Room"));
        const auto error = watcher.result();
        if (!error.isEmpty()) {
            QMessageBox::warning(this, tr("Could not host room"), error);
        } else {
            close();
        }
    });

    // set up validation for all of the fields
    ui->room_name->setValidator(validation.GetRoomName());
    ui->username->setValidator(validation.GetNickname());
    ui->port->setValidator(validation.GetPort());
    ui->port->setPlaceholderText(QString::number(Network::DefaultRoomPort));

    // Create a proxy to the game list to display the list of preferred games
    game_list = new QStandardItemModel(this);
    UpdateGameList(list);

    proxy = new ComboBoxProxyModel;
    proxy->setParent(this);
    proxy->setSourceModel(game_list);
    proxy->sort(0, Qt::AscendingOrder);
    ui->game_list->setModel(proxy);

    // Connect all the widgets to the appropriate events
    connect(ui->host, &QPushButton::clicked, this, &HostRoomWindow::Host);

    // Restore the settings:
    ui->username->setText(
        QString::fromStdString(UISettings::values.multiplayer_room_nickname.GetValue()));
    if (ui->username->text().isEmpty() && !Settings::values.eden_username.GetValue().empty()) {
        // Use Eden Web Service user name as nickname by default
        ui->username->setText(QString::fromStdString(Settings::values.eden_username.GetValue()));
    }
    ui->room_name->setText(
        QString::fromStdString(UISettings::values.multiplayer_room_name.GetValue()));
    ui->port->setText(QString::number(UISettings::values.multiplayer_room_port.GetValue()));
    ui->max_player->setValue(UISettings::values.multiplayer_max_player.GetValue());
    int index = UISettings::values.multiplayer_host_type.GetValue();
    if (index < ui->host_type->count()) {
        ui->host_type->setCurrentIndex(index);
    }
    index = ui->game_list->findData(UISettings::values.multiplayer_game_id.GetValue(),
                                    GameListItemPath::ProgramIdRole);
    if (index != -1) {
        ui->game_list->setCurrentIndex(index);
    }
    if (ui->username->text().isEmpty()) {
        ui->username->setText(QStringLiteral("Player"));
    }
    if (ui->room_name->text().isEmpty()) {
        ui->room_name->setText(QStringLiteral("Mario Kart World"));
    }
    if (index == -1) {
        const auto mkw = ui->game_list->findData(
            QVariant::fromValue<qulonglong>(Settings::MarioKartWorldProgramId),
            GameListItemPath::ProgramIdRole);
        if (mkw != -1) {
            ui->game_list->setCurrentIndex(mkw);
        }
    }
    ui->room_description->setText(
        QString::fromStdString(UISettings::values.multiplayer_room_description.GetValue()));
}

HostRoomWindow::~HostRoomWindow() {
    watcher.waitForFinished();
}

void HostRoomWindow::UpdateGameList(QStandardItemModel* list) {
    game_list->clear();
    AppendSwitch2Games(list->invisibleRootItem(), *game_list);
}

void HostRoomWindow::RetranslateUi() {
    ui->retranslateUi(this);
    setWindowTitle(tr("Host a Switch 2 room"));
}

std::unique_ptr<Network::VerifyUser::Backend> HostRoomWindow::CreateVerifyBackend(
    bool use_validation) const {
    std::unique_ptr<Network::VerifyUser::Backend> verify_backend;
    if (use_validation) {
#ifdef ENABLE_WEB_SERVICE
        verify_backend =
            std::make_unique<WebService::VerifyUserJWT>(Settings::values.web_api_url.GetValue());
#else
        verify_backend = std::make_unique<Network::VerifyUser::NullBackend>();
#endif
    } else {
        verify_backend = std::make_unique<Network::VerifyUser::NullBackend>();
    }
    return verify_backend;
}

void HostRoomWindow::Host() {
    if (static_cast<MultiplayerState*>(parentWidget())->IsConnecting()) {
        return;
    }
    if (system.IsPoweredOn()) {
        QMessageBox::information(
            this, tr("Stop the game first"),
            tr("Host a room before starting the game so it uses the room connection."));
        return;
    }
    if (!Network::GetSelectedNetworkInterface()) {
        Network::SelectFirstNetworkInterface();
    }
    if (!Network::GetSelectedNetworkInterface()) {
        NetworkMessage::ErrorManager::ShowError(
            NetworkMessage::ErrorManager::NO_INTERFACE_SELECTED);
        return;
    }
    if (!ui->username->hasAcceptableInput() || !ui->room_name->hasAcceptableInput()) {
        QMessageBox::warning(this, tr("Room details"),
                             tr("Enter a room name and nickname (4-20 letters, numbers, spaces, "
                                "dots, dashes or underscores)."));
        return;
    }
    if (!ui->port->hasAcceptableInput() || ui->port->text().toUShort() == 0) {
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::PORT_NOT_VALID);
        return;
    }
    const auto game_id = ui->game_list->currentData(GameListItemPath::ProgramIdRole).toULongLong();
    if (!Network::IsSwitch2RoomGame(game_id)) {
        QMessageBox::warning(this, tr("Select a Switch 2 game"),
                             tr("Add a Switch 2 game to your game list, then select it here."));
        return;
    }
    const bool is_public = ui->host_type->currentIndex() == 0;
    const auto api = Settings::values.web_api_url.GetValue();
    const auto username = Settings::values.eden_username.GetValue();
    const auto account_token = Settings::values.eden_token.GetValue();
    if (is_public && (username.empty() || account_token.empty())) {
        QMessageBox::information(
            this, tr("Public room account"),
            tr("Public hosting uses Eden's room directory and requires an account "
               "in Emulation > Configure > Web. Configure it there, or choose "
               "Unlisted to host directly without publishing the room."));
        return;
    }
    const auto member = Network::GetRoomMember().lock();
    const auto room = Network::GetRoom().lock();
    const auto session = announce_multiplayer_session.lock();
    if (!member || !room || !session || member->GetState() == Network::RoomMember::State::Joining) {
        return;
    }
    if ((member->IsConnected() || room->GetState() == Network::Room::State::Open) &&
        !static_cast<MultiplayerState*>(parentWidget())->OnCloseRoom()) {
        return;
    }
    const auto nickname = ui->username->text().toStdString();
    const auto name = ui->room_name->text().toStdString();
    const auto description = ui->room_description->toPlainText().toStdString();
    const auto port = ui->port->text().toUShort();
    const auto connection_limit = ui->max_player->value();
    const auto password = ui->password->text().toStdString();
    const AnnounceMultiplayerRoom::GameInfo game{
        .name = ui->game_list->currentData(Qt::DisplayRole).toString().toStdString(),
        .id = game_id,
    };
    const auto bans = ui->load_ban_list->isChecked() ? UISettings::values.multiplayer_ban_list
                                                     : Network::Room::BanList{};
    auto verification = CreateVerifyBackend(is_public);
    UISettings::values.multiplayer_room_nickname = nickname;
    UISettings::values.multiplayer_room_name = name;
    UISettings::values.multiplayer_game_id = game_id;
    UISettings::values.multiplayer_max_player = connection_limit;
    UISettings::values.multiplayer_host_type = ui->host_type->currentIndex();
    UISettings::values.multiplayer_room_port = port;
    UISettings::values.multiplayer_room_description = description;
    emit SaveConfig();
    ui->host->setEnabled(false);
    ui->host->setText(tr("Starting room..."));
    watcher.setFuture(QtConcurrent::run([=,
                                         backend = std::move(verification)]() mutable -> QString {
        try {
            if (!room->Create(name, description, "", port, password, connection_limit, username,
                              game, std::move(backend), bans)) {
                return tr(
                    "Could not start the room. Check whether its UDP port is already in use.");
            }
            std::string jwt;
            if (is_public) {
                const auto result = session->Register();
                if (result.result_code != WebService::WebResult::Code::Success) {
                    room->Destroy();
                    return tr("Could not publish the room. Check your Web account and directory "
                              "connection.\n%1")
                        .arg(QString::fromStdString(result.result_string));
                }
                session->Start();
#ifdef ENABLE_WEB_SERVICE
                WebService::Client client(api, username, account_token);
                jwt = client.GetExternalJWT(room->GetVerifyUID()).returned_data;
#endif
            }
            member->Join(nickname, "127.0.0.1", port, 0, Network::NoPreferredIP, password, jwt);
            // Join starts the protocol handshake; its final result arrives through the room
            // callbacks.
            return {};
        } catch (const std::exception&) {
            session->Stop();
            if (room->GetState() == Network::Room::State::Open) {
                room->Destroy();
            }
            return tr("The room service returned an invalid response. The room was closed.");
        }
    }));
}
QVariant ComboBoxProxyModel::data(const QModelIndex& idx, int role) const {
    if (role != Qt::DisplayRole) {
        auto val = QSortFilterProxyModel::data(idx, role);
        // If its the icon, shrink it to 16x16
        if (role == Qt::DecorationRole)
            val = val.value<QImage>().scaled(16, 16, Qt::KeepAspectRatio);
        return val;
    }
    std::string filename;
    Common::SplitPath(
        QSortFilterProxyModel::data(idx, GameListItemPath::FullPathRole).toString().toStdString(),
        nullptr, &filename, nullptr);
    QString title = QSortFilterProxyModel::data(idx, GameListItemPath::TitleRole).toString();
    return title.isEmpty() ? QString::fromStdString(filename) : title;
}

bool ComboBoxProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const {
    auto leftData = left.data(GameListItemPath::TitleRole).toString();
    auto rightData = right.data(GameListItemPath::TitleRole).toString();
    return leftData.compare(rightData) < 0;
}
