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

#include <QDateTime>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

// Checks for updates with checkupdates (no root needed) and applies them with
// `pkexec pacman -Syu --noconfirm`, streaming pacman's output.
// With LINGMO_UPDATER_FAKE set, both commands are replaced by fake/fake-pacman.sh.
class UpdateManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QVariantList packages READ packages NOTIFY packagesChanged)
    Q_PROPERTY(int count READ count NOTIFY packagesChanged)
    Q_PROPERTY(QString downloadSize READ downloadSize NOTIFY downloadSizeChanged)
    Q_PROPERTY(QString log READ log NOTIFY logChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString stepText READ stepText NOTIFY progressChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY stateChanged)
    Q_PROPERTY(bool rebootRecommended READ rebootRecommended NOTIFY stateChanged)
    Q_PROPERTY(QString lastCheckText READ lastCheckText NOTIFY lastCheckChanged)
    Q_PROPERTY(bool fakeMode READ fakeMode CONSTANT)

public:
    enum State {
        Idle,          // never checked
        Checking,
        UpToDate,
        Available,
        CheckFailed,
        Updating,
        Updated,
        UpdateFailed,
    };
    Q_ENUM(State)

    explicit UpdateManager(QObject *parent = nullptr);

    State state() const { return m_state; }
    QVariantList packages() const { return m_packages; }
    int count() const { return m_packages.size(); }
    QString downloadSize() const { return m_downloadSize; }
    QString log() const { return m_log; }
    qreal progress() const { return m_progress; }
    QString stepText() const { return m_stepText; }
    QString errorMessage() const { return m_errorMessage; }
    bool rebootRecommended() const { return m_rebootRecommended; }
    QString lastCheckText() const;
    bool fakeMode() const { return !m_fakeMode.isEmpty(); }
    // Identifies the current set of pending updates (to notify only once per set)
    QString updatesKey() const;

public Q_SLOTS:
    void check();
    void update();
    void reboot();

Q_SIGNALS:
    void stateChanged();
    void packagesChanged();
    void downloadSizeChanged();
    void logChanged();
    void progressChanged();
    void lastCheckChanged();
    void checkFinished();
    void updateFinished(bool success);

private:
    void setState(State state, const QString &error = QString());
    void setStep(qreal progress, const QString &text);
    void onCheckFinished(int exitCode, QProcess::ExitStatus status);
    void queryDownloadSize();
    void onUpdateOutput();
    void onUpdateFinished(int exitCode, QProcess::ExitStatus status);
    void parseLine(const QString &line);
    void appendLog(const QString &text);
    QString fakeScript() const;

    State m_state = Idle;
    QString m_fakeMode;
    QVariantList m_packages;
    QString m_downloadSize;
    QString m_log;
    QString m_pending; // partial output line
    qreal m_progress = -1;
    QString m_stepText;
    QString m_errorMessage;
    bool m_rebootRecommended = false;
    QDateTime m_lastCheck;
    QStringList m_upgraded;
    int m_downloadTotal = 0;
    int m_downloaded = 0;
    bool m_inHooks = false;
    QProcess *m_process = nullptr;
    QTimer m_logTimer;
};
