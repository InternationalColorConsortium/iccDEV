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

Two facts about `IccProfLib` shape the design:

1. **Embedded profiles are lazily read.** When the outer profile is opened from
   a file, `CIccTagEmbeddedProfile::Read` (`IccProfLib/IccTagEmbedIcc.cpp:257`)
   takes the `pProfile->HasIO()` branch and `Attach`es the embedded profile to a
   `CIccEmbedIO` wrapping the parent's IO rather than reading its tags. The
   embedded profile is therefore not usable standalone until
   `CIccTagEmbeddedProfile::ReadAll()` has pulled its tags through that IO.

2. **`CIccProfile`'s copy constructor deep-copies only loaded tags.**
   `CIccProfile::CIccProfile(const CIccProfile&)` (`IccProfLib/IccProfile.cpp:130`)
   copies each entry in `m_TagVals` via `NewCopy()`, and sets `entry.pTag = NULL`
   for any tag table entry with no corresponding loaded tag. So `ReadAll()` must
   precede `NewCopy()`, or the copy comes back with an intact tag table and no
   tag data.

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

When the activated tag is an embedded profile, open it as a profile instead of
dumping it:

```
pTag = m_pIcc->FindTag(tagSig)
if pTag and pTag->GetType() == icSigEmbeddedProfileType:
    pEmbedTag = static_cast<CIccTagEmbeddedProfile*>(pTag)
    pEmbedTag->ReadAll()
    if pEmbedTag->GetProfile():
        pCopy = pEmbedTag->GetProfile()->NewCopy()
        if pCopy:
            my_frame->ShowProfile(pCopy, GetTitle() + " [embedded]", wxEmptyString)
            return
// fall through to the existing MyTagDialog dump
```

`my_frame` is the existing file-scope `MyFrame*` in `wxProfileDump.cpp` that
`OpenFile` already uses as the `MyChild` parent, so no new plumbing from child
to parent frame is needed.

If the tag holds no profile, if `ReadAll()` fails, or if `NewCopy()` returns
NULL, control falls through to the existing `MyTagDialog`. A malformed embedded
tag therefore remains inspectable as a text dump rather than producing an error
box.

Requires including `IccTagEmbedIcc.h` in `wxProfileDump.cpp`.

### 4. Ownership and lifetime

The new window displays a deep copy, not the tag's profile. Consequences:

- The copy is owned outright by the new `MyChild` and released by its existing
  `delete m_pIcc`. No double free with the tag's own `delete m_pProfile`.
- The parent window may be closed while the embedded view is open. The copy has
  no dependency on the parent profile or its IO.
- Nested embedded profiles work without further change: the new window's tag
  list uses the same `MyChild::OnTagClicked` handler.

The cost is one profile's worth of duplicated memory per opened embedded view,
which is negligible for this tool.

## Testing

wxProfileDump has no automated test harness, so verification is manual:

1. Open a profile containing an `embeddedV5` tag. Double-click that tag.
   The header fields and tag list of the embedded profile populate, and the
   `Validate Profile` and `Round Trip Report` buttons are absent.
2. Double-click a non-embedded tag in the same window and confirm the
   `MyTagDialog` text dump still appears.
3. Close the parent profile window while the embedded window is open, then
   interact with and close the embedded window. No crash.
4. Open a plain (non-embedded) profile from a file and confirm the header, tag
   list, `Validate Profile`, and `Round Trip Report` behave as before.

## Out of scope

- Making `Validate Profile` or `Round Trip Report` operate on an in-memory
  `CIccProfile`.
- Any indication in the tag list that a row is drillable.
- Saving an embedded profile out to a file.
