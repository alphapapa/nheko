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

#include <QFontDatabase>
#include <QRegExp>
#include <QTextEdit>

#include "Avatar.h"
#include "Config.h"

#include "timeline/TimelineItem.h"
#include "timeline/widgets/AudioItem.h"
#include "timeline/widgets/FileItem.h"
#include "timeline/widgets/ImageItem.h"
#include "timeline/widgets/VideoItem.h"

static const QRegExp URL_REGEX("((?:https?|ftp)://\\S+)");
static const QString URL_HTML = "<a href=\"\\1\">\\1</a>";

void
TimelineItem::init()
{
        userAvatar_ = nullptr;
        timestamp_  = nullptr;
        userName_   = nullptr;
        body_       = nullptr;

        font_.setPixelSize(conf::fontSize);

        QFontMetrics fm(font_);

        topLayout_    = new QHBoxLayout(this);
        sideLayout_   = new QVBoxLayout();
        mainLayout_   = new QVBoxLayout();
        headerLayout_ = new QHBoxLayout();

        topLayout_->setContentsMargins(conf::timeline::msgMargin, conf::timeline::msgMargin, 0, 0);
        topLayout_->setSpacing(0);

        topLayout_->addLayout(sideLayout_);
        topLayout_->addLayout(mainLayout_, 1);

        sideLayout_->setMargin(0);
        sideLayout_->setSpacing(0);

        mainLayout_->setContentsMargins(conf::timeline::headerLeftMargin, 0, 0, 0);
        mainLayout_->setSpacing(0);

        headerLayout_->setMargin(0);
        headerLayout_->setSpacing(conf::timeline::headerSpacing);
}

/*
 * For messages created locally.
 */
TimelineItem::TimelineItem(mtx::events::MessageType ty,
                           const QString &userid,
                           QString body,
                           bool withSender,
                           QWidget *parent)
  : QWidget(parent)
{
        init();

        auto displayName = TimelineViewManager::displayName(userid);
        auto timestamp   = QDateTime::currentDateTime();

        if (ty == mtx::events::MessageType::Emote) {
                body            = QString("* %1 %2").arg(displayName).arg(body);
                descriptionMsg_ = {"", userid, body, descriptiveTime(timestamp)};
        } else {
                descriptionMsg_ = {
                  "You: ", userid, body, descriptiveTime(QDateTime::currentDateTime())};
        }

        body = body.toHtmlEscaped();
        body.replace(URL_REGEX, URL_HTML);
        body.replace("\n", "<br/>");
        generateTimestamp(timestamp);

        if (withSender) {
                generateBody(displayName, body);
                setupAvatarLayout(displayName);
                mainLayout_->addLayout(headerLayout_);

                AvatarProvider::resolve(userid, this);
        } else {
                generateBody(body);
                setupSimpleLayout();
        }

        mainLayout_->addWidget(body_);
}

TimelineItem::TimelineItem(ImageItem *image,
                           const QString &userid,
                           bool withSender,
                           QWidget *parent)
  : QWidget{parent}
{
        init();

        setupLocalWidgetLayout<ImageItem>(image, userid, "sent an image", withSender);
}

TimelineItem::TimelineItem(FileItem *file, const QString &userid, bool withSender, QWidget *parent)
  : QWidget{parent}
{
        init();

        setupLocalWidgetLayout<FileItem>(file, userid, "sent a file", withSender);
}

TimelineItem::TimelineItem(AudioItem *audio,
                           const QString &userid,
                           bool withSender,
                           QWidget *parent)
  : QWidget{parent}
{
        init();

        setupLocalWidgetLayout<AudioItem>(audio, userid, "sent an audio clip", withSender);
}

TimelineItem::TimelineItem(VideoItem *video,
                           const QString &userid,
                           bool withSender,
                           QWidget *parent)
  : QWidget{parent}
{
        init();

        setupLocalWidgetLayout<VideoItem>(video, userid, "sent a video clip", withSender);
}

TimelineItem::TimelineItem(ImageItem *image,
                           const mtx::events::RoomEvent<mtx::events::msg::Image> &event,
                           bool with_sender,
                           QWidget *parent)
  : QWidget(parent)
{
        setupWidgetLayout<mtx::events::RoomEvent<mtx::events::msg::Image>, ImageItem>(
          image, event, " sent an image", with_sender);
}

TimelineItem::TimelineItem(FileItem *file,
                           const mtx::events::RoomEvent<mtx::events::msg::File> &event,
                           bool with_sender,
                           QWidget *parent)
  : QWidget(parent)
{
        setupWidgetLayout<mtx::events::RoomEvent<mtx::events::msg::File>, FileItem>(
          file, event, " sent a file", with_sender);
}

