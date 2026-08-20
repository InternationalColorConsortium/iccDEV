# wxProfileDump: open embedded profiles in a profile view

Date: 2026-08-11
Component: `Tools/wxWidget/wxProfileDump`

## Problem

Double-clicking any tag in a wxProfileDump tag list opens `MyTagDialog`, a
read-only text dump produced by `CIccTag::Describe`. For an `embeddedV5` tag
(`icSigEmbeddedProfileType`) that dump is a flattened rendering of a whole
nested profile, which is far less useful than the structured header + tag list
view the tool already provides for top-level profiles.

Double-clicking an embedded profile tag should instead open the embedded
profile in a normal profile window.

## Current structure

- `MyFrame::OpenFile` (`wxProfileDump.cpp:306`) does two things: reads a
  profile from a path via `OpenIccProfile`, then builds and shows a `MyChild`
  MDI window for it (menu bar, icon, file history, `Show(true)`).
- `MyChild::OnTagClicked` (`wxProfileDump.cpp:722`) unconditionally constructs
  `MyTagDialog` for the activated tag.
- `MyChild::~MyChild` (`wxProfileDump.cpp:666`) does `delete m_pIcc`, so a
  `MyChild` owns the profile it displays.
- The `Round Trip Report` and `Validate Profile` buttons (`wxProfileDump.cpp:500-503`)
  both operate on `m_profilePath`, not on `m_pIcc`: `MyDialog` calls
  `ValidateIccProfile(path)` and `AnalyzeRoundTrip` calls
  `CIccMinMaxEval::EvaluateProfile(path)`. Neither has an in-memory entry point
  in use here.

## Library constraints

Three facts about `IccProfLib` shape the design:

1. **Embedded profiles are lazily read.** When the outer profile is opened from
   a file, `CIccTagEmbeddedProfile::Read` (`IccProfLib/IccTagEmbedIcc.cpp:257`)
   takes the `pProfile->HasIO()` branch and `Attach`es the embedded profile to a
   `CIccEmbedIO` wrapping the parent's IO rather than reading its tags. The
   embedded profile is therefore not usable standalone until its tags have been
   pulled through that IO.

2. **`CIccProfile`'s copy constructor deep-copies only loaded tags.**
   `CIccProfile::CIccProfile(const CIccProfile&)` (`IccProfLib/IccProfile.cpp:130`)
   copies each entry in `m_TagVals` via `NewCopy()`, and sets `entry.pTag = NULL`
   for any tag table entry with no corresponding loaded tag. So the tags must be
   loaded before `NewCopy()`, or the copy comes back with an intact tag table
   and no tag data.

3. **`ReadAll()` is unbounded, and the depth guard does not cover it.**
   `CIccTagEmbeddedProfile::ReadAll()` (`IccProfLib/IccTagEmbedIcc.cpp:294`)
   recursively pulls the entire nested tree of embedded profiles. The depth
   guard (`EmbedDepthGuard`) lives in `CIccTagEmbeddedProfile::Read`
   (`IccProfLib/IccTagEmbedIcc.cpp:218`), not in `ReadAll`. In `LoadTag`
   (`IccProfLib/IccProfile.cpp:1431-1441`), `pTag->Read(...)` **returns** --
   destroying its guard -- before `pTag->ReadAll()` is called, so the guard's
   counter unwinds at every level while the C++ stack keeps growing. Measured:
   `ReadAll()` at nesting depth 9 returns `TRUE` even though
   `kMaxEmbeddedProfileDepth` is 8; depth 1000 also succeeds. The cost grows
   roughly as `O(depth^2.7)` with nothing bounding it: a 90 KB crafted profile
   (depth 250) freezes the GUI for 2.3s, depth 1000 (180 KB) for 95.7s, and
   depth 2000 (359 KB) for over ten minutes -- with no progress indicator and
   no cancel. `wxProfileDump` exists to inspect profiles that are not trusted,
   so this is reachable by design, not just a theoretical concern.

   `CIccProfile::FindTag` (`IccProfLib/IccProfile.cpp:436`) goes through
   `LoadTag` with `bReadAll=false`, so it loads **one level** and never enters
   the recursive descent. Its cost is flat in nesting depth (86ms at depth
   2000 in the same measurement), and the depth guard becomes irrelevant to
   this path rather than needing a fix.

## Design

### 1. Extract window creation from `MyFrame::OpenFile`

Add:

```cpp
void MyFrame::ShowProfile(CIccProfile *pIcc, const wxString &title, const wxString &profilePath);
```

`ShowProfile` contains everything currently in `OpenFile` from
`new MyChild(...)` (line 322) through `subframe->Show(true)` (line 355) — the
`MyChild` construction, icon, file menu, history menu wiring, menu bar, and
`Show`. It takes ownership of `pIcc` by handing it to the `MyChild`.

`OpenFile` retains the `OpenIccProfile` call, the failure message box, and
`m_history.AddFileToHistory`, then delegates to
`ShowProfile(pIcc, profileTitle, profilePath)`. Behavior for file-opened
profiles is unchanged.

### 2. Gate path-dependent buttons on a non-empty path

In the `MyChild` constructor, add the `Round Trip Report` and `Validate Profile`
buttons only when `!profilePath.IsEmpty()`. The existing `IsRoundTripable(pIcc)`
test remains as an additional condition on the round-trip button. This follows
the conditional-button pattern already in the constructor.

