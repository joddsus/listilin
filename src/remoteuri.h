#pragma once

#include <QString>
#include <QStringList>

// Helpers for validating and building remote (FTP/FTPS/SFTP/SMB) URIs. Kept
// free of widgets so the rules can be tested without a GUI.
namespace RemoteUri {

// Returns true for schemes Listista can browse remotely.
bool isSupportedScheme(const QString &scheme);

// Returns true for a host name or address without userinfo, port or path.
bool isValidHost(const QString &host);

// Returns true when `port` is empty or a number in the valid TCP range.
bool isValidPort(const QString &port, int *value = nullptr);

// Returns true when `path` is an absolute remote path without traversal.
bool isSafePath(const QString &path);

// Normalises a remote path to an absolute, slash-separated form.
QString normalizePath(const QString &path);

// Builds a remote URI from its parts, validating each one. Returns an empty
// string and sets `error` when a part is invalid.
QString build(const QString &scheme, const QString &host, const QString &port,
              const QString &user, const QString &password, const QString &path,
              QString *error = nullptr);

} // namespace RemoteUri