TimelineItem::TimelineItem(AudioItem *audio,
                           const mtx::events::RoomEvent<mtx::events::msg::Audio> &event,
                           bool with_sender,
                           QWidget *parent)
  : QWidget(parent)
{
        setupWidgetLayout<mtx::events::RoomEvent<mtx::events::msg::Audio>, AudioItem>(
          audio, event, " sent an audio clip", with_sender);
}

TimelineItem::TimelineItem(VideoItem *video,
                           const mtx::events::RoomEvent<mtx::events::msg::Video> &event,
                           bool with_sender,
                           QWidget *parent)
  : QWidget(parent)
{
        setupWidgetLayout<mtx::events::RoomEvent<mtx::events::msg::Video>, VideoItem>(
          video, event, " sent a video clip", with_sender);
}

/*
 * Used to display remote notice messages.
 */
TimelineItem::TimelineItem(const mtx::events::RoomEvent<mtx::events::msg::Notice> &event,
                           bool with_sender,
                           QWidget *parent)
  : QWidget(parent)
{
        init();

        event_id_         = QString::fromStdString(event.event_id);
        const auto sender = QString::fromStdString(event.sender);

        descriptionMsg_ = {TimelineViewManager::displayName(sender),
                           sender,
                           " sent a notification",
                           descriptiveTime(QDateTime::fromMSecsSinceEpoch(event.origin_server_ts))};

        auto body      = QString::fromStdString(event.content.body).trimmed().toHtmlEscaped();
        auto timestamp = QDateTime::fromMSecsSinceEpoch(event.origin_server_ts);

        generateTimestamp(timestamp);

        body.replace(URL_REGEX, URL_HTML);
        body.replace("\n", "<br/>");
        body = "<i>" + body + "</i>";

        if (with_sender) {
                auto displayName = TimelineViewManager::displayName(sender);

                generateBody(displayName, body);
                setupAvatarLayout(displayName);

                mainLayout_->addLayout(headerLayout_);

                AvatarProvider::resolve(sender, this);
        } else {
                generateBody(body);
                setupSimpleLayout();
        }

        mainLayout_->addWidget(body_);
}

/*
 * Used to display remote emote messages.
 */
TimelineItem::TimelineItem(const mtx::events::RoomEvent<mtx::events::msg::Emote> &event,
                           bool with_sender,
                           QWidget *parent)
  : QWidget(parent)
{
        init();

        event_id_         = QString::fromStdString(event.event_id);
        const auto sender = QString::fromStdString(event.sender);

        auto body        = QString::fromStdString(event.content.body).trimmed();
        auto timestamp   = QDateTime::fromMSecsSinceEpoch(event.origin_server_ts);
        auto displayName = TimelineViewManager::displayName(sender);
        auto emoteMsg    = QString("* %1 %2").arg(displayName).arg(body);

        descriptionMsg_ = {"",
                           sender,
                           emoteMsg,
                           descriptiveTime(QDateTime::fromMSecsSinceEpoch(event.origin_server_ts))};

        generateTimestamp(timestamp);
        emoteMsg = emoteMsg.toHtmlEscaped();
        emoteMsg.replace(URL_REGEX, URL_HTML);
        emoteMsg.replace("\n", "<br/>");

        if (with_sender) {
                generateBody(displayName, emoteMsg);
                setupAvatarLayout(displayName);
                mainLayout_->addLayout(headerLayout_);

                AvatarProvider::resolve(sender, this);
        } else {
                generateBody(emoteMsg);
                setupSimpleLayout();
        }

        mainLayout_->addWidget(body_);
}

/*
 * Used to display remote text messages.
 */
TimelineItem::TimelineItem(const mtx::events::RoomEvent<mtx::events::msg::Text> &event,
                           bool with_sender,
                           QWidget *parent)
  : QWidget(parent)
{
        init();

        event_id_         = QString::fromStdString(event.event_id);
        const auto sender = QString::fromStdString(event.sender);

        auto body        = QString::fromStdString(event.content.body).trimmed();
        auto timestamp   = QDateTime::fromMSecsSinceEpoch(event.origin_server_ts);
        auto displayName = TimelineViewManager::displayName(sender);

        QSettings settings;
        descriptionMsg_ = {sender == settings.value("auth/user_id") ? "You" : displayName,
                           sender,
                           QString(": %1").arg(body),
                           descriptiveTime(QDateTime::fromMSecsSinceEpoch(event.origin_server_ts))};

        generateTimestamp(timestamp);

        body = body.toHtmlEscaped();
        body.replace(URL_REGEX, URL_HTML);
        body.replace("\n", "<br/>");

        if (with_sender) {
                generateBody(displayName, body);
                setupAvatarLayout(displayName);

                mainLayout_->addLayout(headerLayout_);

                AvatarProvider::resolve(sender, this);
        } else {
                generateBody(body);
                setupSimpleLayout();
        }

        mainLayout_->addWidget(body_);
}

