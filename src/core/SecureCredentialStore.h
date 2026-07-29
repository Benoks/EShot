#ifndef SECURECREDENTIALSTORE_H
#define SECURECREDENTIALSTORE_H

#include <QString>

#include <functional>

class QSettings;

namespace SecureCredentialStore {

using WriteFunction = std::function<bool(const QString &key, const QString &secret)>;

QString read(const QString &key);
bool write(const QString &key, const QString &secret);
bool remove(const QString &key);

// Removes a legacy QSettings value even when the operating-system credential
// store is unavailable, so OAuth tokens are never retained as plain text.
QString migrateLegacyToken(QSettings &settings, const QString &legacyKey,
                           const WriteFunction &writeSecret);

} // namespace SecureCredentialStore

#endif
