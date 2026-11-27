#pragma once

#include <QString>
#include <QStringList>

// Checks a copy or move before anything is written, so a job that would
// destroy its own source is refused instead of being discovered afterwards.
namespace TransferPlan {

// Absolute, cleaned form used for identity checks.
QString normalised(const QString &path);

// True when two paths name the same entry. Two different routes to one folder
// count as the same folder.
bool samePath(const QString &a, const QString &b);

// True when `child` is `parent` or lies below it.
bool isInside(const QString &child, const QString &parent);

// The path a source would take in `targetDir`, or an empty string when the
// source has no usable name.
QString destinationFor(const QString &source, const QString &targetDir);

// Why the job cannot run, or an empty string. Identity moves are allowed and
// are no-ops; moving a folder into itself is not.
QString problem(const QString &source, const QString &targetDir, bool moveFiles);

// Every problem for the whole job, including two sources that would land on
// the same destination. Empty means the job can start.
QStringList validate(const QStringList &sources, const QString &targetDir, bool moveFiles);

} // namespace TransferPlan
