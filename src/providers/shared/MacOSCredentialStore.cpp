#include "MacOSCredentialStore.h"

#include <Security/Security.h>

namespace {

static constexpr const char* kServiceName = "CodexBar";

QByteArray utf8(const QString& value)
{
    return value.toUtf8();
}

CFDataRef cfData(const QByteArray& value)
{
    return CFDataCreate(kCFAllocatorDefault,
                        reinterpret_cast<const UInt8*>(value.constData()),
                        static_cast<CFIndex>(value.size()));
}

CFStringRef cfString(const QByteArray& value)
{
    return CFStringCreateWithBytes(kCFAllocatorDefault,
                                   reinterpret_cast<const UInt8*>(value.constData()),
                                   static_cast<CFIndex>(value.size()),
                                   kCFStringEncodingUTF8,
                                   false);
}

CFMutableDictionaryRef baseQuery(const QString& target)
{
    QByteArray service = QByteArray(kServiceName);
    QByteArray account = utf8(target);

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                            0,
                                                            &kCFTypeDictionaryKeyCallBacks,
                                                            &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);

    CFStringRef serviceRef = cfString(service);
    CFStringRef accountRef = cfString(account);
    CFDictionarySetValue(query, kSecAttrService, serviceRef);
    CFDictionarySetValue(query, kSecAttrAccount, accountRef);
    CFRelease(serviceRef);
    CFRelease(accountRef);

    return query;
}

} // namespace

bool MacOSCredentialStore::write(const QString& target,
                                 const QString& username,
                                 const QByteArray& secret)
{
    remove(target);

    CFMutableDictionaryRef item = baseQuery(target);
    CFDataRef secretData = cfData(secret);
    CFDictionarySetValue(item, kSecValueData, secretData);

    if (!username.isEmpty()) {
        QByteArray label = utf8(username);
        CFStringRef labelRef = cfString(label);
        CFDictionarySetValue(item, kSecAttrLabel, labelRef);
        CFRelease(labelRef);
    }

    OSStatus status = SecItemAdd(item, nullptr);
    CFRelease(secretData);
    CFRelease(item);
    return status == errSecSuccess;
}

std::optional<QByteArray> MacOSCredentialStore::read(const QString& target)
{
    CFMutableDictionaryRef query = baseQuery(target);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);

    CFTypeRef result = nullptr;
    OSStatus status = SecItemCopyMatching(query, &result);
    CFRelease(query);
    if (status != errSecSuccess || !result) {
        return std::nullopt;
    }

    auto dataRef = static_cast<CFDataRef>(result);
    QByteArray data(reinterpret_cast<const char*>(CFDataGetBytePtr(dataRef)),
                    static_cast<int>(CFDataGetLength(dataRef)));
    CFRelease(result);
    return data;
}

bool MacOSCredentialStore::remove(const QString& target)
{
    CFMutableDictionaryRef query = baseQuery(target);
    OSStatus status = SecItemDelete(query);
    CFRelease(query);
    return status == errSecSuccess || status == errSecItemNotFound;
}

bool MacOSCredentialStore::exists(const QString& target)
{
    CFMutableDictionaryRef query = baseQuery(target);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);
    OSStatus status = SecItemCopyMatching(query, nullptr);
    CFRelease(query);
    return status == errSecSuccess;
}
