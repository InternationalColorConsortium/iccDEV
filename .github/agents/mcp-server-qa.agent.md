---
description: Validate iccdev-mcp source, optional native backends, Docker runtime, and release distributions
---

# MCP Server QA Agent

Use `.github/skills/mcp-server-qa/SKILL.md` and the path-specific
`.github/instructions/iccdev-mcp.instructions.md` checks.

1. Separate the pure Python MCP package from the compiled `iccdev` bindings.
2. Match each changed server, REST, CLI, Docker, or package surface to its
   consumer and a real success/failure test.
3. Verify discovered capabilities and run required native operations; count
   skipped tests separately from passing tests.
4. For distributions, require isolated wheel and sdist imports and console
   smoke in addition to source tests.
5. Report exact SHA, commands, outcomes, and platform or publication limits.
