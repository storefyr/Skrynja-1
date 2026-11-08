import QtQuick 2.0
import Sailfish.Silica 1.0
import "pages"

ApplicationWindow {
    id: appWindow

    property int uiToolCalls: 0

    initialPage: Component { StartSide { } }
    allowedOrientations: defaultAllowedOrientations
}
