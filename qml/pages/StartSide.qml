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

    Dialog {
        id: permDialog
        canAccept: false

        Column {
            width: parent.width
            spacing: Theme.paddingMedium

            DialogHeader {
                title: "Tilladelse anmodet"
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                text: (acp.pendingPermission.kind || "") + ": " + (acp.pendingPermission.title || "")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                color: Theme.secondaryColor
                text: acp.pendingPermission.kommando || ""
            }

            Repeater {
                model: acp.pendingPermission.options || []

                Button {
                    text: modelData.name
                    onClicked: {
                        acp.answerPermission(modelData.optionId)
                        permDialog.reject()
                    }
                }
            }

            Button {
                text: "Afvis"
                onClicked: {
                    acp.dismissPermission()
                    permDialog.reject()
                }
            }

            Button {
                text: "Stop"
                onClicked: {
                    acp.stopAgent()
                    permDialog.reject()
                }
            }
        }
    }

    Connections {
        target: acp
        onPendingPermissionChanged: {
            if (acp.hasPendingPermission)
                pageStack.push(permDialog)
        }
    }

    PullDownMenu {
        MenuItem {
            text: "Indstillinger"
            onClicked: pageStack.push(configDialog)
        }
    }

    Dialog {
        id: configDialog
        canAccept: false

        Column {
            width: parent.width
            spacing: Theme.paddingMedium

            DialogHeader {
                title: "Indstillinger"
            }

            Repeater {
                model: acp.configOptions

                Column {
                    id: cfgItem
                    width: parent.width
                    property string cfgId: modelData.id

                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        wrapMode: Text.WordWrap
                        text: modelData.name + " — nuværende: " + modelData.currentValue
                    }

                    Row {
                        x: Theme.horizontalPageMargin
                        spacing: Theme.paddingMedium

                        Repeater {
                            model: modelData.options || []

                            Button {
                                text: modelData.name
                                onClicked: acp.sendSetConfigOption(cfgItem.cfgId, modelData.value)
                            }
                        }
                    }
                }
            }
        }
    }
}
