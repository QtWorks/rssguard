// This file is part of RSS Guard.
//
// Copyright (C) 2011-2017 by Martin Rotter <rotter.martinos@gmail.com>
//
// RSS Guard is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// RSS Guard is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with RSS Guard. If not, see <http://www.gnu.org/licenses/>.

#include "services/abstract/feed.h"

#include "definitions/definitions.h"
#include "miscellaneous/application.h"
#include "miscellaneous/mutex.h"
#include "miscellaneous/databasequeries.h"
#include "miscellaneous/feedreader.h"
#include "services/abstract/recyclebin.h"
#include "services/abstract/serviceroot.h"

#include <QThread>


Feed::Feed(RootItem* parent)
	: RootItem(parent), m_url(QString()), m_status(Normal), m_autoUpdateType(DefaultAutoUpdate),
	  m_autoUpdateInitialInterval(DEFAULT_AUTO_UPDATE_INTERVAL), m_autoUpdateRemainingInterval(DEFAULT_AUTO_UPDATE_INTERVAL),
	  m_totalCount(0), m_unreadCount(0) {
	setKind(RootItemKind::Feed);
	setAutoDelete(false);
}

Feed::~Feed() {
}

QList<Message> Feed::undeletedMessages() const {
	QSqlDatabase database = qApp->database()->connection(metaObject()->className(), DatabaseFactory::FromSettings);
	return DatabaseQueries::getUndeletedMessagesForFeed(database, customId(), getParentServiceRoot()->accountId());
}

QVariant Feed::data(int column, int role) const {
	switch (role) {
		case Qt::ToolTipRole:
			if (column == FDS_MODEL_TITLE_INDEX) {
				//: Tooltip for feed.
				return tr("%1"
				          "%2\n\n"
				          "Auto-update status: %3").arg(title(),
				                                        description().isEmpty() ? QString() : QString('\n') + description(),
				                                        getAutoUpdateStatusDescription());
			}

			else {
				return RootItem::data(column, role);
			}

		case Qt::ForegroundRole:
			switch (status()) {
				case NewMessages:
					return QColor(Qt::blue);

				case NetworkError:
				case ParsingError:
				case OtherError:
					return QColor(Qt::red);

				default:
					return QVariant();
			}

		default:
			return RootItem::data(column, role);
	}
}

int Feed::autoUpdateInitialInterval() const {
	return m_autoUpdateInitialInterval;
}

int Feed::countOfAllMessages() const {
	return m_totalCount;
}

int Feed::countOfUnreadMessages() const {
	return m_unreadCount;
}

void Feed::setCountOfAllMessages(int count_all_messages) {
	m_totalCount = count_all_messages;
}

void Feed::setCountOfUnreadMessages(int count_unread_messages) {
	if (status() == NewMessages && count_unread_messages < countOfUnreadMessages()) {
		setStatus(Normal);
	}

	m_unreadCount = count_unread_messages;
}

void Feed::setAutoUpdateInitialInterval(int auto_update_interval) {
	// If new initial auto-update interval is set, then
	// we should reset time that remains to the next auto-update.
	m_autoUpdateInitialInterval = auto_update_interval;
	m_autoUpdateRemainingInterval = auto_update_interval;
}

Feed::AutoUpdateType Feed::autoUpdateType() const {
	return m_autoUpdateType;
}

void Feed::setAutoUpdateType(Feed::AutoUpdateType auto_update_type) {
	m_autoUpdateType = auto_update_type;
}

int Feed::autoUpdateRemainingInterval() const {
	return m_autoUpdateRemainingInterval;
}

void Feed::setAutoUpdateRemainingInterval(int auto_update_remaining_interval) {
	m_autoUpdateRemainingInterval = auto_update_remaining_interval;
}

Feed::Status Feed::status() const {
	return m_status;
}

void Feed::setStatus(const Feed::Status& status) {
	m_status = status;
}

QString Feed::url() const {
	return m_url;
}

void Feed::setUrl(const QString& url) {
	m_url = url;
}

