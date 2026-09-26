/*
 * Copyright (C) 2026 LingmoOS Team.
 *
 * Author:     devalexandre <alexandre@dev2learn.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
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

#include <QObject>
#include <QPointer>
#include <QTimer>

class QMenu;
class QNetworkAccessManager;
class QQmlApplicationEngine;
class QQuickWindow;
class QSystemTrayIcon;
class UpdateManager;

// The resident part: periodic checks, tray icon, notifications and the window.
// Exported on the session bus as com.lingmo.Updater /Updater.
class UpdaterApp : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.lingmo.Updater")

public:
    UpdaterApp(bool resident, QObject *parent = nullptr);
    ~UpdaterApp() override;

public Q_SLOTS:
    void show();
    void checkNow();

private Q_SLOTS:
    void onNotificationAction(uint id, const QString &action);
    // logind: back from suspend
    void onPrepareForSleep(bool sleeping);

private:
    void updateTray();
    void onCheckFinished();
    void onUpdateFinished(bool success);
    void notify(const QString &summary, const QString &body, bool withAction);
    bool windowVisible() const;
    // Asks the [archlingmo] server whether its database changed since last time
    void pollRepo();
    void maybeQuit();

    bool m_resident;
    UpdateManager *m_manager;
    QSystemTrayIcon *m_tray;
    QMenu *m_menu;
    QTimer m_timer;
    QTimer m_repoTimer;
    QNetworkAccessManager *m_network = nullptr;
    QString m_repoDb;       // URL of archlingmo.db
    QByteArray m_repoStamp; // its ETag or Last-Modified when last seen
    QQmlApplicationEngine *m_engine = nullptr;
    QPointer<QQuickWindow> m_window;
    QString m_notifiedKey;
    uint m_notificationId = 0;
};
