# libpng iCCP Fault QA

Reproduce and quality-assure the pinned libpng iCCP stream-completion and
CRC-policy faults using `.github/skills/libpng-iccp-qa/SKILL.md`.

Report:

- iccDEV and libpng revisions and the tracked patch SHA-256;
- compiler, sanitizer, zlib, and CMake versions;
- the vulnerable-before-patch and fixed-after-patch outcomes for extra output,
  bad Adler-32, missing Adler-32, and bad PNG CRC;
- valid-control and explicit relaxed-CRC-policy preservation;
- decoded ICC fixture byte count and SHA-256;
- warning-free build, CTest discovery and focused execution results, and any
  skipped check.

Keep dependency clones, build products, generated carriers, and logs outside
the repository. Do not create standalone C++ reproducers or add the external
build to general iccDEV pull-request workflows.
