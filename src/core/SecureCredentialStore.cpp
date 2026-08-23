#include "SecureCredentialStore.h"

#include <QDebug>
#include <QSettings>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincred.h>
#elif defined(Q_OS_UNIX)
#pragma push_macro("signals")
#undef signals
#include <libsecret/secret.h>
#pragma pop_macro("signals")
#endif

namespace {
#ifdef Q_OS_WIN
QString targetName(const QString &key)
{
    return QStringLiteral("io.github.benoks.EShot/") + key;
}
#elif defined(Q_OS_UNIX)
const SecretSchema kOAuthTokenSchema = {
    "io.github.benoks.EShot",
    SECRET_SCHEMA_NONE,
    {{"credential", SECRET_SCHEMA_ATTRIBUTE_STRING}, {nullptr, static_cast<SecretSchemaAttributeType>(0)}}
};

QByteArray utf8(const QString &value)
{
    return value.toUtf8();
}
#endif
}

namespace SecureCredentialStore {

QString read(const QString &key)
{
#ifdef Q_OS_WIN
    PCREDENTIALW credential = nullptr;
    const QString target = targetName(key);
    if (!CredReadW(reinterpret_cast<LPCWSTR>(target.utf16()), CRED_TYPE_GENERIC, 0, &credential))
        return QString();

    const QString secret = QString::fromUtf8(
        reinterpret_cast<const char *>(credential->CredentialBlob),
        static_cast<qsizetype>(credential->CredentialBlobSize));
    CredFree(credential);
    return secret;
#elif defined(Q_OS_UNIX)
    const QByteArray attribute = utf8(key);
    GError *error = nullptr;
    gchar *storedSecret = secret_password_lookup_sync(
        &kOAuthTokenSchema, nullptr, &error, "credential", attribute.constData(), nullptr);
    if (error) {
        g_error_free(error);
        return QString();
    }
    const QString secret = storedSecret ? QString::fromUtf8(storedSecret) : QString();
    secret_password_free(storedSecret);
    return secret;
#else
    Q_UNUSED(key)
    return QString();
#endif
}

bool write(const QString &key, const QString &secret)
{
    if (secret.isEmpty())
        return remove(key);

#ifdef Q_OS_WIN
    const QString target = targetName(key);
    const QByteArray encoded = secret.toUtf8();
    CREDENTIALW credential = {};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(target.utf16()));
    credential.CredentialBlobSize = static_cast<DWORD>(encoded.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(encoded.constData()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    return CredWriteW(&credential, 0) != FALSE;
#elif defined(Q_OS_UNIX)
    const QByteArray attribute = utf8(key);
    const QByteArray value = utf8(secret);
    GError *error = nullptr;
    const gboolean stored = secret_password_store_sync(
        &kOAuthTokenSchema, SECRET_COLLECTION_DEFAULT, "EShot OAuth token", value.constData(),
        nullptr, &error, "credential", attribute.constData(), nullptr);
    if (error)
        g_error_free(error);
    return stored;
#else
    Q_UNUSED(key)
    Q_UNUSED(secret)
    return false;
#endif
}

bool remove(const QString &key)
{
#ifdef Q_OS_WIN
    const QString target = targetName(key);
    return CredDeleteW(reinterpret_cast<LPCWSTR>(target.utf16()), CRED_TYPE_GENERIC, 0) != FALSE;
#elif defined(Q_OS_UNIX)
    const QByteArray attribute = utf8(key);
    GError *error = nullptr;
    const gboolean cleared = secret_password_clear_sync(
        &kOAuthTokenSchema, nullptr, &error, "credential", attribute.constData(), nullptr);
    if (error)
        g_error_free(error);
    return cleared;
#else
    Q_UNUSED(key)
    return false;
#endif
}

QString migrateLegacyToken(QSettings &settings, const QString &legacyKey,
                           const WriteFunction &writeSecret)
{
    if (!settings.contains(legacyKey))
        return QString();

    const QString token = settings.value(legacyKey).toString().trimmed();

    if (!token.isEmpty() && !writeSecret(legacyKey, token)) {
        qWarning() << "Secure credential storage is unavailable; token will only be kept for this session";
        return token;
    }
    settings.remove(legacyKey);
    settings.sync();
    return token;
}

} // namespace SecureCredentialStore
