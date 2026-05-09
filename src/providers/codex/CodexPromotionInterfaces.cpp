#include "CodexPromotionInterfaces.h"
#include "CodexAtomicFileSwap.h"

#include <QFile>

std::optional<QByteArray> CodexAuthMaterialReader::readAuthData(const QString& homePath)
{
    QString authPath = homePath + QStringLiteral("/auth.json");
    QFile file(authPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    QByteArray data = file.readAll();
    file.close();
    return data;
}

bool CodexLiveAuthSwapper::swapLiveAuthData(const QByteArray& data, const QString& liveHomePath, QString& errorMessage)
{
    QString authPath = liveHomePath + QStringLiteral("/auth.json");
    CodexAtomicFileSwap swapper(authPath);

    if (!swapper.stageFile(data)) {
        errorMessage = swapper.errorMessage();
        return false;
    }

    if (!swapper.commit()) {
        errorMessage = swapper.errorMessage();
        return false;
    }

    return true;
}
