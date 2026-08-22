---
name: iis-isapi-qa
description: >
  Build, deploy, and validate the Windows iccIisIsapi HTTP endpoints, browser
  console, upload pipeline, PAWG report, artifacts, and security behavior.
allowed-tools:
  - read
  - grep
  - glob
  - powershell
  - shell(git:*)
---

# IIS ISAPI QA

Use this skill for changes under `Tools/Winnt/IccIisIsapi/`, its CMake target,
HTTP contract, deployment scripts, or browser console.

## Workflow

1. Read `../../../Tools/Winnt/IccIisIsapi/isapi-instructions.md` and
   `../../../Tools/Winnt/IccIisIsapi/api.md`.
2. Configure a Windows shared-library build with IIS tools, tests, and tools
   enabled.
3. Build the `install` target so the site root contains every runtime library
   and wrapped executable.
4. Run `iccIisIsapiSmoke.exe` against `iccIisIsapi.dll`.
5. Install an isolated IIS site on a non-default port.
6. Run `Test-IccIisIsapiEndpoints.ps1` with a tracked ICC profile.
7. Open `endpoints.html` in a browser and verify:
   - summary, health, and XML buttons render successful responses;
   - the profile upload form uses plain user-facing language;
   - the Profile Assessment Report is visible and downloadable;
   - workspace and artifact links open;
   - the console has no JavaScript or missing-resource errors.
8. Test malformed input kinds, oversized uploads, unsafe filenames, and method
   rejection. Also test missing `X-ICCDEV-Request`, incorrect media types,
   cross-site requests, XML `DOCTYPE`, workspace enumeration, active-content
   upload extensions, internal path disclosure, and response security headers.
   Reject CSP containing `unsafe-inline`; require blocked style attributes and
   exact SHA-256 hashes for retained inline stylesheet blocks.
9. Remove the temporary site and app pool.

## Required behavior

- Accept only `input=icc` or `input=xml`; reject other values with HTTP 400.
- Keep the upload limit at 16 MB unless a reviewed design changes it.
- Require `X-ICCDEV-Request: 1` and the documented ICC/XML media type.
- Bind local QA sites to `127.0.0.1`.
- Force uploaded files to `.icc` or `.xml`; never serve attacker-selected
  active-content extensions.
- Use non-enumerated 128-bit workspace identifiers, retain at most 100 jobs,
  expire jobs after 24 hours, and deny the workspace root.
- Generate workspace identifiers with the Windows system cryptographic RNG.
- Require IIS authentication and authorization for non-loopback deployment.
- Require generic IIS 403/404 responses with no physical paths or diagnostic
  details, and verify malformed request paths are not reflected.
- Reject XML `DOCTYPE` declarations in the HTTP service.
- Run child processes with argument arrays, bounded timeouts, and captured
  output.
- Treat `iccPawgReport` exit code 1 as a completed assessment with findings,
  not a process crash.
- Persist the PAWG output as `icc-pawg-report.txt`.
- Sanitize filenames, JSON, HTML, browser links, and error messages.
- Keep child-process arguments workspace-relative so output does not disclose
  server filesystem paths.
- Do not publish child-process command lines in JSON or workspace HTML.
- Route every method through the ISAPI handler so unsupported verbs receive
  the controlled 405 response rather than an HTTP.sys diagnostic response.
- Do not expose benchmark or arbitrary-argv tools through this endpoint.

## Validation

Run `git diff --check`, verify changed files are ASCII, parse the OpenAPI YAML,
and cite the live IIS test output in the handoff.
