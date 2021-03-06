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

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QSettings>
#include <QStyle>
#include <QStyleOption>

#include "AvatarProvider.h"
#include "RoomInfoListItem.h"
#include "TimelineViewManager.h"

class ImageItem;
class AudioItem;
class VideoItem;
class FileItem;
class Avatar;

class TimelineItem : public QWidget
{
        Q_OBJECT
public:
        TimelineItem(const mtx::events::RoomEvent<mtx::events::msg::Notice> &e,
                     bool with_sender,
                     QWidget *parent = 0);
        TimelineItem(const mtx::events::RoomEvent<mtx::events::msg::Text> &e,
                     bool with_sender,
                     QWidget *parent = 0);
        TimelineItem(const mtx::events::RoomEvent<mtx::events::msg::Emote> &e,
                     bool with_sender,
                     QWidget *parent = 0);

        // For local messages.
        // m.text & m.emote
        TimelineItem(mtx::events::MessageType ty,
                     const QString &userid,
                     QString body,
                     bool withSender,
                     QWidget *parent = 0);
        // m.image
        TimelineItem(ImageItem *item, const QString &userid, bool withSender, QWidget *parent = 0);
        TimelineItem(FileItem *item, const QString &userid, bool withSender, QWidget *parent = 0);
        TimelineItem(AudioItem *item, const QString &userid, bool withSender, QWidget *parent = 0);
        TimelineItem(VideoItem *item, const QString &userid, bool withSender, QWidget *parent = 0);

        TimelineItem(ImageItem *img,
                     const mtx::events::RoomEvent<mtx::events::msg::Image> &e,
                     bool with_sender,
                     QWidget *parent);
        TimelineItem(FileItem *file,
                     const mtx::events::RoomEvent<mtx::events::msg::File> &e,
                     bool with_sender,
                     QWidget *parent);
        TimelineItem(AudioItem *audio,
                     const mtx::events::RoomEvent<mtx::events::msg::Audio> &e,
                     bool with_sender,
                     QWidget *parent);
        TimelineItem(VideoItem *video,
                     const mtx::events::RoomEvent<mtx::events::msg::Video> &e,
                     bool with_sender,
                     QWidget *parent);

        void setUserAvatar(const QImage &pixmap);
        DescInfo descriptionMessage() const { return descriptionMsg_; }
        QString eventId() const { return event_id_; }

        ~TimelineItem();

protected:
        void paintEvent(QPaintEvent *event) override;

private:
        void init();

        template<class Widget>
        void setupLocalWidgetLayout(Widget *widget,
                                    const QString &userid,
                                    const QString &msgDescription,
                                    bool withSender);

        template<class Event, class Widget>
        void setupWidgetLayout(Widget *widget,
                               const Event &event,
                               const QString &msgDescription,
                               bool withSender);

        void generateBody(const QString &body);
        void generateBody(const QString &userid, const QString &body);
        void generateTimestamp(const QDateTime &time);
        QString descriptiveTime(const QDateTime &then);

        void setupAvatarLayout(const QString &userName);
        void setupSimpleLayout();

        QString replaceEmoji(const QString &body);
        QString event_id_;

        DescInfo descriptionMsg_;

        QHBoxLayout *topLayout_;
        QVBoxLayout *sideLayout_; // Avatar or Timestamp
        QVBoxLayout *mainLayout_; // Header & Message body

        QHBoxLayout *headerLayout_; // Username (&) Timestamp

        Avatar *userAvatar_;

        QFont font_;

        QLabel *timestamp_;
        QLabel *userName_;
        QLabel *body_;
};

template<class Widget>
void
TimelineItem::setupLocalWidgetLayout(Widget *widget,
                                     const QString &userid,
                                     const QString &msgDescription,
                                     bool withSender)
{
        auto displayName = TimelineViewManager::displayName(userid);
        auto timestamp   = QDateTime::currentDateTime();

        descriptionMsg_ = {
          "You", userid, QString(" %1").arg(msgDescription), descriptiveTime(timestamp)};

        generateTimestamp(timestamp);

        auto widgetLayout = new QHBoxLayout();
        widgetLayout->setContentsMargins(0, 5, 0, 0);
        widgetLayout->addWidget(widget);
        widgetLayout->addStretch(1);

        if (withSender) {
                generateBody(displayName, "");
                setupAvatarLayout(displayName);
                mainLayout_->addLayout(headerLayout_);

                AvatarProvider::resolve(userid, this);
        } else {
                setupSimpleLayout();
        }

        mainLayout_->addLayout(widgetLayout);
}

template<class Event, class Widget>
void
TimelineItem::setupWidgetLayout(Widget *widget,
                                const Event &event,
                                const QString &msgDescription,
                                bool withSender)
{
        init();

        event_id_         = QString::fromStdString(event.event_id);
        const auto sender = QString::fromStdString(event.sender);

        auto timestamp   = QDateTime::fromMSecsSinceEpoch(event.origin_server_ts);
        auto displayName = TimelineViewManager::displayName(sender);

        QSettings settings;
        descriptionMsg_ = {sender == settings.value("auth/user_id") ? "You" : displayName,
                           sender,
                           msgDescription,
                           descriptiveTime(QDateTime::fromMSecsSinceEpoch(event.origin_server_ts))};

        generateTimestamp(timestamp);

        auto widgetLayout = new QHBoxLayout();
        widgetLayout->setContentsMargins(0, 5, 0, 0);
        widgetLayout->addWidget(widget);
        widgetLayout->addStretch(1);

        if (withSender) {
                generateBody(displayName, "");
                setupAvatarLayout(displayName);

                mainLayout_->addLayout(headerLayout_);

                AvatarProvider::resolve(sender, this);
        } else {
                setupSimpleLayout();
        }

        mainLayout_->addLayout(widgetLayout);
}
