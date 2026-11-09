import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: startSide

    Connections {
        target: acp
        onToolCallsChanged: acp.uiToolCallsSeen(acp.toolCalls.length)
    }

    property string permTekst: {
        var k = acp.pendingPermission.kind || ""
        var t = acp.pendingPermission.title || ""
        var c = acp.pendingPermission.kommando || ""
        var r = acp.pendingPermission.rå || ""
        var o = (k ? k + ": " : "") + t
        if (c)
            o += " — " + c
        if (!t && !c && r)
            o = r
        return o
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
                color: Theme.errorColor
                visible: acp.lastError.length > 0
                text: "Fejl: " + acp.lastError
            }

            Column {
                width: parent.width
                spacing: Theme.paddingMedium
                visible: acp.hasPendingPermission && !selvtest

                SectionHeader {
                    text: "Tilladelse anmodet"
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    wrapMode: Text.WordWrap
                    text: startSide.permTekst
                }

                Repeater {
                    model: acp.pendingPermission.options || []

                    Button {
                        text: modelData.name
                        onClicked: acp.answerPermission(modelData.optionId)
                    }
                }

                Button {
                    text: "Afvis"
                    onClicked: acp.dismissPermission()
                }

                Button {
                    text: "Stop"
                    onClicked: acp.stopAgent()
                }
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
                text: acp.lastText.length > 4000
                      ? "…" + acp.lastText.substring(acp.lastText.length - 4000)
                      : acp.lastText
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
                          (modelData.detalje.length > 0 ? " — " + modelData.detalje : "") +
                          " [" + (modelData.status || "") + "]"
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                color: Theme.secondaryColor
                visible: acp.diffs.length > 0
                text: "Ændringer:"
            }

            Repeater {
                model: acp.diffs

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    wrapMode: Text.Wrap
                    font.family: "monospace"
                    font.pixelSize: Theme.fontSizeSmall
                    text: modelData.sti + "\n" + modelData.diff
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                color: Theme.secondaryColor
                visible: acp.outputs.length > 0
                text: "Udoutput:"
            }

            Repeater {
                model: acp.outputs

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    wrapMode: Text.Wrap
                    font.family: "monospace"
                    font.pixelSize: Theme.fontSizeSmall
                    text: modelData.tekst
                }
            }
        }
    }


}
