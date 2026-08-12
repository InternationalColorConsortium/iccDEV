# wxProfileDump Embedded Profile View Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make double-clicking an `embeddedV5ProfileTag` in wxProfileDump open the embedded profile in a normal profile window instead of a text dump of the tag.

**Architecture:** Split the window-creation half of `MyFrame::OpenFile` into a reusable `MyFrame::ShowProfile`, hide the two file-path-dependent buttons when a window has no path, and branch in `MyChild::OnTagClicked` on `icSigEmbeddedProfileType` to load the embedded profile and hand a copy to `ShowProfile`. Task 3 first did this with `ReadAll()` + `NewCopy()`; Task 4 replaced `ReadAll()` with a one-level per-tag `FindTag` load after measurement showed `ReadAll()` was unbounded in nesting depth (see Task 4).

**Tech Stack:** C++, wxWidgets 3.3 (MSW), IccProfLib, CMake + MSVC (multi-config).

**Spec:** [docs/superpowers/specs/2026-08-11-wxprofiledump-embedded-profile-view-design.md](../specs/2026-08-11-wxprofiledump-embedded-profile-view-design.md)

## Global Constraints

- `IccProfLib` is not modified by this work. The wxProfileDump-side changes
  (Task 3, then Task 4) are deliberately scoped to stop `wxProfileDump` from
  reaching the unbounded `ReadAll()` path rather than fixing the depth guard
  gap inside `IccProfLib` itself; that gap is left open on purpose.
- **wxProfileDump's window-opening behavior has no automated GUI test
  harness.** The CMake target `iccDumpProfileGui` builds a `WIN32_EXECUTABLE`
  GUI app with no test registration in `Build/Cmake/Testing/CMakeLists.txt`,
  and there is no way to write a failing unit test first for the window
  itself. Task 3 and Task 4 are therefore verified by (a) a clean compile and
  (b) explicit manual GUI steps. This does not mean no automated coverage is
  possible at all: a headless regression test in `.github/ci/regression/`
  that exercises the `IccProfLib` invariants those GUI changes depend on
  (tag-loading order, copy independence, one-level-only loading) is in scope
  and is what Task 4 adds.
- The plan's manual-GUI-check steps below (Task 1 Step 4, Task 2 Step 3, Task
  3 Steps 4-5) reference `Testing/hybrid/BeyondRGB.icc`. That file is
  untracked and present only on the author's machine -- it is not a repo
  fixture anyone else can rely on to reproduce those steps.
- Build command (from repo root), used unchanged in every task:
  `cmake --build out/vs2022-x64 --target iccDumpProfileGui --config Release`
  Binary lands at `out/vs2022-x64/bin/Release/iccDumpProfileGui.exe`.
- Automated regression fixture: `Testing/hybrid/ICC/CMYK_Hybrid_Profile.icc`.
  This is generated (not checked in) by `Testing/hybrid/BuildAndTest.sh` (via
  `iccFromXml`) from the tracked `Testing/hybrid/CMYK_Hybrid_Profile.xml`.
  Confirmed via `iccDumpProfile.exe` to carry one `embeddedV5ProfileTag 'ICC5'`
  at offset 2684968, size 1004012. It is absent on CI legs that skip the
  hybrid step (macOS runs "CreateAllProfiles only", Windows has no hybrid
  step, and the Linux hybrid step tolerates its own failure), so its absence
  is not itself a regression -- a test built against it must skip, not fail,
  when it is simply missing.
- Match the surrounding file's existing style: the file mixes tab and
  space indentation; follow whatever the lines immediately adjacent to your
  edit already use rather than reformatting them.
- Keep `MyFrame::OpenFile`'s behavior for file-opened profiles byte-for-byte
  identical. Task 1 is a pure refactor.

---

### Task 1: Extract `MyFrame::ShowProfile` from `MyFrame::OpenFile`

Pure refactor. `OpenFile` currently does two unrelated jobs — read a profile
from a path, and build/show a `MyChild` MDI window for it. Task 3 needs the
second job on its own.

