#include "CodexActiveSource.h"

QString CodexActiveSourceUtil::toString(CodexActiveSource source)
{
    switch (source) {
        case CodexActiveSource::LiveSystem:
            return QStringLiteral("liveSystem");
        case CodexActiveSource::ManagedAccount:
            return QStringLiteral("managedAccount");
    }
    return QStringLiteral("unknown");
}

CodexActiveSource CodexActiveSourceUtil::fromString(const QString& str)
{
    if (str == QStringLiteral("liveSystem")) {
        return CodexActiveSource::LiveSystem;
    }
    if (str == QStringLiteral("managedAccount")) {
        return CodexActiveSource::ManagedAccount;
    }
    return CodexActiveSource::LiveSystem;
}

QVariant CodexActiveSourceUtil::toVariant(CodexActiveSource source)
{
    return QVariant::fromValue(static_cast<int>(source));
}
