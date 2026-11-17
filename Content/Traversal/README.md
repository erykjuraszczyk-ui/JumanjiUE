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
