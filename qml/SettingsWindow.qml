import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import CodexBarX 1.0
import "panes"

Rectangle {
    id: settingsWindow
    width: 900
    color: AppTheme.bgPrimary

    property int rev: LanguageManager.translationRevision
    property bool providersPaneLoaded: false
    property var tabs: {
        settingsWindow.rev
        var items = [
            { label: qsTr("General"), icon: "G" },
            { label: qsTr("Providers"), icon: "P" },
            { label: qsTr("Display"), icon: "D" },
            { label: qsTr("Advanced"), icon: "A" },
            { label: qsTr("About"), icon: "i" }
        ]
        if (SettingsStore.debugMenuEnabled) items.push({ label: qsTr("Debug"), icon: "{}" })
        return items
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: 1
        border.color: AppTheme.borderColor
        z: 20
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: titleBar
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: "#10101f"

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: AppTheme.borderColor
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 8
                spacing: 10

                Rectangle {
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    radius: 5
                    color: AppTheme.accentColor

                    Label {
                        anchors.centerIn: parent
                        text: "C"
                        color: "white"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                Label {
                    text: qsTr("CodexBar Settings")
                    color: AppTheme.textPrimary
                    font.pixelSize: AppTheme.fontSizeMd
                    font.bold: true
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton
                        onPressed: AppController.startSettingsMove()
                        onDoubleClicked: AppController.toggleSettingsMaximized()
                    }
                }

                TitleButton {
                    text: "_"
                    onClicked: AppController.minimizeSettings()
                }

                TitleButton {
                    text: AppController.settingsMaximized ? "[]" : "[ ]"
                    onClicked: AppController.toggleSettingsMaximized()
                }

                TitleButton {
                    text: "x"
                    danger: true
                    onClicked: AppController.closeSettings()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.preferredWidth: 220
                Layout.fillHeight: true
                color: AppTheme.bgSecondary

                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 1
                    color: AppTheme.borderColor
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    Label {
                        text: qsTr("Settings")
                        color: AppTheme.textPrimary
                        font.pixelSize: AppTheme.fontSizeXl
                        font.bold: true
                    }

                    Label {
                        text: qsTr("Configure providers and app behavior")
                        color: AppTheme.textSecondary
                        font.pixelSize: AppTheme.fontSizeSm
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: AppTheme.borderColor
                    }

                    ListView {
                        id: tabList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: 0
                        spacing: 4
                        clip: true
                        model: settingsWindow.tabs
                        onCurrentIndexChanged: {
                            if (currentIndex === 1) settingsWindow.providersPaneLoaded = true
                        }

                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 38
                            radius: 6
                            color: tabList.currentIndex === index
                                ? AppTheme.bgSelected
                                : (tabMouse.containsMouse ? AppTheme.bgHover : "transparent")

                            Rectangle {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                width: 3
                                height: 22
                                radius: 2
                                color: AppTheme.accentColor
                                visible: tabList.currentIndex === index
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 10
                                spacing: 10

                                Rectangle {
                                    Layout.preferredWidth: 22
                                    Layout.preferredHeight: 22
                                    radius: 5
                                    color: tabList.currentIndex === index
                                        ? AppTheme.accentColor
                                        : AppTheme.bgCard

                                    Label {
                                        anchors.centerIn: parent
                                        text: modelData.icon
                                        color: tabList.currentIndex === index ? "white" : AppTheme.textSecondary
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.label
                                    color: tabList.currentIndex === index ? AppTheme.textPrimary : AppTheme.textSecondary
                                    font.pixelSize: AppTheme.fontSizeMd
                                    elide: Text.ElideRight
                                }
                            }

                            MouseArea {
                                id: tabMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: tabList.currentIndex = index
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: AppTheme.bgPrimary

                StackLayout {
                    id: contentStack
                    anchors.fill: parent
                    currentIndex: tabList.currentIndex

                    GeneralPane {}

                    Loader {
                        id: providersPaneLoader
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        active: settingsWindow.providersPaneLoaded
                        asynchronous: true

                        sourceComponent: ProvidersPane {
                            id: providersPane
                            providers: SettingsProvidersModel.providers
                            providerCount: SettingsProvidersModel.providerCount
                            selectedProvider: SettingsProvidersModel.selectedProvider
                            selectedDescriptor: SettingsProvidersModel.selectedDescriptor
                            detailState: SettingsProvidersModel.detailState
                            selectedConnectionTest: SettingsProvidersModel.selectedConnectionTest
                            selectedProviderStatus: SettingsProvidersModel.selectedProviderStatus
                            selectedProviderError: SettingsProvidersModel.selectedProviderError
                            selectedUsageSnapshot: SettingsProvidersModel.selectedUsageSnapshot
                            tokenAccounts: SettingsProvidersModel.selectedTokenAccounts
                            defaultTokenAccountId: SettingsProvidersModel.selectedDefaultTokenAccountId
                            tokenAccountOperationState: SettingsProvidersModel.tokenAccountOperationState
                            codexAccountState: SettingsProvidersModel.codexAccountState
                            codexProjection: SettingsProvidersModel.codexProjection

                            Component.onCompleted: SettingsProvidersModel.requestOpenProvidersTab()

                            onProviderSelected: function(providerId) {
                                SettingsProvidersModel.selectProvider(providerId)
                            }
                            onProviderEnabled: function(providerId, enabled) {
                                SettingsProvidersModel.setProviderEnabled(providerId, enabled)
                            }
                            onTestConnection: function(providerId) {
                                SettingsProvidersModel.testConnection(providerId)
                            }
                            onRefreshProvider: function(providerId) {
                                SettingsProvidersModel.refreshProvider(providerId)
                            }
                            onSettingChanged: function(providerId, key, value) {
                                SettingsProvidersModel.setProviderSetting(providerId, key, value)
                            }
                            onSecretSaveRequested: function(providerId, key, value) {
                                SettingsProvidersModel.setProviderSecret(providerId, key, value)
                            }
                            onSecretClearRequested: function(providerId, key) {
                                SettingsProvidersModel.clearProviderSecret(providerId, key)
                            }
                            onMoveProvider: function(fromIndex, toIndex) {
                                SettingsProvidersModel.moveProvider(fromIndex, toIndex)
                            }
                            onAddTokenAccount: function(providerId, displayName, sourceMode, apiKey) {
                                if (apiKey && apiKey.trim() !== "") {
                                    SettingsProvidersModel.requestAddTokenAccountWithApiKey(providerId, displayName, sourceMode, apiKey)
                                } else {
                                    SettingsProvidersModel.requestAddTokenAccount(providerId, displayName, sourceMode)
                                }
                            }
                            onRemoveTokenAccount: function(accountId) {
                                SettingsProvidersModel.requestRemoveTokenAccount(accountId)
                            }
                            onSetDefaultTokenAccount: function(providerId, accountId) {
                                SettingsProvidersModel.requestSetDefaultTokenAccount(providerId, accountId)
                            }
                            onSetTokenAccountSourceMode: function(accountId, sourceMode) {
                                SettingsProvidersModel.requestSetTokenAccountSourceMode(accountId, sourceMode)
                            }
                            onSetTokenAccountVisibility: function(accountId, visibility) {
                                SettingsProvidersModel.requestSetTokenAccountVisibility(accountId, visibility)
                            }
                            onSetCodexActiveAccount: function(accountId) {
                                SettingsProvidersModel.setCodexActiveAccount(accountId)
                            }
                            onAddCodexAccount: function() {
                                SettingsProvidersModel.addCodexAccount()
                            }
                            onCancelCodexAuthentication: function() {
                                SettingsProvidersModel.cancelCodexAuthentication()
                            }
                            onRemoveCodexAccount: function(accountId) {
                                SettingsProvidersModel.removeCodexAccount(accountId)
                            }
                            onReauthenticateCodexAccount: function(accountId) {
                                SettingsProvidersModel.reauthenticateCodexAccount(accountId)
                            }
                            onPromoteCodexAccount: function(accountId) {
                                SettingsProvidersModel.promoteCodexAccount(accountId)
                            }
                        }
                    }

                    DisplayPane {}
                    AdvancedPane {}
                    AboutPane {}
                    DebugPane {}
                }
            }
        }
    }

    ResizeHandle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; edge: Qt.LeftEdge }
    ResizeHandle { anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; edge: Qt.RightEdge }
    ResizeHandle { anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right; edge: Qt.TopEdge }
    ResizeHandle { anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right; edge: Qt.BottomEdge }
    ResizeHandle { anchors.left: parent.left; anchors.top: parent.top; width: 10; height: 10; edge: Qt.LeftEdge | Qt.TopEdge }
    ResizeHandle { anchors.right: parent.right; anchors.top: parent.top; width: 10; height: 10; edge: Qt.RightEdge | Qt.TopEdge }
    ResizeHandle { anchors.left: parent.left; anchors.bottom: parent.bottom; width: 10; height: 10; edge: Qt.LeftEdge | Qt.BottomEdge }
    ResizeHandle { anchors.right: parent.right; anchors.bottom: parent.bottom; width: 10; height: 10; edge: Qt.RightEdge | Qt.BottomEdge }

    component TitleButton: Rectangle {
        id: titleButton
        property alias text: label.text
        property bool danger: false
        signal clicked()

        Layout.preferredWidth: 36
        Layout.preferredHeight: 30
        radius: 5
        color: mouseArea.containsMouse
            ? (danger ? AppTheme.statusOutage : AppTheme.bgHover)
            : "transparent"

        Label {
            id: label
            anchors.centerIn: parent
            color: "white"
            font.pixelSize: 13
            font.bold: true
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: titleButton.clicked()
        }
    }

    component ResizeHandle: MouseArea {
        property int edge: Qt.LeftEdge
        width: 6
        height: 6
        hoverEnabled: true
        z: 30
        onPressed: AppController.startSettingsResize(edge)
    }
}
