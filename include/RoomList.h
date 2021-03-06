/*
 * nheko Copyright (C) 2017  Konstantinos Sideris <siderisk@auth.gr>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QMap>
#include <QPushButton>
#include <QScrollArea>
#include <QSharedPointer>
#include <QVBoxLayout>
#include <QWidget>

#include "dialogs/LeaveRoom.h"

class LeaveRoomDialog;
class MatrixClient;
class OverlayModal;
class RoomInfoListItem;
class RoomSettings;
class RoomState;
class Sync;
struct DescInfo;

class RoomList : public QWidget
{
        Q_OBJECT

public:
        RoomList(QSharedPointer<MatrixClient> client, QWidget *parent = 0);
        ~RoomList();

        void setInitialRooms(const QMap<QString, QSharedPointer<RoomSettings>> &settings,
                             const QMap<QString, RoomState> &states);
        void sync(const QMap<QString, RoomState> &states,
                  QMap<QString, QSharedPointer<RoomSettings>> &settings);

        void clear();

        void addRoom(const QMap<QString, QSharedPointer<RoomSettings>> &settings,
                     const RoomState &state,
                     const QString &room_id);
        void removeRoom(const QString &room_id, bool reset);

signals:
        void roomChanged(const QString &room_id);
        void totalUnreadMessageCountUpdated(int count);

public slots:
        void updateRoomAvatar(const QString &roomid, const QPixmap &img);
        void highlightSelectedRoom(const QString &room_id);
        void updateUnreadMessageCount(const QString &roomid, int count);
        void updateRoomDescription(const QString &roomid, const DescInfo &info);
        void closeJoinRoomDialog(bool isJoining, QString roomAlias);
        void openLeaveRoomDialog(const QString &room_id);
        void closeLeaveRoomDialog(bool leaving, const QString &room_id);
        void clearRoomMessageCount(const QString &room_id);

protected:
        void paintEvent(QPaintEvent *event) override;

private:
        void calculateUnreadMessageCount();

        QVBoxLayout *topLayout_;
        QVBoxLayout *contentsLayout_;
        QScrollArea *scrollArea_;
        QWidget *scrollAreaContents_;

        QPushButton *joinRoomButton_;

        OverlayModal *joinRoomModal_;

        QSharedPointer<OverlayModal> leaveRoomModal_;
        QSharedPointer<dialogs::LeaveRoom> leaveRoomDialog_;

        QMap<QString, QSharedPointer<RoomInfoListItem>> rooms_;

        QSharedPointer<MatrixClient> client_;
};
