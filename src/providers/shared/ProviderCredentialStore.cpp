#include "ProviderCredentialStore.h"

#ifdef Q_OS_MACOS
#include "MacOSCredentialStore.h"
#elif defined(Q_OS_WIN)
#include "WindowsCredentialStore.h"
#endif

#include <mutex>

namespace {

class NativeCredentialBackend : public ProviderCredentialBackend {
public:
    bool write(const QString& target, const QString& username, const QByteArray& secret) override {
#ifdef Q_OS_MACOS
        return MacOSCredentialStore::write(target, username, secret);
#elif defined(Q_OS_WIN)
        return WindowsCredentialStore::write(target, username, secret);
#else
        Q_UNUSED(target)
        Q_UNUSED(username)
        Q_UNUSED(secret)
        return false;
#endif
    }

    std::optional<QByteArray> read(const QString& target) override {
#ifdef Q_OS_MACOS
        return MacOSCredentialStore::read(target);
#elif defined(Q_OS_WIN)
        return WindowsCredentialStore::read(target);
#else
        Q_UNUSED(target)
        return std::nullopt;
#endif
    }

    bool remove(const QString& target) override {
#ifdef Q_OS_MACOS
        return MacOSCredentialStore::remove(target);
#elif defined(Q_OS_WIN)
        return WindowsCredentialStore::remove(target);
#else
        Q_UNUSED(target)
        return false;
#endif
    }

    bool exists(const QString& target) override {
#ifdef Q_OS_MACOS
        return MacOSCredentialStore::exists(target);
#elif defined(Q_OS_WIN)
        return WindowsCredentialStore::exists(target);
#else
        Q_UNUSED(target)
        return false;
#endif
    }
};

std::shared_ptr<ProviderCredentialBackend>& backend()
{
    static std::shared_ptr<ProviderCredentialBackend> instance =
        std::make_shared<NativeCredentialBackend>();
    return instance;
}

std::mutex& backendMutex()
{
    static std::mutex mutex;
    return mutex;
}

} // namespace

bool ProviderCredentialStore::write(const QString& target,
                                    const QString& username,
                                    const QByteArray& secret)
{
    std::lock_guard<std::mutex> lock(backendMutex());
    return backend()->write(target, username, secret);
}

std::optional<QByteArray> ProviderCredentialStore::read(const QString& target)
{
    std::lock_guard<std::mutex> lock(backendMutex());
    return backend()->read(target);
}

bool ProviderCredentialStore::remove(const QString& target)
{
    std::lock_guard<std::mutex> lock(backendMutex());
    return backend()->remove(target);
}

bool ProviderCredentialStore::exists(const QString& target)
{
    std::lock_guard<std::mutex> lock(backendMutex());
    return backend()->exists(target);
}

void ProviderCredentialStore::setBackendForTesting(std::shared_ptr<ProviderCredentialBackend> testBackend)
{
    std::lock_guard<std::mutex> lock(backendMutex());
    backend() = testBackend ? std::move(testBackend) : std::make_shared<NativeCredentialBackend>();
}

void ProviderCredentialStore::resetBackendForTesting()
{
    std::lock_guard<std::mutex> lock(backendMutex());
    backend() = std::make_shared<NativeCredentialBackend>();
}

bool InMemoryCredentialBackend::write(const QString& target,
                                      const QString& username,
                                      const QByteArray& secret)
{
    Q_UNUSED(username)
    m_values[target] = secret;
    return true;
}

std::optional<QByteArray> InMemoryCredentialBackend::read(const QString& target)
{
    auto it = m_values.constFind(target);
    if (it == m_values.constEnd()) return std::nullopt;
    return it.value();
}

bool InMemoryCredentialBackend::remove(const QString& target)
{
    const bool existed = m_values.contains(target);
    m_values.remove(target);
    return existed;
}

bool InMemoryCredentialBackend::exists(const QString& target)
{
    return m_values.contains(target);
}
