# CodeQL Security Analysis

iccDEV ships custom CodeQL queries for ICC-specific security patterns. The
canonical query reference lives with the queries:

- [.github/codeql-queries/README.md](../.github/codeql-queries/README.md)

The targeted JSON query suite covers both `IccJSON` and `IccConnect` JSON
configuration code. Keep `.github/codeql-config.yml`, the CodeQL workflow path
filters, and the local runner help text in sync when that scope changes.

Run local analysis with:

```bash
.github/scripts/run-codeql-local.sh
```

Workflow and Python-script governance uses the preflight CodeQL gates instead
of the C/C++ database runner:

```bash
PREFLIGHT_BASE_REF=origin/master .github/scripts/preflight-safety-checks.sh --require-tools
```