**Files:**
- Modify: `Tools/wxWidget/wxProfileDump/wxProfileDump.h:105` (declare the new method)
- Modify: `Tools/wxWidget/wxProfileDump/wxProfileDump.cpp:306-356` (split the function)
- Test: none — see Global Constraints

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces: `void MyFrame::ShowProfile(CIccProfile *pIcc, const wxString &title, const wxString &profilePath)`.
  Takes ownership of `pIcc` by handing it to the `MyChild` it constructs (that
  `MyChild` frees it in its destructor). Task 3 calls this. `wxProfileDump.h`
  is included at `wxProfileDump.cpp:113`, well after `IccProfile.h` at line 71,
  so `CIccProfile` is a complete type in the header — no forward declaration
  needed.

- [ ] **Step 1: Declare `ShowProfile` in the `MyFrame` class**

In `Tools/wxWidget/wxProfileDump/wxProfileDump.h`, in `class MyFrame`, change:

```cpp
    void OpenFile(wxString path);
```

to:

```cpp
    void OpenFile(wxString path);
    void ShowProfile(CIccProfile *pIcc, const wxString &title, const wxString &profilePath);
```

- [ ] **Step 2: Split the function body**

In `Tools/wxWidget/wxProfileDump/wxProfileDump.cpp`, replace the whole of
`MyFrame::OpenFile` (lines 306-356, from `void MyFrame::OpenFile(wxString profilePath)`
through its closing brace) with these two functions:

```cpp
void MyFrame::OpenFile(wxString profilePath)
{
  wxFileName filepath(profilePath);
  wxString profileTitle = filepath.GetName();

  std::string path = profilePath.ToStdString(wxConvUTF8);
  CIccProfile *pIcc = OpenIccProfile(path.c_str());

  if (!pIcc) {
    (void)wxMessageBox(wxString(_T("Unable to open profile '")) + profilePath + _T("'"),
                       _T("Open Error!"));
    return;
  }

  wxGetApp().m_history.AddFileToHistory(profilePath);

  ShowProfile(pIcc, profileTitle, profilePath);
}

// Builds and shows an MDI window for an already-loaded profile.  Takes
// ownership of pIcc.  A profile that did not come from a file on disk (an
// embedded profile, for example) passes wxEmptyString as profilePath.
void MyFrame::ShowProfile(CIccProfile *pIcc, const wxString &title, const wxString &profilePath)
{
  // Make another frame, containing a canvas
  MyChild *subframe = new MyChild(my_frame, title, pIcc, profilePath);

  subframe->SetTitle(title);

  // Give it an icon
#ifdef __WXMSW__
  subframe->SetIcon(wxIcon(_T("ProfileDumpDoc_icn")));
#else
//  subframe->SetIcon(wxIcon( mondrian_xpm ));
#endif

  // Make a menubar
  wxMenu *file_menu = new wxMenu;
  subframe->SetFileMenu(file_menu);

  file_menu->Append(MDI_OPEN_PROFILE, _T("&Open Profile"));
  file_menu->Append(MDI_CHILD_QUIT, _T("&Close"), _T("Close this window"));
  file_menu->Append(MDI_QUIT, _T("&Exit"));

  wxGetApp().m_history.UseMenu(file_menu);
  wxGetApp().m_history.AddFilesToMenu(file_menu);

  wxMenu *help_menu = new wxMenu;
  help_menu->Append(MDI_ABOUT, _T("&About"));

  wxMenuBar *menu_bar = new wxMenuBar;

  menu_bar->Append(file_menu, _T("&File"));
  menu_bar->Append(help_menu, _T("&Help"));

  // Associate the menu bar with the frame
  subframe->SetMenuBar(menu_bar);
  subframe->Show(true);
}
```

The only substantive difference from the original body is that the local
`profileTitle` is now the `title` parameter. Everything else is verbatim.

- [ ] **Step 3: Build**

Run from the repo root:

```bash
cmake --build out/vs2022-x64 --target iccDumpProfileGui --config Release
```

Expected: compiles and links with no new warnings or errors.

- [ ] **Step 4: Manual check — no behavior change**

Run `out/vs2022-x64/bin/Release/iccDumpProfileGui.exe`, open
`Testing/hybrid/BeyondRGB.icc` via File > Open Profile.

Expected, exactly as before the change:
- A child window titled `BeyondRGB` appears with the profile header fields
  populated and the tag list filled in (it includes a row
  `embeddedV5ProfileTag`).
- The `Validate Profile` button is present.
- The File menu lists `Open Profile`, `Close`, `Exit`, plus the recent-file
  history, and `BeyondRGB.icc` has been added to that history.
