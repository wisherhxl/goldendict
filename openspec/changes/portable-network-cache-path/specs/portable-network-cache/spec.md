## Purpose

Keep portable HTTP cache side effects inside the selected installation while
preserving explicit injection, normal-profile compatibility and runtime ownership.

## ADDED Requirements

### Requirement: P1 selected Network cache root
The application SHALL select an existing explicit cache root unchanged when
provided; otherwise portable mode SHALL use its established data root/cache,
and non-portable mode SHALL retain its current platform default root.
Selection MUST precede cache preparation side effects and remain consistent
through startup, reapplication and recovery. The owned qt-network-http child
MUST be appended exactly once by the Network storage owner.

#### Scenario: Portable startup and reapplication
- **WHEN** portable configuration is selected without an explicit root override
- **THEN** enabled, disabled and reapplied Network policies use only portable/cache/qt-network-http

#### Scenario: Explicit injection and ordinary profile
- **WHEN** an explicit root is provided, or a non-portable profile has no override
- **THEN** the explicit root or original platform default respectively is preserved unchanged

### Requirement: P1 failure and cleanup containment
Unavailable portable cache storage SHALL preserve the existing caller-specific
failure channel: startup reports uncached degradation, while failed positive-cache
Preferences preparation retains the previous runtime/configuration. It MUST NOT
fall back to the daily user cache.
Cleanup SHALL affect only the selected Network-owned child. Old default-cache
data MUST NOT be migrated, written, deleted or inspected for a writable fallback.
Existing prepare/abandon/publish/retire/lease contracts SHALL remain unchanged.

#### Scenario: Unavailable directory
- **WHEN** the portable cache path cannot be prepared
- **THEN** setup failure is reported through the existing channel without selecting or modifying the default root

#### Scenario: Disable or clean shutdown
- **WHEN** selected cache policy removes owned cached content
- **THEN** adjacent files and simulated old-default data remain unchanged

#### Scenario: Abandoned and consecutive candidates
- **WHEN** a prepared policy is abandoned or aborted and later policies are applied
- **THEN** active ownership remains valid, consumed candidates are not reused, and all operations retain the selected root
