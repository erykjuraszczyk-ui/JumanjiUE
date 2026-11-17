# JUMANJI Traversal Presentation — Stage 24

## Runtime assets

Import `Data/Traversal/TraversalPresentationManifest.csv` as a DataTable with row struct:

`FJumanjiTraversalPresentationDefinition`

Expected asset name:

`DT_TraversalPresentation`

Place it at:

`/Game/Traversal/DT_TraversalPresentation`

Configure the soft-object path in `Config/DefaultGame.ini` under:

`[/Script/JumanjiUE.JumanjiTraversalPresentationSubsystem]`

The row name is the stable `TraversalId` from the placed `AJumanjiTraversalLink`. Keep IDs stable across map revisions. A missing row uses the built-in Polish prompt for its traversal type. A row with the wrong type is rejected rather than being applied to a different traversal.

## Presentation fields

Each row may reference:
- PromptIcon;
- StartMontage, LoopMontage and EndMontage;
- StartSound and EndSound;
- StartEffect and EndEffect (Niagara).

These are soft references. The Widget Blueprint/Character Blueprint should request assets asynchronously and keep handles alive while they are in use. Do not synchronously load a montage, sound or Niagara system at the moment the player enters a traversal.

The code provides labels and presentation metadata. It does not invent final animation assets. Add only montages authored for the project's actual skeleton and retargeted animation set.

## Context prompt Widget Blueprint

Create `WBP_TraversalPrompt` as a child of `UJumanjiTraversalPromptWidget`.

Suggested mobile composition:
- compact icon and traversal name;
- large primary interaction button;
- status text;
- optional progress ring/bar;
- safe-area padding;
- no modal full-screen overlay.

Set `TraversalPromptWidgetClass` on the project PlayerController Blueprint to `WBP_TraversalPrompt`.

The Widget Blueprint receives `OnPromptChanged`. Bind the view model to:
- icon and display name;
- visibility and request enabled state;
- status text;
- optional progress indicator.

Connect the primary button's OnPressed to `RequestFocusedTraversal`. Do not call the server RPC directly from the widget; the C++ request path keeps client and server validation consistent.

## How a traversal becomes focused

The map/character Blueprint must detect the currently usable traversal link using a short-range overlap or camera trace. When one valid link is in focus, call `SetTraversalFocus` on the local `AJumanjiPlayerController`. When focus is lost, call `ClearTraversalFocus`.

Do not scan every traversal actor every frame. Prefer overlap events or a throttled trace (for example 4–8 times per second) and choose the nearest usable link. A link is usable only when `CanBeUsedBy` succeeds and the traversal component is not already active.

## Traversal animation sequence

Use `AJumanjiTraversalLink::GetTraversalMontage` as a per-instance legacy override when appropriate; otherwise use the DataTable row.

Recommended presentation phases:
1. Request accepted: start montage / attach cue.
2. Traversal in progress: loop or root-motion-disabled presentation while the server owns movement.
3. Traversal completed: end montage, sound and Niagara effect.
4. Traversal interrupted: stop/transition montage and show the interrupted prompt.

The authoritative state change is replicated through `UJumanjiTraversalComponent::OnTraversalStateChanged`. The pre-existing `OnTraversalPresentationChanged` Blueprint event remains available on the component. Never play a success animation only because the player pressed the button; wait for the server-confirmed active/completed state.

## Zipline spline authoring

Each `AJumanjiTraversalLink` now contains an optional `TraversalSpline` component.

For a Zipline link:
1. Set `bUseSplinePath` true.
2. Edit the spline points so the first point matches EntryAnchor and the last point matches ExitAnchor.
3. Keep the spline clear of landscape, trees, buildings and traversal blockers.
4. Ensure the spline length and the link's TraversalDuration produce a comfortable speed.
5. Test the same link from both ends when bidirectional.
6. The server samples the spline and moves the character with collision sweep.

If the spline is missing, has fewer than two points or has invalid length, the runtime falls back to the existing straight interpolation between anchors. The spline is for the character's traversed path; it does not create a physical cable mesh. Use a Spline Mesh Component in the link Blueprint for cable visuals.

## Lift presentation