An embedded profile window is therefore created with an empty `m_profilePath`
and shows neither button. Validation and round-trip analysis are not available
for embedded profiles; both require a file on disk and reworking them to run
against an in-memory `CIccProfile` is out of scope.

### 3. Branch in `MyChild::OnTagClicked`

When the activated tag is an embedded profile, load it one level (per tag, via
`FindTag`, not `ReadAll()`) and open it as a profile instead of dumping it:

```
pTag = m_pIcc->FindTag(tagSig)
if pTag and pTag->GetType() == icSigEmbeddedProfileType:
    pEmbedTag = static_cast<CIccTagEmbeddedProfile*>(pTag)
    pInner = pEmbedTag->GetProfile()
    if pInner:
        sigs = [entry.TagInfo.sig for entry in pInner->m_Tags]   // collect first
        for sig in sigs:
            pInner->FindTag(sig)                                 // then load, one level

        nLoaded = count of pInner->m_Tags entries with pTag != NULL
        if nLoaded:
            pCopy = pInner->NewCopy()
            if pCopy:
                my_frame->ShowProfile(pCopy, GetTitle() + " [embedded]", wxEmptyString)
                return
// fall through to the existing MyTagDialog dump
```

Signatures are collected into a vector before any `FindTag` call, rather than
calling `FindTag` while iterating `m_Tags` directly, because `LoadTag` mutates
`m_Tags` entries in place.

`my_frame` is the existing file-scope `MyFrame*` in `wxProfileDump.cpp` that
`OpenFile` already uses as the `MyChild` parent, so no new plumbing from child
to parent frame is needed.

If the tag holds no profile, if the per-tag load leaves `nLoaded == 0`, or if
`NewCopy()` returns NULL, control falls through to the existing `MyTagDialog`.
A malformed embedded tag therefore remains inspectable as a text dump rather
than producing an error box.

**Accepted trade-off: a profile nested two levels deep shows the dump, not a
window.** Loading one level leaves any embeddedV5ProfileTag *nested inside*
`pInner` attached but unread. `NewCopy()` then copies that nested tag with no
loaded tags and `m_pAttachIO = NULL`, so there is nothing to populate a second
window from -- `nLoaded` for that nested profile is 0. Rather than open an
empty window, `OnTagClicked` falls through to `MyTagDialog` for that case.
`CIccTagEmbeddedProfile::Describe` (`IccProfLib/IccTagEmbedIcc.cpp:372`) reads
`m_pProfile->m_Header` and the tag table directly and does not need loaded
tags, so the dump still works. The user experience is: the first level opens a
real profile window; deeper levels show the dump they would have gotten before
this feature existed. Nothing regresses and no empty window is ever shown.
Re-plumbing window ownership so a deeper level can open its own window is a
separate decision, out of scope here.

Requires including `IccTagEmbedIcc.h` in `wxProfileDump.cpp`.

### 4. Ownership and lifetime

The new window displays a deep copy, not the tag's profile. Consequences:

- The copy is owned outright by the new `MyChild` and released by its existing
  `delete m_pIcc`. No double free with the tag's own `delete m_pProfile`.
- The parent window may be closed while the embedded view is open. The copy has
  no dependency on the parent profile or its IO.
- A nested embedded profile *one level down* opens a window the same way,
  through the same `MyChild::OnTagClicked` handler. A profile nested *two*
  levels down does not: see the accepted trade-off in §3. It is reached
  through an already-copied profile with no IO, so nothing loads for it, and
  it falls through to the tag dump instead.

The cost is one profile's worth of duplicated memory per opened embedded view,
which is negligible for this tool.

## Testing

wxProfileDump itself has no automated GUI test harness, so the window-opening
behavior is still verified manually. The library-level invariants the GUI
relies on -- that a copy taken before any load carries an intact tag table
with unloaded (`pTag == NULL`) entries, that the one-level `FindTag` load
populates a copy, that the copy is independent of its parent, that the load
never descends past one level, and that the `nLoaded == 0` condition driving
the second-level dump fallback actually occurs -- are covered by a headless
regression test,
`.github/ci/regression/embedded-profile-onelevel-load.cpp`, registered in
`Build/Cmake/Testing/CMakeLists.txt` as `iccdev.embedded-profile-onelevel-load`.

Manual verification:

1. Open a profile containing an `embeddedV5` tag. Double-click that tag.
   The header fields and tag list of the embedded profile populate, and the
   `Validate Profile` and `Round Trip Report` buttons are absent.
2. Double-click a non-embedded tag in the same window and confirm the
   `MyTagDialog` text dump still appears.
3. Close the parent profile window while the embedded window is open, then
   interact with and close the embedded window. No crash.
4. Open a plain (non-embedded) profile from a file and confirm the header, tag
   list, `Validate Profile`, and `Round Trip Report` behave as before.
5. Double-click a tag row inside the embedded window that itself holds a
   further-nested `embeddedV5` tag. Confirm the text dump appears (not a
   window with an empty tag list), and that the dump's content is a
   reasonable rendering of that nested profile's header and tag table.

## Out of scope

- Making `Validate Profile` or `Round Trip Report` operate on an in-memory
  `CIccProfile`.
- Any indication in the tag list that a row is drillable.
- Saving an embedded profile out to a file.
