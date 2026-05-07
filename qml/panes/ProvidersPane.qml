import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import CodexBar 1.0
import ".."
import "../components"

Rectangle {
    id: root
    color: AppTheme.bgPrimary

    property var providers: []
    property int providerCount: 0
    property string selectedProvider: ""
    property var selectedDescriptor: null
    property string detailState: "idle"
    property var selectedConnectionTest: ({"state": "idle"})
    property var selectedProviderStatus: ({"state": "unknown"})
    property var selectedProviderError: ""
    property var selectedUsageSnapshot: null
    property var tokenAccounts: []
    property string defaultTokenAccountId: ""
    property var tokenAccountOperationState: ({})
    property var codexAccountState: ({})
    property var codexProjection: ({})

    signal providerSelected(string providerId)
    signal providerEnabled(string providerId, bool enabled)
    signal testConnection(string providerId)
    signal refreshProvider(string providerId)
    signal settingChanged(string providerId, string key, var value)
    signal secretSaveRequested(string providerId, string key, string value)
    signal secretClearRequested(string providerId, string key)
    signal moveProvider(int fromIndex, int toIndex)
    signal addTokenAccount(string providerId, string displayName, int sourceMode, string apiKey)
    signal removeTokenAccount(string accountId)
    signal setDefaultTokenAccount(string providerId, string accountId)
    signal setTokenAccountSourceMode(string accountId, int sourceMode)
    signal setTokenAccountVisibility(string accountId, int visibility)
    signal setCodexActiveAccount(string accountId)
    signal addCodexAccount()
    signal cancelCodexAuthentication()
    signal removeCodexAccount(string accountId)
    signal reauthenticateCodexAccount(string accountId)
    signal promoteCodexAccount(string accountId)

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 268
            Layout.fillHeight: true
            color: AppTheme.bgPrimary

            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: AppTheme.borderColor
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 12

                Label {
                    text: qsTr("Providers")
                    color: AppTheme.textPrimary
                    font.pixelSize: 22
                    font.bold: true
                }

                Label {
                    text: qsTr("Enable, order, and test each data source.")
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
                    id: providerList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.providers
                    spacing: 4

                    delegate: ProviderListItem {
                        width: providerList.width
                        providerName: model.name
                        providerId: model.providerId
                        brandColor: model.brandColor || AppTheme.accentColor
                        isEnabled: model.enabled
                        isSelected: root.selectedProvider === model.providerId
                        usageData: model.usage
                        status: model.status || "unknown"
                        lastUpdated: model.lastUpdated || ""
                        itemIndex: index

                        onClicked: root.providerSelected(model.providerId)
                        onToggleChanged: function(checked) {
                            root.providerEnabled(model.providerId, checked)
                        }
                        onDragFinished: function(fromIndex, toIndex) {
                            if (fromIndex !== toIndex && fromIndex >= 0 && toIndex >= 0) {
                                root.moveProvider(fromIndex, toIndex)
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: AppTheme.bgPrimary

            ProviderDetailView {
                anchors.fill: parent
                visible: root.selectedProvider !== ""
                providerId: root.selectedProvider
                descriptor: root.selectedDescriptor
                detailState: root.detailState
                connectionTest: root.selectedConnectionTest
                providerStatus: root.selectedProviderStatus
                providerError: root.selectedProviderError
                usageSnapshot: root.selectedUsageSnapshot
                tokenAccounts: root.tokenAccounts
                defaultTokenAccountId: root.defaultTokenAccountId
                tokenAccountOperationState: root.tokenAccountOperationState
                codexAccountState: root.codexAccountState
                codexProjection: root.codexProjection

                onTestConnectionRequested: root.testConnection(root.selectedProvider)
                onRefreshRequested: root.refreshProvider(root.selectedProvider)
                onToggleEnabled: function(enabled) {
                    root.providerEnabled(root.selectedProvider, enabled)
                }
                onSettingChanged: function(key, value) {
                    root.settingChanged(root.selectedProvider, key, value)
                }
                onSecretSaveRequested: function(key, value) {
                    root.secretSaveRequested(root.selectedProvider, key, value)
                }
                onSecretClearRequested: function(key) {
                    root.secretClearRequested(root.selectedProvider, key)
                }
                onAddTokenAccountRequested: function(displayName, sourceMode, apiKey) {
                    root.addTokenAccount(root.selectedProvider, displayName, sourceMode, apiKey)
                }
                onRemoveTokenAccountRequested: function(accountId) {
                    root.removeTokenAccount(accountId)
                }
                onSetDefaultTokenAccountRequested: function(accountId) {
                    root.setDefaultTokenAccount(root.selectedProvider, accountId)
                }
                onSetTokenAccountSourceModeRequested: function(accountId, sourceMode) {
                    root.setTokenAccountSourceMode(accountId, sourceMode)
                }
                onSetTokenAccountVisibilityRequested: function(accountId, visibility) {
                    root.setTokenAccountVisibility(accountId, visibility)
                }
                onSetCodexActiveAccountRequested: function(accountId) {
                    root.setCodexActiveAccount(accountId)
                }
                onAddCodexAccountRequested: root.addCodexAccount()
                onCancelCodexAuthenticationRequested: root.cancelCodexAuthentication()
                onRemoveCodexAccountRequested: function(accountId) {
                    root.removeCodexAccount(accountId)
                }
                onReauthenticateCodexAccountRequested: function(accountId) {
                    root.reauthenticateCodexAccount(accountId)
                }
                onPromoteCodexAccountRequested: function(accountId) {
                    root.promoteCodexAccount(accountId)
                }
            }

            ColumnLayout {
                anchors.centerIn: parent
                width: 280
                spacing: 8
                visible: root.selectedProvider === ""

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Select a provider")
                    color: AppTheme.textPrimary
                    font.pixelSize: AppTheme.fontSizeLg
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Choose a provider from the list to edit connection settings.")
                    color: AppTheme.textSecondary
                    font.pixelSize: AppTheme.fontSizeSm
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }
}
