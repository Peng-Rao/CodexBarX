function Component() {
    // Constructor
}

Component.prototype.createOperations = function() {
    try {
        component.createOperations();

        if (installer.value("os") === "win") {
            // Create desktop shortcut
            component.addOperation("CreateShortcut",
                "@TargetDir@/WinCodexBar.exe",
                "@DesktopDir@/WinCodexBar.lnk",
                "workingDirectory=@TargetDir@,iconPath=@TargetDir@/WinCodexBar.exe,iconId=0"
            );

            // Create start menu shortcut
            component.addOperation("CreateShortcut",
                "@TargetDir@/WinCodexBar.exe",
                "@StartMenuDir@/WinCodexBar.lnk",
                "workingDirectory=@TargetDir@,iconPath=@TargetDir@/WinCodexBar.exe,iconId=0"
            );

            // Create uninstall shortcut in start menu
            component.addOperation("CreateShortcut",
                "@TargetDir@/@MaintenanceToolName@.exe",
                "@StartMenuDir@/Uninstall WinCodexBar.lnk",
                "workingDirectory=@TargetDir@"
            );

            // Register uninstaller in Windows Add/Remove Programs
            component.addOperation("RegisterFileType",
                "WinCodexBar.Assoc",
                "@TargetDir@/WinCodexBar.exe",
                "WinCodexBar Application"
            );

            component.addElevatedOperation("Settings",
                "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WinCodexBar",
                "DisplayName", "WinCodexBar",
                "string"
            );

            component.addElevatedOperation("Settings",
                "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WinCodexBar",
                "DisplayVersion", "@Version@",
                "string"
            );

            component.addElevatedOperation("Settings",
                "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WinCodexBar",
                "Publisher", "WinCodexBar",
                "string"
            );

            component.addElevatedOperation("Settings",
                "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WinCodexBar",
                "UninstallString",
                "@TargetDir@/@MaintenanceToolName@.exe",
                "string"
            );

            component.addElevatedOperation("Settings",
                "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WinCodexBar",
                "InstallLocation",
                "@TargetDir@",
                "string"
            );

            component.addElevatedOperation("Settings",
                "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WinCodexBar",
                "DisplayIcon",
                "@TargetDir@/WinCodexBar.exe,0",
                "string"
            );

            component.addElevatedOperation("Settings",
                "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WinCodexBar",
                "NoModify",
                "1",
                "dword"
            );

            component.addElevatedOperation("Settings",
                "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WinCodexBar",
                "NoRepair",
                "1",
                "dword"
            );
        }
    } catch (e) {
        print("Error in createOperations: " + e);
    }
};

Component.prototype.createOperationsForArchive = function(archive) {
    component.addOperation("Extract", archive, "@TargetDir@");
};

Component.prototype.createOperationsForPath = function(path) {
    component.addOperation("Copy", path, "@TargetDir@/" + path);
};
