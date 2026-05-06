// GATE-P0-4 T3: /roles-reload command + Command Palette entry
// Reference: Traveler_Phase0_Spec_v0.1.md §8.1 (T3)
//
// Implementation is header-only in reload_cmd.h for Phase 0 testability.
// When the Command Palette / slash-command dispatcher is completed, this
// file will host:
//   - Slash-command registration ("/roles-reload" → reload_roles_cmd)
//   - Command Palette entry (name, description, keyboard shortcut)
//   - Integration with the GATE-P0-2 Command Palette dispatch loop
//
// A6 registration: add this file to the traveler target in xmake.lua.
#include "reload_cmd.h"
