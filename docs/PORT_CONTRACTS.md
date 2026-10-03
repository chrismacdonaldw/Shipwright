# Shipwright maintained-layer contracts

The inherited OoT engine/actor layouts coexist with maintained port, enhancement, UI, save and Anchor code. Preserve native gameplay semantics when improving a maintained seam; a different C++ idiom is not itself a correctness improvement. The target is an independently useful native port with supported optional embedding. This guide separates current guarantees from transition gaps.

## Widgets own their behavior and metadata

[`SohMenu::AddWidget`](../soh/soh/SohGui/SohMenu.cpp) creates options matching the native widget type; [`WidgetInfo`](../soh/soh/SohGui/MenuTypes.h) carries the CVar/value pointer, callbacks, pre/post functions and visibility conditions. [`AddMenuRandomizer`](../soh/soh/SohGui/SohMenuRandomizer.cpp) owns the actual categories, labels, default values, combo IDs and slider formats. Change that registration rather than maintaining another catalogue in a consumer.

`SohMenu::GetRandomizerEnhancements` exports copied typed data, not callbacks or options pointers. It filters supported widget types before touching CVar, preserves combo IDs and fractional defaults, and separates the human slider label from its format. `false` means the registry is unavailable; `true` with an empty vector is a valid empty result. Consumers must defer schema/default writes on unavailable, not reset preferences.

`AddMenuElements` marks the registry initialized after registrations; [`InitOTR`](../soh/soh/OTRGlobals.cpp) sets up menu elements before module randomizer initialization. Metadata availability requires initialization, not rendering or being the foreground game. The Diptych `ExportRandoSchema`/`RandoFrameSync` consumer retries deferred export. Pure generator defaults remain independent of this GUI registry.

Current `WidgetInfo` still couples its enum, raw CVar and polymorphic options; the copied export does not make arbitrary option/type mismatches safe. Preserve matching construction/casts when extending it. A broader typed-construction cleanup is not an implemented guarantee.

## Native hooks are the extension seam

[`GameInteractor`](../soh/soh/Enhancements/game-interactor/GameInteractor.h) supplies typed normal, ID, pointer and filtered hooks with deferred unregistration. Hooks must be idempotent and their execution order is not guaranteed. Put dependent operations in one hook rather than relying on registration order.

[`Flags_SetSwitch`/`Flags_UnsetSwitch`](../soh/src/code/z_actor.c) distinguish saved/current bits below 32 from temporary bits and emit producer events only on an actual change. Use native setters for local changes; inbound effects must avoid producing a network echo or an award. Do not reinterpret temporary puzzle/control state as saved shared progress.

## Anchor room authority and application

[`Anchor::PrepRoomState`](../soh/soh/Network/Anchor/Packets/UpdateRoomState.cpp) emits `syncItemsAndFlags` as integer 0/1; the global room hardcodes sharing OFF. The native [network menu](../soh/soh/SohGui/SohMenuNetwork.cpp) owns connected/admin/global-room permissions. Presence/player updates are separate from item/flag synchronization; OFF is not a blanket networking disable.

[`Anchor`](../soh/soh/Network/Anchor/Anchor.cpp) queues incoming packets and processes stock effects on the game thread. `HandleRemoteJson` in [`Network.cpp`](../soh/soh/Network/Network.cpp) catches parse/dispatch exceptions, and `ProcessIncomingPacketQueue` catches handler exceptions. Individual handlers still perform heterogeneous field checks; these catches do not guarantee atomic validation of all fields before state assignment.

[`SetFlag`](../soh/soh/Network/Anchor/Packets/SetFlag.cpp), `UnsetFlag` and [`HookHandlers`](../soh/soh/Network/Anchor/HookHandlers.cpp) retain native category exclusions and actor follow-ups. For example, the Deku web hook calls native `BgYdanSp_BurnWeb` after the destroyed switch arrives. [`AnchorSceneSwitches`](../soh/soh/Network/Anchor/SceneSwitches.cpp) captures eligible saved banks, substitutes the current scene bank, and applies through native RawAction without producer/reward hooks. Ordered bank correctness does not promise every transient animation or resurrection of destroyed geometry.

The optional Diptych module transport is a separate compiled path. Host durable admission, seed/team/pair fences and metadata capability are not guarantees of standalone stock Anchor. Keep native behavior and host admission policy separate.

## Save and embedding boundaries

[`SaveManager`](../soh/soh/SaveManager.cpp) registers versioned sections and copies `SaveContext` before queued saving. Section loaders own their native migration meanings; generic code must not silently flatten those versions or promise arbitrary old randomizer builds are compatible. Unsupported Diptych sections have a specific refusal path. The generic native load path can quarantine malformed data, while manual mutex/filesystem handling still needs care around exceptions; failure-safe transactions are not universal across all sections/platforms.

Standalone builds require no Diptych checkout. Root [`CMakeLists.txt`](../CMakeLists.txt) defaults `DIPTYCH_ROOT` empty and rejects module ON without a valid explicit root/shared engine. Nonempty root intentionally includes external sources even with module OFF. For optional integration, read the single [Diptych build contract](https://github.com/chrismacdonaldw/diptych/blob/docs/runtime-contracts/docs/NATIVE_BUILDS.md) at the matching Diptych revision; older pins may not contain it. This fork's [build instructions](BUILDING.md) remain the standalone entrypoint.

The module SaveManager still includes private `host/native_save.h` and calls `Diptych_BeforeDeleteFile`. These are current reverse dependencies, not target APIs. Native publication and save/delete lifecycle should be native-owned; an optional manager observer/policy seam should retain paired backup, identity and deletion decisions outside this port. The shared runtime/module infrastructure is intentional and need not be removed to improve that ownership boundary.

Use focused registration/export, hook, section/version or packet-boundary checks for changed seams, plus the affected native consumer compile. Cross-game admission tests belong in Diptych. A source/object/module check does not establish full game execution or all platform behavior.
