pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import "ColorLogo.js" as ColorLogo

Item
{
    id: r
    required property string appPackage
    property bool loadTVBanner: true
    property bool async: false
    property real cardRadius: 6 // in the 1080p design this is about 12 px

    // everything drawn in the card goes through a rounded mask
    Item
    {
        id: content
        anchors.fill: parent
        layer.enabled: r.cardRadius > 0
        layer.effect: MultiEffect { maskEnabled: true; maskSource: mask; maskThresholdMin: 0.5; maskSpreadAtMin: 1.0 }

    // background color based on app's dominant color, used when banner is not available or not wanted
    Rectangle
    {
        id: cover
        anchors.fill: parent
        visible: r.loadTVBanner && icon.status === Image.Ready
        color: visible ? ColorLogo.createByName(r.appPackage) : ""
    }

    Image
    {
        // 16:9 tv banner image | not available for all apps
        id: banner
        source: r.loadTVBanner ? "image://banner/" + r.appPackage : ""
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        cache: true
        asynchronous: r.async
    }

    Image
    {
        // app icon, loaded if banner image is not wanted or not available
        id: icon
        source: (banner.status === Image.Error || !r.loadTVBanner) ? "image://icon/" + r.appPackage : ""
        anchors.fill: parent
        anchors.margins: r.loadTVBanner ? 15 : 0
        fillMode: Image.PreserveAspectFit
        asynchronous: r.async
        cache: true
    }
    }

    Rectangle
    {
        id: mask
        anchors.fill: parent
        radius: r.cardRadius
        color: Qt.color("#ffffff")
        layer.enabled: true
        visible: false
    }
}