- Double-clicking any tag row still opens the text dump dialog.

- [ ] **Step 5: Commit**

```bash
git add Tools/wxWidget/wxProfileDump/wxProfileDump.cpp Tools/wxWidget/wxProfileDump/wxProfileDump.h
git commit -m "refactor(wxProfileDump): extract MyFrame::ShowProfile from OpenFile

Separates reading a profile from a path from building the MDI window that
displays it, so a profile that did not come from a file can be shown."
```

---

### Task 2: Hide Validate and Round Trip when there is no file path

`MyDialog` calls `ValidateIccProfile(path)` and `AnalyzeRoundTrip` calls
`CIccMinMaxEval::EvaluateProfile(path)` — both need a file on disk, not the
in-memory `CIccProfile`. A window opened without a path must not offer them.

**Files:**
- Modify: `Tools/wxWidget/wxProfileDump/wxProfileDump.cpp:499-505` (button creation in the `MyChild` constructor)
- Test: none — see Global Constraints

**Interfaces:**
- Consumes: nothing from Task 1 (this edit is independent of it).
- Produces: the invariant that a `MyChild` built with an empty `profilePath`
  shows neither `ID_ROUND_TRIP` nor `ID_VALIDATE_PROFILE`. Task 3 relies on it.

- [ ] **Step 1: Wrap the two buttons in a path check**

In the `MyChild` constructor in `Tools/wxWidget/wxProfileDump/wxProfileDump.cpp`,
replace:

```cpp
	wxSizer *sizerBtn = new wxBoxSizer(wxHORIZONTAL);
    if (IsRoundTripable(pIcc)) {
  		sizerBtn->Add(new wxButton(m_panel, ID_ROUND_TRIP, _("&Round Trip Report")), wxSizerFlags().Border(wxRIGHT, 5));
    }
    sizerBtn->Add(new wxButton(m_panel, ID_VALIDATE_PROFILE, _("&Validate Profile")), wxSizerFlags().Border(wxRIGHT, 5));
```

with:

```cpp
	wxSizer *sizerBtn = new wxBoxSizer(wxHORIZONTAL);
    // Both reports re-read the profile from disk by path rather than using
    // m_pIcc, so neither is available for a profile shown without a path.
    if (!profilePath.IsEmpty()) {
        if (IsRoundTripable(pIcc)) {
  		    sizerBtn->Add(new wxButton(m_panel, ID_ROUND_TRIP, _("&Round Trip Report")), wxSizerFlags().Border(wxRIGHT, 5));
        }
        sizerBtn->Add(new wxButton(m_panel, ID_VALIDATE_PROFILE, _("&Validate Profile")), wxSizerFlags().Border(wxRIGHT, 5));
    }
```

`profilePath` is the constructor parameter, already in scope at this point (it
is assigned to `m_profilePath` near the top of the constructor). Leave the
`sizerTop->Add(sizerBtn, ...)` line that follows untouched — an empty sizer
adds no visible row.

- [ ] **Step 2: Build**

Run from the repo root:

```bash
cmake --build out/vs2022-x64 --target iccDumpProfileGui --config Release
```

Expected: compiles and links with no new warnings or errors.

- [ ] **Step 3: Manual check — file-opened profiles are unaffected**

Run `out/vs2022-x64/bin/Release/iccDumpProfileGui.exe` and open
`Testing/hybrid/BeyondRGB.icc`.

Expected: the `Validate Profile` button is still present (the path is
non-empty), and clicking it still produces the validation report dialog. The
`Round Trip Report` button appears or not exactly as it did before, governed by
`IsRoundTripable`.

There is no way to exercise the empty-path branch until Task 3 lands; Task 3's
manual check covers it.

- [ ] **Step 4: Commit**

```bash
git add Tools/wxWidget/wxProfileDump/wxProfileDump.cpp
git commit -m "feat(wxProfileDump): hide Validate and Round Trip without a file path

Both reports re-read the profile from disk by path, so they cannot work for a
window whose profile has no path."
```

---

### Task 3: Open embedded profiles in a profile window

**Files:**
- Modify: `Tools/wxWidget/wxProfileDump/wxProfileDump.cpp:72` (add an include)
- Modify: `Tools/wxWidget/wxProfileDump/wxProfileDump.cpp:722-730` (`MyChild::OnTagClicked`)
- Test: none — see Global Constraints

