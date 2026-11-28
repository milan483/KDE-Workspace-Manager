# Profile format

## Status and compatibility

The Profile Engine persists profiles as versioned JSON documents. The current persisted format is schema version **2**. Schema version 1 remains readable through an explicit migration.

- The top-level `schema_version` property is required and must be an integer.
- V2 rejects unknown properties rather than silently discarding data.
- UUIDs use canonical lowercase, hyphenated text without braces.
- A missing, malformed, older unsupported, or newer unsupported schema version is rejected.
- V1-to-V2 migration maps `state.maximized: true` to `state.maximize_mode: "full"` and `false` to `"none"`.
- There is no v0 format and no synthetic v0-to-v1 migration.

The C++ implementation lives in the independent `kwm_profiles` library. It depends on Qt Core and does not depend on Kirigami, KWin, D-Bus, or the application UI.

## Example document

```json
{
  "schema_version": 2,
  "profile": {
    "id": "7f13c79e-6d12-4b3c-9a44-27f8b6cfcf01",
    "name": "Development"
  },
  "active_desktop_id": "10000000-0000-4000-8000-000000000001",
  "virtual_desktops": [
    {
      "id": "10000000-0000-4000-8000-000000000001",
      "name": "Development"
    },
    {
      "id": "10000000-0000-4000-8000-000000000002",
      "name": "Communication"
    }
  ],
  "applications": [
    {
      "id": "30000000-0000-4000-8000-000000000001",
      "launch": {
        "desktop_entry_id": "org.kde.kate",
        "arguments": [
          "--new-window",
          "/home/user/project"
        ],
        "working_directory": "/home/user/project"
      },
      "windows": [
        {
          "id": "40000000-0000-4000-8000-000000000001",
          "match": {
            "application_identity": {
              "kind": "desktop_file_name",
              "value": "org.kde.kate"
            },
            "role": "main"
          },
          "desktop_membership": {
            "mode": "specific",
            "desktop_ids": [
              "10000000-0000-4000-8000-000000000001",
              "10000000-0000-4000-8000-000000000002"
            ]
          },
          "geometry": {
            "x": 0.05,
            "y": 0.05,
            "width": 0.9,
            "height": 0.9
          },
          "state": {
            "minimized": false,
            "maximize_mode": "full",
            "fullscreen": false
          }
        },
        {
          "id": "40000000-0000-4000-8000-000000000002",
          "match": {
            "application_identity": {
              "kind": "resource_class",
              "value": "org.kde.kate"
            }
          },
          "desktop_membership": {
            "mode": "all"
          },
          "state": {
            "minimized": true,
            "maximize_mode": "none",
            "fullscreen": true
          }
        }
      ]
    }
  ]
}
```

The example is illustrative. Runtime window handles and KWin UUIDs are never written into the document.

## Object model

### Profile

| Property | Type | Required | Meaning |
|---|---|---:|---|
| `schema_version` | integer | Yes | Version of the persisted JSON schema |
| `profile.id` | UUID string | Yes | Stable profile identifier |
| `profile.name` | string | Yes | Non-empty display name |
| `active_desktop_id` | UUID string | No | Logical ID of the profile's active desktop |
| `virtual_desktops` | array | Yes | Ordered logical virtual desktops; may be empty |
| `applications` | array | Yes | Application records; may be empty |

The active desktop, when present, must reference an entry in `virtual_desktops`.

### Virtual desktop

Each desktop has a unique UUID `id` and a non-empty `name`. The array order is the profile's logical desktop order.

These identifiers belong to the profile format. They are not KWin runtime desktop identifiers. During capture, the adapter maps runtime KWin desktop UUIDs to newly generated logical IDs; restoration will resolve logical IDs to runtime IDs separately.

### Application

Each application has:

- A unique UUID `id`.
- An optional `launch` specification.
- A required `windows` array, which may be empty.

A launch specification is independent of window matching:

| Property | Type | Required | Meaning |
|---|---|---:|---|
| `desktop_entry_id` | string | Yes | Desktop entry used to launch the application |
| `arguments` | string array | Yes | Argument vector; an empty array is valid |
| `working_directory` | string | No | Working directory supplied to the launcher |

The profile model does not turn a window identity into a launch command. Application launching is outside the Profile Engine's scope. V1 does not include application-specific session data.

### Window

Each window record has a unique logical UUID `id`, a `match` object, a `desktop_membership` object, an optional `geometry`, and a `state` object.

The window UUID identifies the persisted record only. It must never contain a temporary KWin window UUID.

#### Matching identity

