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

// Pending packages: name on the left, "current → new" on the right
ListView {
    id: list

    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}

    delegate: Item {
        width: ListView.view.width
        height: 38

        Rectangle {
            anchors.fill: parent
            anchors.rightMargin: LingmoUI.Units.smallSpacing
            radius: LingmoUI.Theme.smallRadius
            color: index % 2 ? "transparent" : LingmoUI.Theme.alternateBackgroundColor
            opacity: 0.6
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: LingmoUI.Units.largeSpacing
            anchors.rightMargin: LingmoUI.Units.largeSpacing * 1.5
            spacing: LingmoUI.Units.largeSpacing

            Label {
                text: modelData.name
                color: LingmoUI.Theme.textColor
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.minimumWidth: 80
            }

            Label {
                text: modelData.oldVersion
                color: LingmoUI.Theme.disabledTextColor
                elide: Text.ElideMiddle
                horizontalAlignment: Text.AlignRight
                Layout.maximumWidth: list.width * 0.28
            }

            Label {
                text: "→"
                color: LingmoUI.Theme.disabledTextColor
            }

            Label {
                text: modelData.newVersion
                color: LingmoUI.Theme.highlightColor
                font.bold: true
                elide: Text.ElideMiddle
                Layout.maximumWidth: list.width * 0.28
            }
        }
    }
}
