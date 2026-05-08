#pragma once

#include <QString>
#include <optional>

enum class UsageProvider : int {
#define CODEXBAR_PROVIDER(ClassName, stringId, enumName) enumName,
#include "../providers/ProviderDefs.def"
};

struct ProviderIdentitySnapshot {
    std::optional<UsageProvider> providerID;
    std::optional<QString> accountEmail;
    std::optional<QString> accountOrganization;
    std::optional<QString> loginMethod;
};
