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

#include "miscellaneous/systemfactory.h"

#include "network-web/networkfactory.h"
#include "miscellaneous/application.h"
#include "miscellaneous/systemfactory.h"

#if defined(Q_OS_WIN)
#include <QSettings>
#endif

#include <QString>
#include <QProcess>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFuture>
#include <QFileInfo>
#include <QDir>
#include <QFutureWatcher>
#include <QDesktopServices>


typedef QPair<UpdateInfo, QNetworkReply::NetworkError> UpdateCheck;

SystemFactory::SystemFactory(QObject* parent) : QObject(parent) {
}

SystemFactory::~SystemFactory() {
}

SystemFactory::AutoStartStatus SystemFactory::getAutoStartStatus() const {
	// User registry way to auto-start the application on Windows.
#if defined(Q_OS_WIN)
	QSettings registry_key(QSL("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
	                       QSettings::NativeFormat);
	const bool autostart_enabled = registry_key.value(QSL(APP_LOW_NAME),
	                                                  QString()).toString().replace(QL1C('\\'),
	                                                          QL1C('/')) ==
	                               Application::applicationFilePath();

	if (autostart_enabled) {
		return SystemFactory::Enabled;
	}

	else {
		return SystemFactory::Disabled;
	}

#elif defined(Q_OS_LINUX)
	// Use proper freedesktop.org way to auto-start the application on Linux.
	// INFO: http://standards.freedesktop.org/autostart-spec/latest/
	const QString desktop_file_location = getAutostartDesktopFileLocation();

	// No correct path was found.
	if (desktop_file_location.isEmpty()) {
		qWarning("Searching for auto-start function status failed. HOME variable not found.");
		return SystemFactory::Unavailable;
	}

	// We found correct path, now check if file exists and return correct status.
	if (QFile::exists(desktop_file_location)) {
		// File exists, we must read it and check if "Hidden" attribute is defined and what is its value.
		QSettings desktop_settings(desktop_file_location, QSettings::IniFormat);
		bool hidden_value = desktop_settings.value(QSL("Desktop Entry/Hidden"), false).toBool();
		return hidden_value ? SystemFactory::Disabled : SystemFactory::Enabled;
	}

	else {
		return SystemFactory::Disabled;
	}

#else
	// Disable auto-start functionality on unsupported platforms.
	return SystemFactory::Unavailable;
#endif
}

#if defined(Q_OS_LINUX)
QString SystemFactory::getAutostartDesktopFileLocation() const {
	const QString xdg_config_path(qgetenv("XDG_CONFIG_HOME"));
	QString desktop_file_location;

	if (!xdg_config_path.isEmpty()) {
		// XDG_CONFIG_HOME variable is specified. Look for .desktop file
		// in 'autostart' subdirectory.
		desktop_file_location = xdg_config_path + QSL("/autostart/") + APP_DESKTOP_ENTRY_FILE;
	}

	else {
		// Desired variable is not set, look for the default 'autostart' subdirectory.
		const QString home_directory(qgetenv("HOME"));

		if (!home_directory.isEmpty()) {
			// Home directory exists. Check if target .desktop file exists and
			// return according status.
			desktop_file_location = home_directory + QSL("/.config/autostart/") + APP_DESKTOP_ENTRY_FILE;
		}
	}

	return desktop_file_location;
}
#endif

bool SystemFactory::setAutoStartStatus(const AutoStartStatus& new_status) {
	const SystemFactory::AutoStartStatus current_status = SystemFactory::getAutoStartStatus();

	// Auto-start feature is not even available, exit.
	if (current_status == SystemFactory::Unavailable) {
		return false;
	}

#if defined(Q_OS_WIN)
	QSettings registry_key(QSL("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"), QSettings::NativeFormat);

	switch (new_status) {
		case SystemFactory::Enabled:
			registry_key.setValue(APP_LOW_NAME,
			                      Application::applicationFilePath().replace(QL1C('/'), QL1C('\\')));
			return true;

		case SystemFactory::Disabled:
			registry_key.remove(APP_LOW_NAME);
			return true;

		default:
			return false;
	}

#elif defined(Q_OS_LINUX)
	// Note that we expect here that no other program uses
	// "rssguard.desktop" desktop file.
	const QString destination_file = getAutostartDesktopFileLocation();
	const QString destination_folder = QFileInfo(destination_file).absolutePath();

	switch (new_status) {
		case SystemFactory::Enabled: {
			if (QFile::exists(destination_file)) {
				if (!QFile::remove(destination_file)) {
					return false;
				}
			}

			if (!QDir().mkpath(destination_folder)) {
				return false;
			}

			const QString source_autostart_desktop_file = QString(APP_DESKTOP_ENTRY_PATH) + QDir::separator() + APP_DESKTOP_SOURCE_ENTRY_FILE;
			return QFile::copy(source_autostart_desktop_file, destination_file);
		}

		case SystemFactory::Disabled:
			return QFile::remove(destination_file);

		default:
			return false;
	}

#else
	return false;
#endif
}

#if defined(Q_OS_WIN)
bool SystemFactory::removeTrolltechJunkRegistryKeys() {
	if (qApp->settings()->value(GROUP(General), SETTING(General::RemoveTrolltechJunk)).toBool()) {
		QSettings registry_key(QSL("HKEY_CURRENT_USER\\Software\\TrollTech"), QSettings::NativeFormat);
		registry_key.remove(QSL(""));
		registry_key.sync();
		return registry_key.status() == QSettings::NoError;
	}

	else {
		return false;
	}
}
#endif

QString SystemFactory::getUsername() const {
	QString name = qgetenv("USER");

	if (name.isEmpty()) {
		name = qgetenv("USERNAME");
	}

	if (name.isEmpty()) {
		name = tr("anonymous");
	}

	return name;
}

QPair<QList<UpdateInfo>, QNetworkReply::NetworkError> SystemFactory::checkForUpdates() const {
	QPair<QList<UpdateInfo>, QNetworkReply::NetworkError> result;
	QByteArray releases_json;
	result.second = NetworkFactory::performNetworkOperation(RELEASES_LIST, DOWNLOAD_TIMEOUT, QByteArray(), QString(),
	                                                        releases_json, QNetworkAccessManager::GetOperation).first;

	if (result.second == QNetworkReply::NoError) {
		result.first = parseUpdatesFile(releases_json);
	}

	return result;
}

bool SystemFactory::isVersionNewer(const QString& new_version, const QString& base_version) {
	QStringList base_version_tkn = base_version.split(QL1C('.'));
	QStringList new_version_tkn = new_version.split(QL1C('.'));

	while (!base_version_tkn.isEmpty() && !new_version_tkn.isEmpty()) {
		const int base_number = base_version_tkn.takeFirst().toInt();
		const int new_number = new_version_tkn.takeFirst().toInt();

		if (new_number > base_number) {
			// New version is indeed higher thatn current version.
			return true;
		}

		else if (new_number < base_number) {
			return false;
		}
	}

	// Versions are either the same or they have unequal sizes.
	if (base_version_tkn.isEmpty() && new_version_tkn.isEmpty()) {
		// Versions are the same.
		return false;
	}

	else {
		if (new_version_tkn.isEmpty()) {
			return false;
		}

		else {
			return new_version_tkn.join(QString()).toInt() > 0;
		}
	}
}

bool SystemFactory::isVersionEqualOrNewer(const QString& new_version, const QString& base_version) {
	return new_version == base_version || isVersionNewer(new_version, base_version);
}

bool SystemFactory::openFolderFile(const QString& file_path) {
#if defined(Q_OS_WIN)
	return QProcess::startDetached(QString("explorer.exe /select, \"") + QDir::toNativeSeparators(file_path) + "\"");
#else
	const QString folder = QDir::toNativeSeparators(QFileInfo(file_path).absoluteDir().absolutePath());
	return QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
#endif
}

QList<UpdateInfo> SystemFactory::parseUpdatesFile(const QByteArray& updates_file) const {
	QList<UpdateInfo> updates;
	QJsonArray document = QJsonDocument::fromJson(updates_file).array();

	for (int i = 0; i < document.size(); i++) {
		QJsonObject release = document.at(i).toObject();
		UpdateInfo update;
		update.m_date = QDateTime::fromString(release["published_at"].toString(), QSL("yyyy-MM-ddTHH:mm:ssZ"));
		update.m_availableVersion = release["tag_name"].toString();
		update.m_changes = release["body"].toString();
		QJsonArray assets = release["assets"].toArray();

		for (int j = 0; j < assets.size(); j++) {
			QJsonObject asset = assets.at(j).toObject();
			UpdateUrl url;
			url.m_fileUrl = asset["browser_download_url"].toString();
			url.m_name = asset["name"].toString();
			url.m_size = asset["size"].toVariant().toString() + tr(" bytes");
			update.m_urls.append(url);
		}

		updates.append(update);
	}

	qSort(updates.begin(), updates.end(), [](const UpdateInfo & a, const UpdateInfo & b) -> bool {
		return a.m_date > b.m_date;
	});
	return updates;
}

void SystemFactory::checkForUpdatesOnStartup() {
	const QPair<QList<UpdateInfo>, QNetworkReply::NetworkError> updates = checkForUpdates();

	if (!updates.first.isEmpty() && updates.second == QNetworkReply::NoError && isVersionNewer(updates.first.at(0).m_availableVersion,
	        APP_VERSION)) {
		qApp->showGuiMessage(tr("New version available"),
		                     tr("Click the bubble for more information."),
		                     QSystemTrayIcon::Information,
		                     nullptr, true, qApp->mainFormWidget(), SLOT(showUpdates()));
	}
}