The current Lift runtime interpolates the character between its two anchors. It does not yet simulate a moving platform, platform collision, elevator doors or synchronized multi-rider physics.

For a first playable greybox:
- author the platform mesh and visible rails in the Blueprint child;
- keep EntryAnchor and ExitAnchor at capsule-center height;
- place the platform collision at the safe landing surface;
- use start/end montage and cues for entering/exiting;
- do not move the platform independently until a dedicated replicated lift-platform actor exists.

## Required editor assets

- DT_TraversalPresentation DataTable imported from the CSV;
- WBP_TraversalPrompt;
- BP_TraversalLink_Base and per-type children;
- authored Start/Loop/End montages for the target skeleton;
- prompt icons, sounds and Niagara effects;
- spline mesh cable visual for zipline;
- mesh and collision for lift platform.

The text-only repository workflow does not create binary `.uasset` files or final animation content.


## Stage 24 — Traversal animation and interaction presentation

- Import `Data/Traversal/TraversalPresentationManifest.csv` as `DT_TraversalPresentation`.
- Row struct: `FJumanjiTraversalPresentationDefinition`.
- Runtime lookup: `UJumanjiTraversalPresentationSubsystem`; missing rows use built-in Polish copy for the traversal type.
- Prompt base: `UJumanjiTraversalPromptWidget`; create `WBP_TraversalPrompt` as its child.
- Assign `WBP_TraversalPrompt` to `TraversalPromptWidgetClass` in the PlayerController Blueprint.
- Call `SetTraversalFocus` when a nearby usable link is focused and `ClearTraversalFocus` when focus is lost.
- Bind the widget's `OnPromptChanged` view model to the prompt visuals and call `RequestFocusedTraversal` from the primary button.
- Traversal state now publishes `OnTraversalStateChanged`, including the server-confirmed completion/interruption result.
- Zipline links may enable `bUseSplinePath`; the server samples the authored `TraversalSpline` with collision sweep.

Validate the source manifest before importing:

```bash
python Scripts/validate_traversal_presentation.py
python -m unittest discover -s Scripts/Tests -p "test_validate_traversal_presentation.py" -v
```

The validator checks CSV consistency only. Montages, icons, sounds, Niagara effects, Widget Blueprints, spline meshes, link placements and a moving lift platform still need Unreal Editor assets and PIE validation.


## Stage 25 — World integration and replicated lift platform

The native overlap focus volume on each AJumanjiTraversalLink registers the link with the local PlayerController. The controller chooses the nearest usable candidate and forwards it to the existing UJumanjiTraversalPromptWidget. No full-world traversal actor scan runs each frame.

Create AJumanjiLiftPlatform Blueprint children as BP_JumanjiLiftPlatform:
- assign a platform Static Mesh;
- adjust PlatformCollision (swept blocking root) and BoardingVolume (query-only);
- position LowerStopAnchor and UpperStopAnchor;
- place the actor at the lower stop;
- assign TravelDurationSeconds and MaxPassengers;
- assign the actor reference to the matching Lift link's LiftPlatform field.

The platform moves on the server with swept movement and replicates its actor transform. Characters attach to the platform while riding. The default passenger limit is four; additional riders may join only during the boarding window and in the same direction. A blocking hit moves the platform to Blocked and releases passengers at the last safe position.

Stage 25 source manifests:
- Data/Traversal/EmeraldJungle_TraversalPlacement.csv — 12 starter links.
- Data/Traversal/EmeraldJungle_LiftPlatformManifest.csv — two lift platforms.
- Data/Traversal/EmeraldJungle_TraversalManifest.json — expected counts, Data Layers and authoring rules.

Coordinates are greybox placeholders. Reposition all anchors against the final landscape, check capsule clearance and rebuild World Partition Navigation Data.

Run source validation:
    python Scripts/validate_emerald_jungle_traversal.py
    python -m unittest discover -s Scripts/Tests -p "test_validate_emerald_jungle_traversal.py" -v

Validation only checks source CSV/JSON. It does not compile Unreal C++, inspect .umap/.uasset, build NavMesh or prove multiplayer/device behavior. Follow Docs/STAGE_25_TRAVERSAL_WORLD_INTEGRATION_MULTIPLAYER.md for the editor and two-client acceptance tests.
