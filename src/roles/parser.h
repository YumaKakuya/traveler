// Reference: ~/hatch-v3/packages/opencode/src/agent/roles.ts:61-155 (parseRoles)
// Reference: Traveler_Phase0_Spec_v0.1.md §8.1 (REQ-ROLES-1, REQ-ROLES-2)
#pragma once

#include "types.h"
#include <string>
#include <unordered_map>

// Forward declare tl::expected
// tl::expected is a header-only C++11/14/17 implementation of std::expected
// Available via xrepo: add_requires("tl_expected")
// Or vendored single-header: third_party/tl/expected.hpp
#include <tl/expected.hpp>

namespace traveler::roles {

// ============================================================================
// parseRoles — port of parseRoles() from roles.ts L61-155
//
// Reads roles.md from the given directory and returns a map of role name
// to ParsedRole. On file-not-found, returns empty map (no error).
// On parse/validation failure, returns Error with message (faithful to
// roles.ts which logs warnings and returns empty map).
// ============================================================================
tl::expected<std::unordered_map<std::string, ParsedRole>, Error>
parseRoles(const std::string& directory);

// ============================================================================
// Constants from roles.ts
// ============================================================================

// Protected agent names (roles.ts L48): override forbidden
// (usable by RoleRegistry::lookup)
constexpr const char* PROTECTED_NAMES[] = {"compaction", "title", "summary"};

// Overridable names (roles.ts L51): built-in can be overridden
constexpr const char* OVERRIDABLE_NAMES[] = {"build", "plan", "general", "explore"};

// roles.md name validation regex (roles.ts L54): ASCII alphanumeric + dash + underscore
// Equivalent to: ^[a-zA-Z0-9_-]+$
bool isValidRoleName(const std::string& name);

// Model format validation: must contain '/' separator (roles.ts L135-136)
bool isValidModelFormat(const std::string& model);

}  // namespace traveler::roles
