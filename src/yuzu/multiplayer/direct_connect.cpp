// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright 2017 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QtConcurrentRun>
#include "common/settings.h"
#include "core/core.h"
#include "core/internal_network/network_interface.h"
#include "network/network.h"
#include "network/switch2_rooms.h"
#include "qt_common/config/uisettings.h"
#include "ui_direct_connect.h"
#include "yuzu/multiplayer/direct_connect.h"
#include "yuzu/multiplayer/message.h"
#include "yuzu/multiplayer/state.h"
#ifdef ENABLE_WEB_SERVICE
#include "web_service/announce_room_json.h"
#include "web_service/web_backend.h"
#endif

DirectConnectWindow::DirectConnectWindow(Core::System& system_, QWidget* parent)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint),
      ui(std::make_unique<Ui::DirectConnect>()), system{system_} {
    ui->setupUi(this);
    room_id = new QLineEdit(this);
    room_id->setObjectName(QStringLiteral("public_room_id"));
    room_id->setMaxLength(160);
    room_id->setPlaceholderText(tr("Paste the host's eglantine-room: ID"));
    auto* form = new QFormLayout;
    form->addRow(tr("Room ID"), room_id);
    ui->verticalLayout->insertLayout(0, form);
    auto* help = new QLabel(tr("Enter a public room ID, or leave it blank and use a server address "
                               "and port for an unlisted room. Join before starting the game."),
                            this);
    help->setWordWrap(true);
    ui->verticalLayout->insertWidget(0, help);
    connect(room_id, &QLineEdit::textChanged, this, [this](const QString& text) {
        ui->ip_container->setEnabled(text.trimmed().isEmpty());
    });
    connect(&watcher, &QFutureWatcher<RoomConnectionResult>::finished, this,
            &DirectConnectWindow::OnConnection);
    ui->nickname->setValidator(validation.GetNickname());
    auto nickname = UISettings::values.multiplayer_nickname.GetValue();
    if (nickname.empty()) {
        nickname = Settings::values.eden_username.GetValue();
    }
    ui->nickname->setText(QString::fromStdString(nickname.empty() ? "Player" : nickname));
    ui->ip->setValidator(validation.GetIP());
    ui->ip->setText(QString::fromStdString(UISettings::values.multiplayer_ip.GetValue()));
    ui->port->setValidator(validation.GetPort());
    ui->port->setText(QString::number(UISettings::values.multiplayer_port.GetValue()));
    ui->password->setEchoMode(QLineEdit::Password);
    connect(ui->connect, &QPushButton::clicked, this, &DirectConnectWindow::Connect);
    RetranslateUi();
}

DirectConnectWindow::~DirectConnectWindow() {
    watcher.waitForFinished();
}

void DirectConnectWindow::RetranslateUi() {
    ui->retranslateUi(this);
    setWindowTitle(tr("Join a Switch 2 room"));
}

void DirectConnectWindow::Connect() {
    if (static_cast<MultiplayerState*>(parentWidget())->IsConnecting()) {
        return;
    }
    if (system.IsPoweredOn()) {
        QMessageBox::information(this, tr("Stop the game first"),
                                 tr("Join a room before starting the game so its network sockets "
                                    "use the room connection."));
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
    if (!ui->nickname->hasAcceptableInput()) {
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::USERNAME_NOT_VALID);
        return;
    }
    const auto member = Network::GetRoomMember().lock();
    if (!member || member->GetState() == Network::RoomMember::State::Joining) {
        return;
    }
    if (member->IsConnected() && !static_cast<MultiplayerState*>(parentWidget())->OnCloseRoom()) {
        return;
    }
    const auto code = room_id->text().trimmed().toStdString();
    auto id = std::string_view{code};
    if (id.starts_with("eglantine-room:")) {
        id.remove_prefix(15);
    }
    if (!code.empty() && !Network::IsValidPublicRoomId(id)) {
        QMessageBox::warning(this, tr("Invalid room ID"),
                             tr("Paste a room ID copied by the host."));
        return;
    }
    if (code.empty() && (!ui->ip->hasAcceptableInput() || !ui->port->hasAcceptableInput() ||
                         ui->port->text().toUShort() == 0)) {
        QMessageBox::warning(this, tr("Invalid address"),
                             tr("Enter a server address and port (1-65535)."));
        return;
    }
    const auto nickname = ui->nickname->text().toStdString();
    const auto address = ui->ip->text().toStdString();
    const auto port = ui->port->text().toUShort();
    const auto password = ui->password->text().toStdString();
    const auto api = Settings::values.web_api_url.GetValue();
    const auto username = Settings::values.eden_username.GetValue();
    const auto token = Settings::values.eden_token.GetValue();
    UISettings::values.multiplayer_nickname = nickname;
    emit SaveConfig();
    BeginConnecting();
    watcher.setFuture(QtConcurrent::run([=]() -> RoomConnectionResult {
        try {
            auto host = address;
            auto host_port = port;
            std::string verify_uid;
            if (!code.empty()) {
#ifdef ENABLE_WEB_SERVICE
                WebService::RoomJson directory(api, username, token);
                std::string error;
                const auto rooms = directory.GetRoomList(&error);
                if (!error.empty()) {
                    return {QString::fromStdString(error)};
                }
                const auto* room = Network::FindSwitch2Room(rooms, code);
                if (!room) {
                    return {tr("Room ID not found among compatible Switch 2 rooms. It may be "
                               "closed, unlisted, or the directory may be unavailable.")};
                }
                host = room->ip;
                host_port = room->information.port;
                verify_uid = room->verify_uid;
#else
                return {tr("This build does not include the public room directory.")};
#endif
            }
            std::string jwt;
#ifdef ENABLE_WEB_SERVICE
            if (!verify_uid.empty() && !username.empty() && !token.empty()) {
                WebService::Client client(api, username, token);
                jwt = client.GetExternalJWT(verify_uid).returned_data;
            }
#endif
            member->Join(nickname, host.c_str(), host_port, 0, Network::NoPreferredIP, password,
                         jwt);
            return {{}, host, host_port};
        } catch (const std::exception&) {
            return {tr("The room directory returned an invalid response. Try again later.")};
        }
    }));
}

void DirectConnectWindow::BeginConnecting() {
    ui->connect->setEnabled(false);
    ui->connect->setText(tr("Connecting..."));
    room_id->setEnabled(false);
    ui->ip_container->setEnabled(false);
    ui->nickname->setEnabled(false);
    ui->password->setEnabled(false);
}

void DirectConnectWindow::EndConnecting() {
    ui->connect->setEnabled(true);
    ui->connect->setText(tr("Join Room"));
    room_id->setEnabled(true);
    ui->ip_container->setEnabled(room_id->text().trimmed().isEmpty());
    ui->nickname->setEnabled(true);
    ui->password->setEnabled(true);
}

void DirectConnectWindow::OnConnection() {
    const auto result = watcher.result();
    EndConnecting();
    if (!result.error.isEmpty()) {
        QMessageBox::warning(this, tr("Could not join room"), result.error);
        return;
    }
    UISettings::values.multiplayer_ip = result.address;
    UISettings::values.multiplayer_port = result.port;
    emit SaveConfig();
    if (auto member = Network::GetRoomMember().lock(); member && member->IsConnected()) {
        close();
    }
}
