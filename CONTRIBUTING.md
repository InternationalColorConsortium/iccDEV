# Contributing to International Color Consortium Software

Thank you for your interest in contributing to ICC Software. This document
explains our contribution process and procedures, so please review it first:

* [Get Connected](#Get-Connected)
* [Legal Requirements](#Legal-Requirements)
* [Getting Started](#Getting-Started)
* [Development and Pull Requests](#Development-and-Pull-Requests)
* [Versioning Policy](#Versioning-Policy)

Contributors are anyone who submits content to the project, Committers 
review and approve such submissions, and the ICC provides general project
oversight.

We require all participants to abide by our [Code of Conduct](CODE_OF_CONDUCT.md).


## Get Connected

The first thing to do, before anything else, is to talk to us! Whether you are reporting an issue, requesting or implementing a feature, or just asking a question, please don’t hesitate to reach out to project maintainers or the community as a whole. This is an important first step because your issue, feature, or question may have been solved or discussed already, and you will save yourself a lot of time by asking first.

How do you talk to us? There are several ways to get in touch:

* [GitHub Issues](https://github.com/InternationalColorConsortium/iccDev/issues):
GitHub **issues** are a great place to start a conversation! Issues aren’t
restricted to bugs; we happily welcome feature requests and other suggestions
submitted as issues. The only conversations we would direct away from issues are
questions in the form of “How do I do X”. Those questions should be discussed on the ICC Members Mailing List.

## Legal Requirements

All official software projects hosted by the International Color Consoritum (ICC)
follows the open source software best practice policies. The [International Color Consoritum IP policy](https://www.color.org/iccip.xalter) governs ICC specification development and contributions to ICC open source software. Software contributions are also covered by the Contributor License Agreement (CLA).

### Contributor License Agreements

Developers who wish to contribute code to be considered for inclusion
in ICC software must first complete a **Contributor License Agreement
(CLA)**.

There is no cost or membership requirement to sign the ICC Contributor License Agreement (CLA). Please note that this is different from membership in the International Color Consortium (ICC). If your organization relies on our projects, please become a member. Membership dues are an essential source of funding and investment for these projects.

* If you are an individual writing the code on your own time and you are SURE you are the sole owner of any intellectual property you contribute, you can sign the [CLA as an individual contributor](https://github.com/InternationalColorConsortium/.github/blob/main/docs/CLA.md).

* If you are writing the code as part of your job, or if there is any possibility that your employer might think they own any intellectual property you create, then you should use the [Corporate Contributor Licence Agreement](https://github.com/InternationalColorConsortium/.github/blob/main/docs/CLA.md)

### License

ICC software is licensed under the BSD 3-Clause “New” or “Revised” License. Contributions to ICC software projects should abide by that license unless otherwised specified or approved by the ICC.

### Copyright Notices

All new source files must begin with the ICC Copyright notice and include or reference the BSD 3-Clause "New" or "Revised" License.

## Getting Started

So you’ve broken the ice and chatted with us, and it turns out you’ve found a
gnarly bug that you have a beautiful solution for. Wonderful!

From here on out we’ll be using a significant amount of Git and GitHub based
terminology. If you’re unfamiliar with these tools or their lingo, please look
at the [GitHub Glossary](https://help.github.com/articles/github-glossary/) or
browse [GitHub Help](https://help.github.com/).

The first requirement for contributing is to have a GitHub account. This is
needed in order to push changes to the upstream repository. After setting up
your account you should then **fork** the the ICC software project repository 
to your account. This creates a copy of the repository under your user namespace
and serves as the “home base” for your development branches, from which you will
submit **pull requests** to the upstream repository to be merged.

You will also need Git installed on your local development machine. If you need
setup assistance, please see the official [Git Documentation](https://git-scm.com/doc).

Once your Git environment is operational, the next step is to locally
**clone** your forked ICC project repository, and add a **remote** pointing to
the upstream ICC project repository. These topics are covered in
[Cloning a repository](https://help.github.com/articles/cloning-a-repository/)
and [Configuring a remote for a fork](https://help.github.com/articles/configuring-a-remote-for-a-fork/).

You are now ready to contribute.

## Code Style
It's an older codebase, it isn't perfect, but let's at least **try** to keep things consistent.
| Category                   |  Style
| -------------------------- | ---------------------------- |
| **Indentation**            | 2 space indentation, no tabs. |
| **Braces**                   | **K&R** style  |
| **Naming class/struct members** |  prefix with m_ |
| **Naming variables** | There is currently no uniform convention. Try to match nearby code. |
| **Header guards**         | Use header guards.   |
| **Namespaces**           | Currently not using namespaces, though there is work in progress. |
| **File organization**      | Multiple classes per file, grouped by functionality.  | 
| **Use of `std` namespace** | Minimize pollution.   | 
| **Commenting style**       | There is no consistent style. Try to match nearby code.   | 
| **Const correctness**      | Make inputs const when possible, class functions const when appropriate, and variables const as needed. |
| **Compiler Warnings** | Should be zero (or as close as we can get across all platforms). |
| **Static Analysis Warnings** | Should be zero (or as close as we can get across all platforms). |
| **Templates / Generics**   | Currently minimal.  Make sure new templates are readable. | 
| **Exceptions**             | Most of the code uses manual return values for error handling. |
| **Containers vs. Raw Pointers**  | Prefer STL containers, but historically uses a lot of raw pointers. |


## Development and Pull Requests

Contributions should be submitted as Github pull requests. See
[Creating a pull request](https://help.github.com/articles/creating-a-pull-request/)
if you're unfamiliar with this concept.

Small bug fixes and documentation changes can be
more informal, but modifications to core functionality MUST follow
process outlined below.

The development cycle for a code change should follow this protocol:

1. Create a topic branch in your local repository, following the naming format
"feature/<your-feature>" or "bugfix/<your-fix>".

2. Make changes, compile, and test thoroughly. Code style should match existing
style and conventions, and changes should be focused on the topic the pull
request will be addressing. Make unrelated changes in a separate topic branch
with a separate pull request.

3. Push commits to your fork.

4. Create a Github pull request from your topic branch.

5. Pull requests will be reviewed by project Committers and Contributors,
who may discuss, offer constructive feedback, request changes, or approve
the work.

6. Upon receiving the required number of Committer approvals, a Committer other than the PR contributor may squash and merge changes into the  main branch.


## Versioning Policy

ICC projects labels each version with three numbers: Major.Minor.Patch, where:

* **MAJOR** indicates major architectural changes
* **MINOR** indicates an introduction of significant new features
* **PATCH** indicates ABI-compatible bug fixes and minor enhancements