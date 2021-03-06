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

#include <stdexcept>

#include <QDebug>
#include <QFile>
#include <QStandardPaths>

#include "Cache.h"
#include "RoomState.h"

static const lmdb::val NEXT_BATCH_KEY("next_batch");
static const lmdb::val transactionID("transaction_id");

Cache::Cache(const QString &userId)
  : env_{nullptr}
  , stateDb_{0}
  , roomDb_{0}
  , isMounted_{false}
  , userId_{userId}
{}

void
Cache::setup()
{
        qDebug() << "Setting up cache";

        auto statePath = QString("%1/%2/state")
                           .arg(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
                           .arg(QString::fromUtf8(userId_.toUtf8().toHex()));

        cacheDirectory_ = QString("%1/%2")
                            .arg(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
                            .arg(QString::fromUtf8(userId_.toUtf8().toHex()));

        bool isInitial = !QFile::exists(statePath);

        env_ = lmdb::env::create();
        env_.set_mapsize(128UL * 1024UL * 1024UL); /* 128 MB */
        env_.set_max_dbs(1024UL);

        if (isInitial) {
                qDebug() << "First time initializing LMDB";

                if (!QDir().mkpath(statePath)) {
                        throw std::runtime_error(
                          ("Unable to create state directory:" + statePath).toStdString().c_str());
                }
        }

        try {
                env_.open(statePath.toStdString().c_str());
        } catch (const lmdb::error &e) {
                if (e.code() != MDB_VERSION_MISMATCH && e.code() != MDB_INVALID) {
                        throw std::runtime_error("LMDB initialization failed" +
                                                 std::string(e.what()));
                }

                qWarning() << "Resetting cache due to LMDB version mismatch:" << e.what();

                QDir stateDir(statePath);

                for (const auto &file : stateDir.entryList(QDir::NoDotAndDotDot)) {
                        if (!stateDir.remove(file))
                                throw std::runtime_error(
                                  ("Unable to delete file " + file).toStdString().c_str());
                }

                env_.open(statePath.toStdString().c_str());
        }

        auto txn = lmdb::txn::begin(env_);
        stateDb_ = lmdb::dbi::open(txn, "state", MDB_CREATE);
        roomDb_  = lmdb::dbi::open(txn, "rooms", MDB_CREATE);

        txn.commit();

        isMounted_ = true;
}

void
Cache::setState(const QString &nextBatchToken, const QMap<QString, RoomState> &states)
{
        if (!isMounted_)
                return;

        try {
                auto txn = lmdb::txn::begin(env_);

                setNextBatchToken(txn, nextBatchToken);

                for (auto it = states.constBegin(); it != states.constEnd(); ++it)
                        insertRoomState(txn, it.key(), it.value());

                txn.commit();
        } catch (const lmdb::error &e) {
                qCritical() << "The cache couldn't be updated: " << e.what();

                unmount();
                deleteData();
        }
}

void
Cache::insertRoomState(lmdb::txn &txn, const QString &roomid, const RoomState &state)
{
        auto stateEvents = state.serialize();
        auto id          = roomid.toUtf8();

        lmdb::dbi_put(txn, roomDb_, lmdb::val(id.data(), id.size()), lmdb::val(stateEvents));

        for (const auto &membership : state.memberships) {
                lmdb::dbi membersDb =
                  lmdb::dbi::open(txn, roomid.toStdString().c_str(), MDB_CREATE);

                // The user_id this membership event relates to, is used
                // as the index on the membership database.
                auto key = membership.second.state_key;

                // Serialize membership event.
                nlohmann::json data     = membership.second;
                std::string memberEvent = data.dump();

                switch (membership.second.content.membership) {
                // We add or update (e.g invite -> join) a new user to the membership
                // list.
                case mtx::events::state::Membership::Invite:
                case mtx::events::state::Membership::Join: {
                        lmdb::dbi_put(txn, membersDb, lmdb::val(key), lmdb::val(memberEvent));
                        break;
                }
                // We remove the user from the membership list.
                case mtx::events::state::Membership::Leave:
                case mtx::events::state::Membership::Ban: {
                        lmdb::dbi_del(txn, membersDb, lmdb::val(key), lmdb::val(memberEvent));
                        break;
                }
                case mtx::events::state::Membership::Knock: {
                        qWarning()
                          << "Skipping knock membership" << roomid << QString::fromStdString(key);
                        break;
                }
                }
        }
}

void
Cache::removeRoom(const QString &roomid)
{
        if (!isMounted_)
                return;

        auto txn = lmdb::txn::begin(env_, nullptr, 0);

        lmdb::dbi_del(txn, roomDb_, lmdb::val(roomid.toUtf8(), roomid.toUtf8().size()), nullptr);

        txn.commit();
}

QMap<QString, RoomState>
Cache::states()
{
        QMap<QString, RoomState> states;

        auto txn    = lmdb::txn::begin(env_, nullptr, MDB_RDONLY);
        auto cursor = lmdb::cursor::open(txn, roomDb_);

        std::string room;
        std::string stateData;

        // Retrieve all the room names.
        while (cursor.get(room, stateData, MDB_NEXT)) {
                auto roomid = QString::fromUtf8(room.data(), room.size());
                auto json   = nlohmann::json::parse(stateData);

                RoomState state;
                state.parse(json);

                auto memberDb = lmdb::dbi::open(txn, roomid.toStdString().c_str(), MDB_CREATE);
                std::map<std::string, mtx::events::StateEvent<mtx::events::state::Member>> members;

                auto memberCursor = lmdb::cursor::open(txn, memberDb);

                std::string memberId;
                std::string memberContent;

                while (memberCursor.get(memberId, memberContent, MDB_NEXT)) {
                        auto userid = QString::fromStdString(memberId);

                        try {
                                auto data = nlohmann::json::parse(memberContent);
                                mtx::events::StateEvent<mtx::events::state::Member> member = data;
                                members.emplace(memberId, member);
                        } catch (std::exception &e) {
                                qWarning() << "Fault while parsing member event" << e.what()
                                           << QString::fromStdString(memberContent);
                                continue;
                        }
                }

                qDebug() << members.size() << "members for" << roomid;

                state.memberships = members;
                states.insert(roomid, state);
        }

        qDebug() << "Retrieved" << states.size() << "rooms";

        cursor.close();

        txn.commit();

        return states;
}

void
Cache::setNextBatchToken(lmdb::txn &txn, const QString &token)
{
        auto value = token.toUtf8();

        lmdb::dbi_put(txn, stateDb_, NEXT_BATCH_KEY, lmdb::val(value.data(), value.size()));
}

bool
Cache::isInitialized() const
{
        auto txn = lmdb::txn::begin(env_, nullptr, MDB_RDONLY);
        lmdb::val token;

        bool res = lmdb::dbi_get(txn, stateDb_, NEXT_BATCH_KEY, token);

        txn.commit();

        return res;
}

QString
Cache::nextBatchToken() const
{
        auto txn = lmdb::txn::begin(env_, nullptr, MDB_RDONLY);
        lmdb::val token;

        lmdb::dbi_get(txn, stateDb_, NEXT_BATCH_KEY, token);

        txn.commit();

        return QString::fromUtf8(token.data(), token.size());
}

void
Cache::deleteData()
{
        qInfo() << "Deleting cache data";

        if (!cacheDirectory_.isEmpty())
                QDir(cacheDirectory_).removeRecursively();
}
