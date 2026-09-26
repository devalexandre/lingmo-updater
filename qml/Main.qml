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

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LingmoUI.CompatibleModule 3.0 as LingmoUI
import Lingmo.Updater

LingmoUI.Window {
    id: root

    width: 680
    height: 580
    minimumWidth: 540
    minimumHeight: 460
    visible: false
    title: qsTr("Updates")

    background.opacity: LingmoUI.Theme.darkMode ? 0.88 : 0.97
    header.height: 40

    readonly property int st: updater.state
    readonly property bool busy: st === UpdateManager.Checking || st === UpdateManager.Updating
    readonly property bool showList: updater.count > 0
                                     && (st === UpdateManager.Available || st === UpdateManager.Updating
                                         || st === UpdateManager.Updated || st === UpdateManager.UpdateFailed)
    property bool detailsOpen: false

    Connections {
        target: updater
        function onStateChanged() {
            if (updater.state === UpdateManager.UpdateFailed && updater.log.length > 0)
                root.detailsOpen = true
        }
    }

    LingmoUI.WindowBlur {
        view: root
        geometry: Qt.rect(root.x, root.y, root.width, root.height)
        windowRadius: root.windowRadius
        enabled: true
    }

    function titleText() {
        switch (st) {
        case UpdateManager.Available: return qsTr("%n update(s) available", "", updater.count)
        case UpdateManager.UpToDate: return qsTr("Your system is up to date")
        case UpdateManager.CheckFailed: return qsTr("Could not check for updates")
        case UpdateManager.Updating: return qsTr("Updating the system…")
        case UpdateManager.Updated: return qsTr("System updated")
        case UpdateManager.UpdateFailed: return qsTr("Update failed")
        default: return qsTr("Checking for updates…")
        }
    }

    function subtitleText() {
        const checked = updater.lastCheckText ? qsTr("Last checked at %1").arg(updater.lastCheckText) : ""
        switch (st) {
        case UpdateManager.Available:
            if (updater.errorMessage)
                return updater.errorMessage
            return updater.downloadSize ? qsTr("Download size: %1").arg(updater.downloadSize) + "  ·  " + checked
                                        : checked
        case UpdateManager.UpToDate: return checked
        case UpdateManager.CheckFailed:
        case UpdateManager.UpdateFailed: return updater.errorMessage
        case UpdateManager.Updating: return updater.stepText
        case UpdateManager.Updated:
            return updater.rebootRecommended
                    ? qsTr("Important system components were updated. Restart the computer to finish.")
                    : qsTr("All updates were installed.")
        default: return qsTr("Contacting the package servers…")
        }
    }

    function iconName() {
        switch (st) {
        case UpdateManager.UpToDate:
        case UpdateManager.Updated: return "emblem-default"
        case UpdateManager.CheckFailed:
        case UpdateManager.UpdateFailed: return "dialog-error"
        default: return "system-software-update"
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: LingmoUI.Units.largeSpacing * 2
        anchors.rightMargin: LingmoUI.Units.largeSpacing * 2
        anchors.bottomMargin: LingmoUI.Units.largeSpacing * 2
        anchors.topMargin: LingmoUI.Units.smallSpacing
        spacing: LingmoUI.Units.largeSpacing

        // Summary: state, actions and progress
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: summary.implicitHeight + LingmoUI.Units.largeSpacing * 3
            radius: LingmoUI.Theme.mediumRadius
            color: LingmoUI.Theme.secondBackgroundColor

            ColumnLayout {
                id: summary
                anchors.fill: parent
                anchors.margins: LingmoUI.Units.largeSpacing * 1.5
                spacing: LingmoUI.Units.largeSpacing

                RowLayout {
                    spacing: LingmoUI.Units.largeSpacing * 1.5
                    Layout.fillWidth: true

                    LingmoUI.IconItem {
                        Layout.preferredWidth: 48
                        Layout.preferredHeight: 48
                        Layout.alignment: Qt.AlignTop
                        source: root.iconName()
                        smooth: true
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: LingmoUI.Units.smallSpacing

                        Label {
                            text: root.titleText()
                            font.pointSize: LingmoUI.Theme.fontSize * 1.35
                            font.bold: true
                            color: LingmoUI.Theme.textColor
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Label {
                            text: root.subtitleText()
                            visible: text.length > 0
                            wrapMode: Text.Wrap
                            maximumLineCount: 4
                            color: updater.errorMessage ? LingmoUI.Theme.redColor : LingmoUI.Theme.disabledTextColor
                            Layout.fillWidth: true
                        }
                    }
                }

                ProgressBar {
                    Layout.fillWidth: true
                    visible: root.busy
                    indeterminate: updater.progress < 0
                    value: updater.progress < 0 ? 0 : updater.progress
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: st !== UpdateManager.Updating
                    spacing: LingmoUI.Units.largeSpacing

                    Item { Layout.fillWidth: true }

                    ActionButton {
                        text: qsTr("Check Again")
                        enabled: !root.busy
                        onClicked: updater.check()
                    }

                    ActionButton {
                        primary: true
                        text: st === UpdateManager.UpdateFailed ? qsTr("Try Again") : qsTr("Update Now")
                        visible: updater.count > 0
                                 && (st === UpdateManager.Available || st === UpdateManager.UpdateFailed)
                        enabled: !root.busy
                        onClicked: updater.update()
                    }

                    ActionButton {
                        primary: true
                        text: qsTr("Restart Now")
                        visible: st === UpdateManager.Updated && updater.rebootRecommended
                        onClicked: updater.reboot()
                    }
                }
            }
        }

        // Packages to update
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 120
            visible: root.showList || !root.detailsOpen
            radius: LingmoUI.Theme.mediumRadius
            color: LingmoUI.Theme.secondBackgroundColor

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: LingmoUI.Units.largeSpacing
                spacing: LingmoUI.Units.smallSpacing
                visible: root.showList

                Label {
                    text: st === UpdateManager.Updated ? qsTr("Updated packages") : qsTr("Packages")
                    color: LingmoUI.Theme.disabledTextColor
                    font.bold: true
                    Layout.leftMargin: LingmoUI.Units.largeSpacing
                }

                PackageList {
                    model: root.showList ? updater.packages : []
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }

            // Nothing to list
            ColumnLayout {
                anchors.centerIn: parent
                visible: !root.showList
                spacing: LingmoUI.Units.largeSpacing

                LingmoUI.IconItem {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 64
                    Layout.preferredHeight: 64
                    source: st === UpdateManager.UpToDate ? "emblem-default" : "system-software-update"
                    opacity: 0.35
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    color: LingmoUI.Theme.disabledTextColor
                    text: st === UpdateManager.UpToDate ? qsTr("All software is up to date.")
                        : st === UpdateManager.CheckFailed ? qsTr("Try again in a moment.")
                        : qsTr("Looking for new versions…")
                }
            }
        }

        // Details: pacman's output
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: root.detailsOpen
            Layout.preferredHeight: root.detailsOpen ? 220 : implicitHeight
            visible: updater.log.length > 0
            spacing: LingmoUI.Units.smallSpacing

            ItemDelegate {
                Layout.fillWidth: true
                implicitHeight: 32
                onClicked: root.detailsOpen = !root.detailsOpen

                background: Rectangle {
                    radius: LingmoUI.Theme.smallRadius
                    color: parent.hovered ? LingmoUI.Theme.secondBackgroundColor : "transparent"
                }

                contentItem: RowLayout {
                    spacing: LingmoUI.Units.smallSpacing

                    Label {
                        text: "›"
                        font.pointSize: LingmoUI.Theme.fontSize * 1.3
                        color: LingmoUI.Theme.textColor
                        rotation: root.detailsOpen ? 90 : 0
                        Layout.preferredWidth: 14
                        horizontalAlignment: Text.AlignHCenter
                        Behavior on rotation { NumberAnimation { duration: 150 } }
                    }

                    Label {
                        text: qsTr("Details")
                        color: LingmoUI.Theme.textColor
                        Layout.fillWidth: true
                    }
                }
            }

            LogView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.detailsOpen
                text: updater.log
            }
        }
    }
}
