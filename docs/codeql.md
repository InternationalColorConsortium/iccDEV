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
