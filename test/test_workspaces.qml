import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niri 0.1

ApplicationWindow {
    visible: true
    width: 600
    height: 400
    title: "Niri Workspaces Test"

    Niri {
        id: niri
        Component.onCompleted: connect()

        onConnected: {
            console.log("✓ Connected to niri")
            statusText.text = "Connected"
            statusText.color = "green"
        }

        onDisconnected: {
            console.log("✗ Disconnected from niri")
            statusText.text = "Disconnected"
            statusText.color = "red"
        }

        onErrorOccurred: function(error) {
            console.log("✗ Error:", error)
            statusText.text = "Error: " + error
            statusText.color = "red"
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10

        // Status header
        RowLayout {
            Layout.fillWidth: true

            Text {
                id: statusText
                text: "Connecting..."
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            Text {
                text: "Total workspaces: " + niri.workspaces.count
                font.pixelSize: 12
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#CCC"
        }

        Text {
            text: "Click on a workspace to switch to it"
            font.pixelSize: 10
            color: "#666"
            font.italic: true
        }

        // All workspaces list
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            model: niri.workspaces
            spacing: 5
            clip: true

            delegate: Rectangle {
                width: ListView.view.width
                height: 70
                color: model.isFocused ? "#4CAF50" :
                       model.isActive ? "#8BC34A" : "#F5F5F5"
                border.color: model.isUrgent ? "red" : "#CCC"
                border.width: model.isUrgent ? 3 : 1
                radius: 5

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true

                    onEntered: {
                        parent.opacity = 0.8
                    }

                    onExited: {
                        parent.opacity = 1.0
                    }

                    onClicked: {
                        console.log("Switching to workspace", model.index, "(ID:", model.id + ")")
                        niri.focusWorkspaceById(model.id)
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 2

                    RowLayout {
                        spacing: 5

                        Text {
                            text: "Workspace " + model.index
                            font.bold: model.isFocused
                            font.pixelSize: 14
                            color: model.isFocused ? "white" : "black"
                        }

                        Text {
                            text: model.name || "(unnamed)"
                            font.italic: !model.name
                            color: model.isFocused ? "white" : "#666"
                        }

                        Text {
                            text: "● FOCUSED"
                            font.bold: true
                            color: "white"
                            visible: model.isFocused
                        }

                        Text {
                            text: "○ ACTIVE"
                            font.bold: true
                            color: "#333"
                            visible: model.isActive && !model.isFocused
                        }

                        Text {
                            text: "⚠ URGENT"
                            color: "darkred"
                            font.bold: true
                            visible: model.isUrgent
                        }
                    }

                    RowLayout {
                        Text {
                            text: "Output: " + (model.output || "none")
                            font.pixelSize: 10
                            color: model.isFocused ? "white" : "#666"
                        }
                        Text {
                            text: "Active window ID: " +
                                  (model.activeWindowId ? model.activeWindowId : "none")
                            font.pixelSize: 10
                            color: model.isFocused ? "white" : "#666"
                        }
                    }

                    Text {
                        text: `ID: ${model.id} | Index: ${model.index}`
                        font.pixelSize: 10
                        color: model.isFocused ? "white" : "#888"
                    }
                }
            }
        }
    }
}
