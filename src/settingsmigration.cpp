#include "settingsmigration.h"

namespace SettingsMigration {

bool shouldCopyLegacy(const State &state)
{
    if (!state.legacyHasKeys)
        return false;
    if (state.currentHasKeys)
        return false;
    // An existing but unreadable file must be preserved, not overwritten.
    if (state.currentFileExists && !state.currentFileReadable)
        return false;
    return true;
}

} // namespace SettingsMigration