void Feed::updateCounts(bool including_total_count) {
	bool is_main_thread = QThread::currentThread() == qApp->thread();
	QSqlDatabase database = is_main_thread ?
	                        qApp->database()->connection(metaObject()->className(), DatabaseFactory::FromSettings) :
	                        qApp->database()->connection(QSL("feed_upd"), DatabaseFactory::FromSettings);
	int account_id = getParentServiceRoot()->accountId();

	if (including_total_count) {
		setCountOfAllMessages(DatabaseQueries::getMessageCountsForFeed(database, customId(), account_id, true));
	}

	setCountOfUnreadMessages(DatabaseQueries::getMessageCountsForFeed(database, customId(), account_id, false));
}

void Feed::run() {
	qDebug().nospace() << "Downloading new messages for feed "
	                   << customId() << " in thread: \'"
	                   << QThread::currentThreadId() << "\'.";
	// Save all cached data first.
	getParentServiceRoot()->saveAllCachedData();
	bool error_during_obtaining;
	QList<Message> msgs = obtainNewMessages(&error_during_obtaining);
	qDebug().nospace() << "Downloaded " << msgs.size() << " messages for feed "
	                   << customId() << " in thread: \'"
	                   << QThread::currentThreadId() << "\'.";

	// Now, do some general operations on messages (tweak encoding etc.).
	for (int i = 0; i < msgs.size(); i++) {
		// Also, make sure that HTML encoding, encoding of special characters, etc., is fixed.
		msgs[i].m_contents = QUrl::fromPercentEncoding(msgs[i].m_contents.toUtf8());
		msgs[i].m_author = msgs[i].m_author.toUtf8();
		// Sanitize title. Remove newlines etc.
		msgs[i].m_title = QUrl::fromPercentEncoding(msgs[i].m_title.toUtf8())
		                  // Replace all continuous white space.
		                  .replace(QRegExp(QSL("[\\s]{2,}")), QSL(" "))
		                  // Remove all newlines and leading white space.
		                  .remove(QRegExp(QSL("([\\n\\r])|(^\\s)")));
	}

	emit messagesObtained(msgs, error_during_obtaining);
}

int Feed::updateMessages(const QList<Message>& messages, bool error_during_obtaining) {
	QList<RootItem*> items_to_update;
	int updated_messages = 0;
	bool is_main_thread = QThread::currentThread() == qApp->thread();
	qDebug("Updating messages in DB. Main thread: '%s'.", qPrintable(is_main_thread ? "true" : "false"));

	if (!error_during_obtaining) {
		bool anything_updated = false;
		bool ok = true;

		if (!messages.isEmpty()) {
			int custom_id = customId();
			int account_id = getParentServiceRoot()->accountId();
			QSqlDatabase database = is_main_thread ?
			                        qApp->database()->connection(metaObject()->className(), DatabaseFactory::FromSettings) :
			                        qApp->database()->connection(QSL("feed_upd"), DatabaseFactory::FromSettings);
			updated_messages = DatabaseQueries::updateMessages(database, messages, custom_id, account_id, url(), &anything_updated, &ok);
		}

		if (ok) {
			setStatus(updated_messages > 0 ? NewMessages : Normal);
			updateCounts(true);

			if (getParentServiceRoot()->recycleBin() != nullptr && anything_updated) {
				getParentServiceRoot()->recycleBin()->updateCounts(true);
				items_to_update.append(getParentServiceRoot()->recycleBin());
			}
		}
	}

	items_to_update.append(this);
	getParentServiceRoot()->itemChanged(items_to_update);
	return updated_messages;
}

QString Feed::getAutoUpdateStatusDescription() const {
	QString auto_update_string;

	switch (autoUpdateType()) {
		case DontAutoUpdate:
			//: Describes feed auto-update status.
			auto_update_string = tr("does not use auto-update");
			break;

		case DefaultAutoUpdate:
			//: Describes feed auto-update status.
			auto_update_string = tr("uses global settings (%n minute(s) to next auto-update)", 0, qApp->feedReader()->autoUpdateRemainingInterval());
			break;

		case SpecificAutoUpdate:
		default:
			//: Describes feed auto-update status.
			auto_update_string = tr("uses specific settings (%n minute(s) to next auto-update)", 0, autoUpdateRemainingInterval());
			break;
	}

	return auto_update_string;
}
