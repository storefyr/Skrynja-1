import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: startSide

    Connections {
        target: acp
        onToolCallsChanged: acp.uiToolCallsSeen(acp.toolCalls.length)
    }

    function sendPrompt() {
        if (promptField.text.length > 0 && acp.sessionId.length > 0) {
            acp.sendPrompt(promptField.text)
            promptField.text = ""
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: "Skrynja-1"
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                color: Theme.secondaryColor
                text: acp.sessionId.length > 0 ? "Session: " + acp.sessionId : "Starter agent…"
            }

            TextField {
                id: promptField
                width: parent.width
                label: "Prompt"
                placeholderText: "Skriv en prompt…"
                EnterKey.enabled: text.length > 0
                EnterKey.onClicked: startSide.sendPrompt()
            }

            Button {
                id: sendButton
                text: "Send"
                enabled: promptField.text.length > 0 && acp.sessionId.length > 0
                onClicked: startSide.sendPrompt()
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                text: acp.lastText
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                color: Theme.secondaryColor
                visible: acp.toolCalls.length > 0
                text: "Værktøjskald:"
            }

            Repeater {
                id: toolRepeater
                model: acp.toolCalls

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    wrapMode: Text.WordWrap
                    color: Theme.secondaryColor
                    text: modelData.kind + ": " + modelData.title +
                          (modelData.detalje.length > 0 ? " — " + modelData.detalje : "")
                }
            }
        }
    }
}