**Interfaces:**
- Consumes: `MyFrame::ShowProfile(CIccProfile*, const wxString&, const wxString&)`
  from Task 1, and the empty-path invariant from Task 2.
- Produces: no new interface.

Library facts this task depends on, all verified against the current tree:
- `icSigEmbeddedProfileType` is the tag *type* (`'ICCp'`,
  `IccProfLib/icProfileHeader.h:597`); `icSigEmbeddedV5ProfileTag` is the tag
  *signature* (`'ICC5'`, line 493). Branch on the type via `GetType()`, since
  that is what identifies a `CIccTagEmbeddedProfile` regardless of which
  signature it was stored under.
- `CIccTagEmbeddedProfile` is declared in `IccProfLib/IccTagEmbedIcc.h`, which
  `wxProfileDump.cpp` does not currently include.
- `CIccTagEmbeddedProfile::Read` (`IccProfLib/IccTagEmbedIcc.cpp:257`) `Attach`es
  rather than reads when the parent profile has IO, so the embedded profile's
  tags are unread until something pulls them through.
- `CIccProfile::NewCopy()` (`IccProfLib/IccProfile.h:152`) forwards to the copy
  constructor at `IccProfLib/IccProfile.cpp:130`, which deep-copies every
  *loaded* tag and sets `entry.pTag = NULL` for tag table entries with no loaded
  tag. Hence the tags must be loaded before `NewCopy()`.
- **Load one level with `FindTag`, not the whole tree with `ReadAll()`.**
  `CIccTagEmbeddedProfile::ReadAll()` recursively pulls every nested embedded
  profile, and the depth guard in `CIccTagEmbeddedProfile::Read`
  (`IccProfLib/IccTagEmbedIcc.cpp:218`) does not bound that path: `LoadTag`
  (`IccProfLib/IccProfile.cpp:1431-1441`) lets `Read()` return -- destroying
  its guard -- before calling `ReadAll()`, so the guard unwinds at every level
  while the C++ stack keeps growing. Measured cost is roughly `O(depth^2.7)`:
  a 90 KB crafted profile (depth 250) freezes the GUI for 2.3s, and a 359 KB
  one (depth 2000) for over ten minutes, with no progress indicator and no
  cancel -- reachable by design, since this tool exists to inspect untrusted
  profiles. `CIccProfile::FindTag` (`IccProfLib/IccProfile.cpp:436`) goes
  through `LoadTag` with `bReadAll=false` instead, loading one level and never
  entering the recursive descent; cost is flat in depth (86ms at depth 2000).
  **This task's own commit (below) still uses `ReadAll()`; Task 4 replaces it
  with the one-level `FindTag` load described here after this cost was
  measured post-hoc. See Task 4 for the actual code and the accepted
  trade-off (a profile nested two levels deep falls back to the tag dump
  instead of opening a second window).**
- `my_frame` is the file-scope `MyFrame*` at `wxProfileDump.cpp:129`, already
  used as the `MyChild` parent inside `ShowProfile`.

- [ ] **Step 1: Include the embedded profile tag header**

In `Tools/wxWidget/wxProfileDump/wxProfileDump.cpp`, after the existing
`#include "IccTag.h"` (line 72), add:

```cpp
#include "IccTagEmbedIcc.h"
```

- [ ] **Step 2: Branch in `OnTagClicked`**

Replace the whole of `MyChild::OnTagClicked` (lines 722-730) with:

```cpp
void MyChild::OnTagClicked(wxListEvent& event)
{
    icTagSignature tagSig = (icTagSignature)event.GetData();
	CIccTag *pTag = m_pIcc->FindTag(tagSig);

    // An embedded profile is far more useful shown as a profile than as a flat
    // text dump of the tag, so give it a window of its own.
    if (pTag && pTag->GetType() == icSigEmbeddedProfileType) {
        CIccTagEmbeddedProfile *pEmbed = (CIccTagEmbeddedProfile*)pTag;

        // When the outer profile came from a file the embedded profile is only
        // attached to that file's IO, so its tags have to be pulled in before
        // the copy below - a copy only carries tags that are already loaded.
        if (pEmbed->ReadAll() && pEmbed->GetProfile()) {
            // The new window owns and frees whatever profile it is given, and
            // the tag owns its own, so hand over an independent deep copy.
            // This also lets this window be closed while that one stays open.
            CIccProfile *pCopy = pEmbed->GetProfile()->NewCopy();

            if (pCopy) {
                my_frame->ShowProfile(pCopy, GetTitle() + _T(" [embedded]"), wxEmptyString);
                return;
            }
        }
        // Anything went wrong - fall through to the tag dump, which at least
        // shows what is there.
    }

    MyTagDialog dialog(this, m_pIcc, tagSig, pTag);

    dialog.ShowModal();
}
```