`match.application_identity` contains:

- `kind: "desktop_file_name"` with the `desktopFileName` value when available.
- `kind: "resource_class"` with the `resourceClass` value when no desktop file name is available.

The two kinds are explicit and are not interchangeable. `role` is an optional discriminator. Window titles are not primary identity. Matching and ambiguity resolution are performed by a later restoration component; the Profile Engine does not guess identities.

#### Desktop membership

Desktop membership is a discriminated object:

- `{"mode":"specific","desktop_ids":[...]}` assigns the window to one or more logical desktops.
- `{"mode":"all"}` explicitly represents an all-desktop/sticky window.

For `specific`, `desktop_ids` is required, non-empty, unique, and every ID must reference a desktop in the same profile. For `all`, `desktop_ids` must be omitted.

This structure preserves multi-desktop membership and does not overload a singular `desktop` property.

#### Geometry

Geometry is optional. When present, it describes the normal/restored rectangle relative to the available work area:

- `x` and `y` are normalized origins.
- `width` and `height` are normalized dimensions.
- All four values must be finite.
- `x` and `y` must be non-negative.
- `width` and `height` must be positive.
- The rectangle must fit inside the normalized work area: `x + width <= 1` and `y + height <= 1`.

V2 is single-monitor. It does not persist a monitor identity or define cross-monitor geometry conversion. Geometry may be omitted when a normal/restored rectangle is unavailable or not meaningful for the captured state. In particular, a capture adapter must not save the maximized or fullscreen frame rectangle as normal geometry when KWin does not expose the underlying restore rectangle.

#### Window state

All three properties are required:

- `minimized` is a boolean.
- `maximize_mode` is one of `none`, `vertical`, `horizontal`, or `full`.
- `fullscreen` is a boolean.

Maximize mode and fullscreen are independent properties and may both be set when the window manager reports both states. Minimized may coexist with any underlying mode because minimizing does not erase the underlying window state.

## Identifier and reference rules

Profile, desktop, application, and window identifiers must be non-null UUIDs and unique across the entire profile document. Desktop references must resolve within the same profile.

Filenames are derived from the profile UUID, not from the profile name. The repository accepts only canonical UUID filenames ending in `.json`.

## Validation

Validation has two stages:

1. **Structural validation** checks JSON object/array/string/number/boolean types, required properties, UUID syntax, supported enum values, and unknown properties.
2. **Semantic validation** checks identifier uniqueness, desktop references, membership rules, geometry bounds, non-empty identity values, and incompatible window states.

Serialization validates the domain model before producing JSON. Deserialization performs JSON parsing, schema migration, structural parsing, and semantic validation. Invalid profiles are rejected; validation does not silently repair or normalize them.

Errors use `ProfileError` with a stable error code, a message, an optional JSON path or file path, and structured validation issues where applicable.

## Schema migration

`ProfileMigrator::migrateToCurrent()` checks the schema version and provides the sequential migration entry point.

V1 is the first persisted version and has no synthetic v0 migration. The V1-to-V2 migration replaces the required boolean `state.maximized` with the required string `state.maximize_mode`: `true` becomes `full` and `false` becomes `none`. The migration preserves `minimized` and `fullscreen`, advances exactly one version, and does not write the migrated document back to disk. Future schema changes must increment the version, register an explicit migration from the immediately preceding version, transform the JSON object to exactly the next version, and include success and failure tests.

Unknown future versions and older versions without a registered migration are rejected. Loading a migrated document does not overwrite its source file; persistence occurs only after an explicit repository save.

## Persistent storage

The default repository directory is:

```text
QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)/profiles/
```

Each profile is stored as:

```text
<profile-uuid>.json
```

The repository supports:

- `list()` — returns valid profiles and individual errors for corrupt or invalid profile files.
- `load(profileId)` — loads and validates one profile and verifies that the document ID matches its filename.
- `save(profile)` — validates and atomically creates or updates a profile (upsert).
- `remove(profileId)` — removes one profile.

Writes use `QSaveFile` with direct-write fallback disabled. A failed write or commit must not replace an existing profile with a partial document. Listing a directory with one corrupt profile still returns other valid profiles and reports the corrupt file separately.

Repository tests inject a temporary storage directory. They do not read or modify real user profile data.

## Implementation boundary

The Profile Engine owns the domain model, V2 JSON format, structural and semantic validation, sequential migrations, and persistent repository. It does not enumerate windows, launch applications, communicate with KWin or D-Bus, restore windows, switch desktops, or provide profile-management UI. Window enumeration and capture belong to the separate KWin adapter.
