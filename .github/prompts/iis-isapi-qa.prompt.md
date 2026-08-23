# IIS ISAPI Endpoint QA

Use this prompt to validate or diagnose the Windows `iccIisIsapi` HTTP and
browser assessment surface.

## Inputs

- Branch or worktree:
- Build and install directories:
- IIS site name and isolated port:
- Sample ICC profile:
- Behavior or failure to prove:

## Procedure

1. Confirm the worktree and build identify the target commit.
2. Read `Tools/Winnt/IccIisIsapi/isapi-instructions.md` and `api.md`.
3. Use PowerShell 7 or later (`pwsh`) for stress validation.
4. Build the complete `install` target, not only the IIS DLL.
5. Export the site package, then import it with
   `Import-IccIisIsapiSite.ps1`. Verify package contents in clean staging
   before deployment; missing web files or wrapped executables, including
   `iccPawgReport.exe`, must fail without modifying the destination.
6. Repeat the incomplete-package check against a destination containing stale
   tools to prove stale files cannot satisfy validation.
7. Run `iccIisIsapiSmoke.exe`.
8. Install an isolated IIS site with `Install-IccIisIsapiSite.ps1`.
9. Run `Test-IccIisIsapiEndpoints.ps1`.
10. In a browser, exercise summary, health, XML, profile upload, PAWG report
   display, report download, and workspace navigation.
11. Inspect browser console and network errors.
12. Verify invalid input kinds return HTTP 400, oversized bodies are rejected,
   unsafe filenames are forced to `.icc`/`.xml`, and unsupported methods
   return 405.
13. Verify missing request headers return 403, wrong media types return 415,
    cross-site requests return 403, XML `DOCTYPE` returns 400, workspace IDs
    contain 128 cryptographically random bits, the workspace root returns 403
    or 404, artifact links do not duplicate paths, static responses
    have CSP/frame/nosniff headers, CSP omits `unsafe-inline`, style attributes
    are blocked, retained inline styles have exact SHA-256 hashes, generic IIS
    errors contain no physical paths or diagnostics, unsupported verbs do not
    leak HTTP.sys product headers, and tool output exposes neither absolute
    paths nor child-process command lines. Include encoded slash and backslash
    probes and reject reflection of attacker-controlled path text.
14. Run `Stress-IccIisIsapi.ps1` with `pwsh`, then remove the temporary IIS
    site and app pool.

Report endpoint status codes, tool exit classifications, report and workspace
URLs, browser errors, and any skipped checks. Do not call exit code 1 from
`iccPawgReport` a crash; it means the assessment completed with findings.
