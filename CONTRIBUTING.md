# Contributing to International Color Consortium Software

Thank you for your interest in contributing to ICC software. This document
summarizes the contribution process:

* [Get Connected](#Get-Connected)
* [Legal Requirements](#Legal-Requirements)
* [Getting Started](#Getting-Started)
* [Code Style](#Code-Style)
* [Development Workflow](#Development-Workflow)
* [Versioning Policy](#Versioning-Policy)

Contributors submit content to the project, Committers review and approve such
submissions, and the ICC provides general project oversight.

We require all participants to abide by our [Code of Conduct](CODE_OF_CONDUCT.md).

## Get Connected

Before starting non-trivial work, open or join a discussion with maintainers.
Your issue, feature, or question may already have context, constraints, or a
preferred implementation path.

**Before opening a pull request, please start a conversation with us.**
Pull requests should be tied to an existing issue that explains the problem or
feature being addressed.

### Where to talk

* [GitHub Issues](https://github.com/InternationalColorConsortium/iccDEV/issues)
  are the primary place for bug reports, feature requests, and planned changes.

* [GitHub Discussions](https://github.com/InternationalColorConsortium/iccDEV/discussions)
  are the place for open-ended questions, ideas, and early design feedback.

Questions such as "How do I do X with ICC profiles?" are better suited to the
ICC Members Mailing List.

## Legal Requirements

The [International Color Consortium IP policy](https://www.color.org/iccip.xalter)
governs ICC specification development and contributions to ICC open source
software. Software contributions are also covered by the Contributor License
Agreement (CLA).

### Contributor License Agreements

Developers who wish to contribute code for inclusion in ICC software must first
complete a **Contributor License Agreement (CLA)**.

There is no cost or membership requirement to sign the CLA. This is different
from membership in the International Color Consortium. If your organization
relies on ICC projects, please consider becoming a member.

* If you are an individual writing the code on your own time and you are sure
  you are the sole owner of any intellectual property you contribute, you can
  sign the [CLA as an individual contributor](https://github.com/InternationalColorConsortium/.github/blob/main/docs/CLA.md).

* If you are writing the code as part of your job, or if there is any possibility
  that your employer might think they own any intellectual property you create,
  use the [Corporate Contributor License Agreement](https://github.com/InternationalColorConsortium/.github/blob/main/docs/CLA.md).

### License

ICC software is licensed under the BSD 3-Clause "New" or "Revised" License.
Contributions should use that license unless otherwise specified or approved by
the ICC.

### Copyright Notices

All new source files must begin with the ICC copyright notice and include or
reference the BSD 3-Clause "New" or "Revised" License.

### Intellectual Property and Patents

Participation in ICC development activities is subject to
[ICC's Patent Policy](https://www.color.org/iccip.xalter).

## Getting Started

Fork the repository, create a topic branch, make a focused change, and open a
pull request tied to the relevant issue. Keep unrelated changes in separate
branches and pull requests.

Build instructions are in [docs/build.md](docs/build.md). Tool, library, and
profile-test documentation starts at [docs/index.md](docs/index.md).

## Code Style

This is an older codebase. Consistency with nearby code is more important than
introducing a new style.

| Category | Style |
|----------|-------|
| **Indentation** | 2 space indentation, no tabs. |
| **Braces** | K&R style. |
| **Class/struct members** | Prefix with `m_`. |
| **Variables** | No uniform convention; match nearby code. |
| **Header guards** | Use header guards. |
| **Namespaces** | Currently not used broadly, though work is in progress. |
| **File organization** | Multiple classes per file, grouped by functionality. |
| **`std` namespace** | Minimize namespace pollution. |
| **Comments** | Match nearby code. |
| **Const correctness** | Make inputs, methods, and local variables const when appropriate. |
| **Warnings** | Keep compiler and static-analysis warnings at or near zero. |
| **Templates** | Keep new templates readable. |
| **Exceptions** | Most code uses manual return values for error handling. |
| **Containers vs. raw pointers** | Prefer STL containers, while respecting existing ownership patterns. |

## Development Workflow

Contributions should be submitted as GitHub pull requests. Small bug fixes and
documentation updates can be lightweight, but core functionality changes should
follow this protocol:

1. Create a topic branch in your local repository, following the naming format
   `feature/<your-feature>` or `bugfix/<your-fix>`.
2. Make focused changes, compile, and test thoroughly. Put unrelated changes in
   separate branches and pull requests.
3. Push commits to your fork.
4. Create a GitHub pull request from your topic branch.
5. Pull requests will be reviewed by project Committers and Contributors, who may
   discuss, offer feedback, request changes, or approve the work.
6. After the required Committer approvals, a Committer other than the PR
   contributor may squash and merge changes into the main branch.

## Versioning Policy

ICC projects label each version with three numbers: Major.Minor.Patch, where:

* **MAJOR** indicates major architectural changes
* **MINOR** indicates an introduction of significant new features
* **PATCH** indicates ABI-compatible bug fixes and minor enhancements
