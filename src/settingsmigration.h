#pragma once

#include <QString>

namespace SettingsMigration {

struct State
{
    bool legacyHasKeys = false;
    bool currentFileExists = false;
    bool currentFileReadable = false;
    bool currentHasKeys = false;
};

// Returns true when copying legacy values into the current store is safe.
// An existing but unreadable current file is never overwritten.
bool shouldCopyLegacy(const State &state);

} // namespace SettingsMigration