// Only the body is displayed.
void
TimelineItem::generateBody(const QString &body)
{
        QString content("<span> %1 </span>");

        body_ = new QLabel(this);
        body_->setFont(font_);
        body_->setWordWrap(true);
        body_->setText(content.arg(replaceEmoji(body)));
        body_->setMargin(0);

        body_->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextBrowserInteraction);
        body_->setOpenExternalLinks(true);
}

// The username/timestamp is displayed along with the message body.
void
TimelineItem::generateBody(const QString &userid, const QString &body)
{
        auto sender = userid;

        if (userid.startsWith("@")) {
                // TODO: Fix this by using a UserId type.
                if (userid.split(":")[0].split("@").size() > 1)
                        sender = userid.split(":")[0].split("@")[1];
        }

        QFont usernameFont = font_;
        usernameFont.setBold(true);

        userName_ = new QLabel(this);
        userName_->setFont(usernameFont);
        userName_->setText(sender);

        if (body.isEmpty())
                return;

        body_ = new QLabel(this);
        body_->setFont(font_);
        body_->setWordWrap(true);
        body_->setText(QString("<span> %1 </span>").arg(replaceEmoji(body)));
        body_->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextBrowserInteraction);
        body_->setOpenExternalLinks(true);
        body_->setMargin(0);
}

void
TimelineItem::generateTimestamp(const QDateTime &time)
{
        QFont timestampFont;
        timestampFont.setPixelSize(conf::timeline::fonts::timestamp);

        QFontMetrics fm(timestampFont);
        int topMargin = QFontMetrics(font_).ascent() - fm.ascent();

        timestamp_ = new QLabel(this);
        timestamp_->setFont(timestampFont);
        timestamp_->setText(time.toString("HH:mm"));
        timestamp_->setContentsMargins(0, topMargin, 0, 0);
        timestamp_->setStyleSheet(
          QString("font-size: %1px;").arg(conf::timeline::fonts::timestamp));
}

QString
TimelineItem::replaceEmoji(const QString &body)
{
        QString fmtBody = "";

        QVector<uint> utf32_string = body.toUcs4();

        for (auto &code : utf32_string) {
                // TODO: Be more precise here.
                if (code > 9000)
                        fmtBody += QString("<span style=\"font-family: Emoji "
                                           "One; font-size: %1px\">")
                                     .arg(conf::emojiSize) +
                                   QString::fromUcs4(&code, 1) + "</span>";
                else
                        fmtBody += QString::fromUcs4(&code, 1);
        }

        return fmtBody;
}

void
TimelineItem::setupAvatarLayout(const QString &userName)
{
        topLayout_->setContentsMargins(conf::timeline::msgMargin, conf::timeline::msgMargin, 0, 0);

        userAvatar_ = new Avatar(this);
        userAvatar_->setLetter(QChar(userName[0]).toUpper());
        userAvatar_->setSize(conf::timeline::avatarSize);

        // TODO: The provided user name should be a UserId class
        if (userName[0] == '@' && userName.size() > 1)
                userAvatar_->setLetter(QChar(userName[1]).toUpper());

        sideLayout_->addWidget(userAvatar_);
        sideLayout_->addStretch(1);

        headerLayout_->addWidget(userName_);
        headerLayout_->addWidget(timestamp_, 1);
}

void
TimelineItem::setupSimpleLayout()
{
        sideLayout_->addWidget(timestamp_);

        // Keep only the time in plain text.
        QTextEdit htmlText(timestamp_->text());
        QString plainText = htmlText.toPlainText();

        timestamp_->adjustSize();

        // Align the end of the avatar bubble with the end of the timestamp for
        // messages with and without avatar. Otherwise their bodies would not be
        // aligned.
        int tsWidth = timestamp_->fontMetrics().width(plainText);
        int offset  = std::max(0, conf::timeline::avatarSize - tsWidth);

        int defaultFontHeight = QFontMetrics(font_).ascent();

        timestamp_->setAlignment(Qt::AlignTop);
        timestamp_->setContentsMargins(
          offset + 1, defaultFontHeight - timestamp_->fontMetrics().ascent(), 0, 0);
        topLayout_->setContentsMargins(
          conf::timeline::msgMargin, conf::timeline::msgMargin / 3, 0, 0);
}

void
TimelineItem::setUserAvatar(const QImage &avatar)
{
        if (userAvatar_ == nullptr)
                return;

        userAvatar_->setImage(avatar);
}

QString
TimelineItem::descriptiveTime(const QDateTime &then)
{
        auto now = QDateTime::currentDateTime();

        auto days = then.daysTo(now);

        if (days == 0)
                return then.toString("HH:mm");
        else if (days < 2)
                return QString("Yesterday");
        else if (days < 365)
                return then.toString("dd/MM");

        return then.toString("dd/MM/yy");
}

TimelineItem::~TimelineItem() {}

void
TimelineItem::paintEvent(QPaintEvent *)
{
        QStyleOption opt;
        opt.init(this);
        QPainter p(this);
        style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
