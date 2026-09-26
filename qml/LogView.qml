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
import LingmoUI.CompatibleModule 3.0 as LingmoUI

// pacman's output, following the end while it streams in
Rectangle {
    id: control

    property alias text: textEdit.text

    radius: LingmoUI.Theme.smallRadius
    color: LingmoUI.Theme.darkMode ? "#161618" : "#F6F7FA"
    border.width: 1
    border.color: LingmoUI.Theme.darkMode ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.06)

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.margins: LingmoUI.Units.smallSpacing * 1.5
        clip: true
        contentWidth: width
        contentHeight: textEdit.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        property bool atEnd: contentY >= contentHeight - height - 4

        TextEdit {
            id: textEdit
            width: flick.width - LingmoUI.Units.largeSpacing
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.WrapAnywhere
            font.family: fixedFontFamily
            font.pointSize: LingmoUI.Theme.fontSize * 0.9
            color: LingmoUI.Theme.textColor
            selectionColor: LingmoUI.Theme.highlightColor
            selectedTextColor: LingmoUI.Theme.highlightedTextColor

        }

        property bool follow: true
        onContentHeightChanged: if (follow) toEnd()
        onMovementEnded: follow = atEnd
        function toEnd() {
            contentY = Math.max(0, contentHeight - height)
        }
    }
}
