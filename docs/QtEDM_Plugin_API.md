# QtEDM Plugin API Version 1

QtEDM version 1 plugins are local Qt libraries that implement one or more
interfaces from `qtedm/qtedm_plugin_api.h`. The supported extension points are
display objects, data providers, and archive providers. This API deliberately
does not provide filesystem, process-launch, or unrestricted application
access.

## Installation and discovery

Install a plugin library and its metadata sidecar in either:

- the `plugins` directory next to the QtEDM executable; or
- an absolute local directory listed in `QTEDM_PLUGIN_PATH`.

`QTEDM_PLUGIN_PATH` uses the platform path-list separator (`:` on Unix and `;`
on Windows). Relative paths, missing directories, non-library files, and
libraries without valid metadata are rejected. Remote loading is not
supported.

For a library named `libfacility_widgets.so`, the required sidecar is
`libfacility_widgets.so.qtedm-plugin.json`. The suffix is appended to the full
platform library name, including `.dll`, `.dylib`, or `.so`.

```json
{
  "schema": "org.aps.qtedm.plugin-metadata",
  "schema_version": 1,
  "plugin_id": "org.example.facility.widgets",
  "interfaces": [
    "display",
    "data",
    "archive"
  ]
}
```

The loader limits metadata to 64 KiB. `plugin_id` and `interfaces` must exactly
match the interfaces exposed by the Qt object. Unknown schemas, duplicate
interfaces, and metadata/binary disagreement reject the library before it is
registered.

## Binary compatibility

Every implemented interface returns `qtedmCurrentPluginCompatibility()`.
QtEDM requires an exact match for:

- `QTEDM_PLUGIN_INTERFACE_VERSION`;
- the Qt major version;
- the build CPU architecture; and
- compiler family and version ABI.

Rebuild plugins for each QtEDM toolchain and target architecture. A library
compiled for another Qt major, compiler version, or architecture is rejected
with a diagnostic rather than loaded optimistically.

The Qt plugin class needs `Q_OBJECT`, one `Q_PLUGIN_METADATA` declaration, and
all implemented interfaces in `Q_INTERFACES`. The checked-in
`qtedm/tests/plugin_api_discovery_plugin.cc` is the smallest buildable display-plugin
example.

## Display-object plugins

Implement `QtedmDisplayObjectPluginInterface` and register stable, lowercase
type IDs. Each `QtedmDisplayObjectType` supplies palette name/category,
default size, schema version, and typed property declarations. Version 1
property types are Boolean, integer, double, string, color, and string list.

QtEDM calls the plugin to:

1. construct the child `QWidget`;
2. apply and serialize typed properties;
3. enumerate all PV or provider channels used by the object; and
4. create an optional execute-mode `QtedmPluginRuntime`.

Runtime code receives only `QtedmPluginHost`. Use it for subscriptions,
diagnostics, and writes. Never retain a widget or host subscription past
`stop()`. Construction, property, serialization, channel, and runtime
exceptions are contained and reported as plugin diagnostics.

Saved objects use an explicit extension block:

```text
qtedm_plugin {
  pluginId="org.example.facility.widgets"
  typeId="vacuum_summary"
  schemaVersion=1
  object { x=20 y=40 width=220 height=90 }
  property { name="title" type="string" value="Sector 4" }
  property { name="channel" type="string" value="VAC:S4:STATE" }
}
```

Unknown properties and children are retained. A missing plugin or a newer
unsupported object schema produces a visible diagnostic placeholder, and the
original raw node survives save, reopen, copy/paste, and undo/redo.

## Data-provider plugins

Implement `QtedmDataProviderPluginInterface` and register one or more URI
schemes such as `facility://`. The built-in `ca` and `pva` schemes are
reserved. Subscriptions return a `QtedmDataSubscription`; `cancel()` must be
idempotent and promptly detach provider callbacks.

The provider reports value, connection, and access-right changes with
`QtedmChannelCallbacks`. It must honor the requested passive or realtime
delivery mode and keep callback work bounded.

All plugin-provider puts pass through `PvChannelManager` before the provider's
`put()` implementation. Consequently observe-only mode blocks them before the
plugin sees a value, and both blocked and successful operations use the common
audit path. Display plugins must also write only through
`QtedmPluginHost::put()`; direct CA/PVA client writes are outside the supported
contract.

## Archive-provider plugins

Implement `QtedmArchiveProviderPluginInterface` and return an
`ArchiveProvider` from `qtedm/archive_provider.h`. The query supplies explicit
time bounds, maximum points, timeout, and response-byte limits. Providers must
respect those bounds, complete asynchronously, observe owner/request
cancellation, and return partial/failure state through `ArchiveResult`.

Choose a provider for `qtedm_archive_plot` with
`QTEDM_ARCHIVER_PROVIDER=<provider-id>`. An empty value or
`archiver-appliance` selects the built-in provider. A missing plugin provider
leaves live plotting active and displays the archive diagnostic.

## Diagnostics and shutdown

Rejected libraries, duplicate plugin/type/scheme/provider IDs, construction
failures, and provider errors appear in QtEDM test-state and application
diagnostics. QtEDM cancels data subscriptions before unregistering types and
unloading libraries. Plugin shutdown and subscription cancellation must be
idempotent because handles can be released during display close as well as
application teardown.

## Security boundary

Plugin libraries are native code and therefore trusted local extensions. QtEDM
does not download, discover, or sandbox remote plugins. Facilities should
install reviewed binaries in administrator-controlled paths. Declarative rules
are the sandboxed automation mechanism for ordinary display authors; they are
not a substitute for native-plugin review.
