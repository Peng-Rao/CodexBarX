var installerPageCopies = [
    {
        id: QInstaller.Introduction,
        objectName: "IntroductionPage",
        title: "Welcome to CodexBarX",
        subtitle: "Install the focused desktop command bar with a dark interface that matches the app."
    },
    {
        id: QInstaller.TargetDirectory,
        objectName: "TargetDirectoryPage",
        title: "Choose Install Location",
        subtitle: "Pick where CodexBarX and its support files should be installed."
    },
    {
        id: QInstaller.ComponentSelection,
        objectName: "ComponentSelectionPage",
        title: "Select Components",
        subtitle: "Install the CodexBarX desktop app and required runtime files."
    },
    {
        id: QInstaller.LicenseCheck,
        objectName: "LicenseAgreementPage",
        title: "Review License",
        subtitle: "Accept the license terms to continue with installation."
    },
    {
        id: QInstaller.StartMenuSelection,
        objectName: "StartMenuDirectoryPage",
        title: "Start Menu Folder",
        subtitle: "Choose where CodexBarX shortcuts should be created."
    },
    {
        id: QInstaller.ReadyForInstallation,
        objectName: "ReadyForInstallationPage",
        title: "Ready to Install",
        subtitle: "Review your selections before CodexBarX is installed."
    },
    {
        id: QInstaller.PerformInstallation,
        objectName: "PerformInstallationPage",
        title: "Installing CodexBarX",
        subtitle: "Files are being copied and shortcuts are being prepared."
    },
    {
        id: QInstaller.InstallationFinished,
        objectName: "FinishedPage",
        title: "CodexBarX Is Ready",
        subtitle: "Launch CodexBarX after installation or close this installer."
    }
];

function Controller() {
    applyWizardButtonText();
}

function applyWizardButtonText() {
    for (var i = 0; i < installerPageCopies.length; i++) {
        var pageId = installerPageCopies[i].id;
        setWizardButtonText(pageId, buttons.BackButton, "Back");
        setWizardButtonText(pageId, buttons.NextButton, "Next");
        setWizardButtonText(pageId, buttons.CommitButton, "Install");
        setWizardButtonText(pageId, buttons.FinishButton, "Finish");
        setWizardButtonText(pageId, buttons.CancelButton, "Cancel");
    }
}

function setWizardButtonText(pageId, buttonId, text) {
    try {
        gui.setWizardPageButtonText(pageId, buttonId, text);
    } catch (e) {
        print("Unable to set wizard button text: " + e);
    }
}

function applyPageCopy(objectName, title, subtitle) {
    var page = null;
    try {
        page = gui.pageByObjectName(objectName);
    } catch (e) {
        print("Unable to resolve installer page '" + objectName + "': " + e);
    }

    if (!page) {
        return;
    }

    try {
        page.title = title;
    } catch (titlePropertyError) {
        try {
            page.setTitle(title);
        } catch (titleMethodError) {
            print("Unable to set title for '" + objectName + "': " + titleMethodError);
        }
    }

    try {
        page.subTitle = subtitle;
    } catch (subtitlePropertyError) {
        try {
            page.setSubTitle(subtitle);
        } catch (subtitleMethodError) {
            print("Unable to set subtitle for '" + objectName + "': " + subtitleMethodError);
        }
    }
}

function applyPageCopyByName(objectName) {
    for (var i = 0; i < installerPageCopies.length; i++) {
        var pageCopy = installerPageCopies[i];
        if (pageCopy.objectName === objectName) {
            applyPageCopy(pageCopy.objectName, pageCopy.title, pageCopy.subtitle);
            return;
        }
    }
}

function setPageLabelText(objectName, labelName, text) {
    var widget = null;
    try {
        widget = gui.pageWidgetByObjectName(objectName);
    } catch (e) {
        print("Unable to resolve installer page widget '" + objectName + "': " + e);
    }

    if (!widget || !widget[labelName]) {
        return;
    }

    try {
        widget[labelName].setText(text);
    } catch (textError) {
        print("Unable to set " + objectName + "." + labelName + ": " + textError);
    }

    try {
        widget[labelName].setWordWrap(true);
    } catch (wrapError) {
        // Some IFW labels do not expose word wrapping through scripting.
    }
}

