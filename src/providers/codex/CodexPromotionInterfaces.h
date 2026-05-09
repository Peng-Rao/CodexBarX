#pragma once

#include <QString>
#include <QByteArray>
#include <optional>

#include "CodexActiveSource.h"

/**
 * @brief Interface for reading Codex auth material.
 *
 * Similar to Swift's CodexAuthMaterialReading protocol.
 * Allows mocking for unit tests.
 */
class ICodexAuthMaterialReader {
public:
    virtual ~ICodexAuthMaterialReader() = default;
    virtual std::optional<QByteArray> readAuthData(const QString& homePath) = 0;
};

/**
 * @brief Interface for swapping live Codex auth data.
 *
 * Similar to Swift's CodexLiveAuthSwapping protocol.
 * Allows mocking for unit tests.
 */
class ICodexLiveAuthSwapper {
public:
    virtual ~ICodexLiveAuthSwapper() = default;
    virtual bool swapLiveAuthData(const QByteArray& data, const QString& liveHomePath, QString& errorMessage) = 0;
};

/**
 * @brief Interface for writing Codex active source.
 *
 * Similar to Swift's CodexActiveSourceWriting protocol.
 */
class ICodexActiveSourceWriter {
public:
    virtual ~ICodexActiveSourceWriter() = default;
    virtual void writeCodexActiveSource(CodexActiveSource source) = 0;
};

/**
 * @brief Default implementation of ICodexAuthMaterialReader.
 */
class CodexAuthMaterialReader : public ICodexAuthMaterialReader {
public:
    std::optional<QByteArray> readAuthData(const QString& homePath) override;
};

/**
 * @brief Default implementation of ICodexLiveAuthSwapper using atomic file swap.
 */
class CodexLiveAuthSwapper : public ICodexLiveAuthSwapper {
public:
    bool swapLiveAuthData(const QByteArray& data, const QString& liveHomePath, QString& errorMessage) override;
};
