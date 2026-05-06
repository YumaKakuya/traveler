// GATE-P0-4 T2: Roles dispatcher integration with Cockpit @-mention
// Reference: Traveler_Phase0_Spec_v0.1.md §8.1 (T2)
//
// Implementation is header-only in dispatch.h for Phase 0 testability.
// When the Cockpit dispatcher + provider request construction are
// completed, this file will host non-trivial dispatch logic:
//   - Provider fan-out (route to anthropic_adapter / openai_adapter / etc.)
//   - System prompt prepend + request body assembly
//   - Callsign validation against the 4-roster limit
//
// A6 registration: add this file to the traveler target in xmake.lua.
#include "dispatch.h"