Controller.prototype.IntroductionPageCallback = function() {
    applyPageCopyByName("IntroductionPage");
    setPageLabelText(
        "IntroductionPage",
        "MessageLabel",
        "<h2 style='color:#ffffff; margin:0;'>Install CodexBarX</h2>" +
        "<p style='color:#d7d7e6;'>Set up the focused desktop command bar with the same dark interface as the app.</p>" +
        "<p style='color:#ffffff;'><b>Fast setup</b><br/><span style='color:#aaaaaa;'>Copy the app, runtime files, and shortcuts in one guided flow.</span></p>" +
        "<p style='color:#ffffff;'><b>Dark by default</b><br/><span style='color:#aaaaaa;'>Installer controls, license text, and progress stay readable on the dark theme.</span></p>"
    );
};

Controller.prototype.TargetDirectoryPageCallback = function() {
    applyPageCopyByName("TargetDirectoryPage");
    setPageLabelText(
        "TargetDirectoryPage",
        "MessageLabel",
        "<h2 style='color:#ffffff; margin:0;'>Destination Folder</h2>" +
        "<p style='color:#d7d7e6;'>Choose the folder where CodexBarX should be installed.</p>" +
        "<p style='color:#ffffff;'><b>Recommended</b><br/><span style='color:#aaaaaa;'>Keep the default path unless you manage apps in a custom location.</span></p>"
    );
};

Controller.prototype.ComponentSelectionPageCallback = function() {
    applyPageCopyByName("ComponentSelectionPage");
    setPageLabelText(
        "ComponentSelectionPage",
        "MessageLabel",
        "<h2 style='color:#ffffff; margin:0;'>Install Components</h2>" +
        "<p style='color:#d7d7e6;'>CodexBarX installs the desktop app and the runtime files it needs to run reliably.</p>"
    );
};

Controller.prototype.LicenseAgreementPageCallback = function() {
    applyPageCopyByName("LicenseAgreementPage");
    setPageLabelText(
        "LicenseAgreementPage",
        "LicenseInfoLabel",
        "Review the license terms before continuing."
    );
    setPageLabelText(
        "LicenseAgreementPage",
        "AcceptLicenseLabel",
        "I accept the license terms for CodexBarX."
    );
};

Controller.prototype.StartMenuDirectoryPageCallback = function() {
    applyPageCopyByName("StartMenuDirectoryPage");
    setPageLabelText(
        "StartMenuDirectoryPage",
        "MessageLabel",
        "<h2 style='color:#ffffff; margin:0;'>Shortcut Folder</h2>" +
        "<p style='color:#d7d7e6;'>Choose where CodexBarX shortcuts should appear in the Start Menu.</p>"
    );
};

Controller.prototype.ReadyForInstallationPageCallback = function() {
    applyPageCopyByName("ReadyForInstallationPage");
    setPageLabelText(
        "ReadyForInstallationPage",
        "MessageLabel",
        "<h2 style='color:#ffffff; margin:0;'>Ready to Install</h2>" +
        "<p style='color:#d7d7e6;'>Review the selected location, shortcuts, and components before copying files.</p>"
    );
};

Controller.prototype.PerformInstallationPageCallback = function() {
    applyPageCopyByName("PerformInstallationPage");
    setPageLabelText(
        "PerformInstallationPage",
        "MessageLabel",
        "<h2 style='color:#ffffff; margin:0;'>Installing</h2>" +
        "<p style='color:#d7d7e6;'>CodexBarX files are being copied and shortcuts are being prepared.</p>"
    );
};

Controller.prototype.FinishedPageCallback = function() {
    applyPageCopyByName("FinishedPage");
    setPageLabelText(
        "FinishedPage",
        "MessageLabel",
        "<h2 style='color:#ffffff; margin:0;'>Installation Complete</h2>" +
        "<p style='color:#d7d7e6;'>CodexBarX has been installed successfully.</p>" +
        "<p style='color:#aaaaaa;'>You can launch it now or close this installer and start it later from the Start Menu.</p>"
    );
};
