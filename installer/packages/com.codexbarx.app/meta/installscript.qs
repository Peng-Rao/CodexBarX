function Component() {
    // Constructor
}

Component.prototype.createOperations = function() {
    try {
        component.createOperations();

        if (installer.value("os") === "win") {
            // Create desktop shortcut
            component.addOperation("CreateShortcut",
                "@TargetDir@/CodexBarX.exe",
                "@DesktopDir@/CodexBarX.lnk",
                "workingDirectory=@TargetDir@,iconPath=@TargetDir@/CodexBarX.exe,iconId=0"
            );

            // Create start menu shortcut
            component.addOperation("CreateShortcut",
                "@TargetDir@/CodexBarX.exe",
                "@StartMenuDir@/CodexBarX.lnk",
                "workingDirectory=@TargetDir@,iconPath=@TargetDir@/CodexBarX.exe,iconId=0"
            );

            // Create uninstall shortcut in start menu
            component.addOperation("CreateShortcut",
                "@TargetDir@/@MaintenanceToolName@.exe",
                "@StartMenuDir@/Uninstall CodexBarX.lnk",
                "workingDirectory=@TargetDir@"
            );

            // Register uninstaller in Windows Add/Remove Programs
            component.addOperation("RegisterFileType",
                "CodexBarX.Assoc",
                "@TargetDir@/CodexBarX.exe",
                "CodexBarX Application"
            );

            component.addElevatedOperation("Settings",
                "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CodexBarX",
                "DisplayName", "CodexBarX",
                "string"
            );

            component.addElevatedOperation("Settings",
                "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CodexBarX",
                "DisplayVersion", "@Version@",
                "string"
            );

            component.addElevatedOperation("Settings",
                "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CodexBarX",
                "Publisher", "CodexBarX",
                "string"
            );

            component.addElevatedOperation("Settings",
                "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CodexBarX",
                "UninstallString",
                "@TargetDir@/@MaintenanceToolName@.exe",
                "string"
            );

            component.addElevatedOperation("Settings",
                "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CodexBarX",
                "InstallLocation",
                "@TargetDir@",
                "string"
            );

            component.addElevatedOperation("Settings",
                "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CodexBarX",
                "DisplayIcon",
                "@TargetDir@/CodexBarX.exe,0",
                "string"
            );

            component.addElevatedOperation("Settings",
                "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CodexBarX",
                "NoModify",
                "1",
                "dword"
            );

            component.addElevatedOperation("Settings",
                "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CodexBarX",
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