- [ ] **Step 3: Build**

Run from the repo root:

```bash
cmake --build out/vs2022-x64 --target iccDumpProfileGui --config Release
```

Expected: compiles and links with no new warnings or errors.

- [ ] **Step 4: Manual check — the embedded profile opens as a profile**

Run `out/vs2022-x64/bin/Release/iccDumpProfileGui.exe` and open
`Testing/hybrid/BeyondRGB.icc`. Double-click the `embeddedV5ProfileTag` row.

Expected:
- A new MDI window titled `BeyondRGB [embedded]` appears.
- Its header fields are populated (not blank) and differ from the outer
  profile's — the embedded profile is the V5 one.
- Its tag list is non-empty and its rows have real Tag ID, Tag Type, Offset and
  Size values.
- Neither `Validate Profile` nor `Round Trip Report` is present in that window.

- [ ] **Step 5: Manual check — other tags still dump, and lifetimes hold**

With the same build:

1. In the outer `BeyondRGB` window, double-click a non-embedded tag row (for
   example `profileDescriptionTag`). Expected: the usual `MyTagDialog` text
   dump appears, unchanged.
2. In the embedded window, double-click one of its tag rows. Expected: a text
   dump of that tag appears (or, if that profile nests another embedded
   profile, a further profile window). **Superseded by Task 4**: with the
   one-level load, a profile nested two levels deep no longer opens a window
   here -- it shows the tag dump instead. See Task 4's accepted trade-off.
3. With the embedded window still open, close the outer `BeyondRGB` window
   first. Then scroll the embedded window's tag list, double-click a tag in it,
   close the dump, and close the embedded window. Expected: no crash and no
   blank or garbage field values — the copy is independent of the parent.
4. Reopen `Testing/hybrid/BeyondRGB.icc` from the File menu's recent-file list
   and confirm it still opens normally.

- [ ] **Step 6: Commit**

```bash
git add Tools/wxWidget/wxProfileDump/wxProfileDump.cpp
git commit -m "feat(wxProfileDump): open embedded profiles in a profile view

Double-clicking an embeddedV5ProfileTag opened a flat text dump of the whole
nested profile.  It now opens that profile in its own window with the usual
header and tag list, so it can be browsed and drilled into like any other.

The window is handed a deep copy taken after ReadAll(), so it owns its profile
outright and is unaffected by the parent window closing.  A tag that holds no
readable profile still falls back to the text dump."
```

---

## Self-Review

**Spec coverage:**
- Spec §1 "Extract window creation from `MyFrame::OpenFile`" → Task 1.
- Spec §2 "Gate path-dependent buttons on a non-empty path" → Task 2.
- Spec §3 "Branch in `MyChild::OnTagClicked`" → Task 3, Steps 1-2.
- Spec §4 "Ownership and lifetime" → Task 3, Step 2 (the `NewCopy` call) and
  Step 5 item 3 (the close-parent-first check).
- Spec "Testing" items 1-4 → Task 3 Step 4, Task 3 Step 5 item 1, Task 3 Step 5
  item 3, and Task 1 Step 4 / Task 2 Step 3 respectively.
- Spec "Out of scope" → nothing in the plan touches validation internals, tag
  list rendering, or profile saving.

**Placeholder scan:** No TBD/TODO, no "add error handling", no "similar to Task
N". Every code step carries the literal code.

**Type consistency:** `ShowProfile` is declared in Task 1 Step 1 and defined in
Task 1 Step 2 with the same signature, and called in Task 3 Step 2 with three
arguments matching that signature. `ReadAll`, `GetProfile`, `NewCopy`,
`GetType` and `icSigEmbeddedProfileType` are all verified against the headers
cited in Task 3's Interfaces block.
