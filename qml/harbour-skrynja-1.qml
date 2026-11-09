import QtQuick 2.0
import Sailfish.Silica 1.0
import "pages"

ApplicationWindow {
    initialPage: Component { StartSide { } }
    allowedOrientations: defaultAllowedOrientations

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

            Button {
                text: "Luk"
                onClicked: pageStack.pop()
            }
        }
    }
}
