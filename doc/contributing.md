Guidelines for Contributing
===========================

You don't need to be a developer in the repository to contribute.
Anyone is welcome to become a contributor by creating [issues](#issues) or [pull requests](#pullrequests).

Commits
-------

Please follow the guidelines below in your commits in the repository or in [pull requests](#pullrequests) submitted to the repository.

### Contents

- Make atomic commits.
  - Each commit should contain concise and related changes.
  - Separate unrelated changes or that do not require each other into separate commits.
  - Separate code formatting or other style changes into a separate initial commit.
  - Separate fixes to unrelated code that you happen to find while implementing your change into initial commits.
- Do not commit generated files.
- Do not commit dependencies or code that can be referenced or used from elsewhere.

### Message

Follow the [Conventional Commits](https://conventionalcommits.org/) structure:

```
<type>[optional scope]: <description>

[optional body]

[optional footer]
```

- Wrap all lines at 72 characters.
  - Try to keep the first line (subject) around 50 characters.
- Use the following **types**:
  - For changes affecting product implementation:
    - `fix`: corrections for the user.
    - `feat`: new feature for the user.
    - `refactor`: other changes that might affect the product behavior.
    - `style`: changes that should not affect the product behavior.
  - For changes on supporting files (in order of precedence):
    - `test`: changes afecting testing.
    - `build`: changes afecting building/installation.
    - `chore`: changes in other development supporting files.
    - `docs`: changes afecting documentation only.
- Do not include a **scope**.
- **Description** should be written:
  - As a single sentence.
  - In imperative and present tense ("add", not "added" nor "adds").
  - Without a capitalized first letter.
  - Without a dot (.) at the end.
- Include a `Fix: #123` **footer** for each issue that the commit addresses,
where `123` is a issue number in GitHub.

### Checklist

When commiting changes to the implementation,
please make sure that:

- Tests were included or updated to assert the correctness of the changes.
- The [change log](changelog.md) was updated describing changes relevant to users since the last release.
- Other documentation in the repo where updated accordingly, especially the [manual](manual.md) and [demos](../demo).

Pull Requests
-------------

If you are not a developer in the repository,
please create a [pull requests from a fork](https://docs.github.com/en/github/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/creating-a-pull-request-from-a-fork).

Please make sure the submitted commits follow the [guidelines](#commits).

Issues
------

Try to use the provided [templates](https://github.com/renatomaia/coutil/issues/new/choose) whenever they fit,
but feel free to create a [blank issue](https://github.com/renatomaia/coutil/issues/new) if the templates are not appropriate.

Please make sure to search the [current issues](https://www.github.com/renatomaia/coutil/issues) for already existing ones dealing with the same or similar matters,
and consider improving and contributing to the issue instead of creating a new one.

Labels
------

Labels shall be used to organize issues, pull requests and other features provided by GitHub.
The recommended labels are grouped into the the following categories:

### Product Design

Labels to describe the relation with the current product design
(how it is intended to behave and its use cases).

- `bug`: to meet the intended design (fixes).
- `enhancement`: to better meet the current or an expanded design (refactoring or additional features).
- `redesign`: to realize a new alternative design (compatibility breaking changes).

**Note**: these labels have no direct implication on [Semantic Versioning](https://semver.org/spec/v2.0.0.html),
which shall be later specified by the [commit messages](#messages) addressing the issue.

### Analysis Decisions

Labels to reflect the perceived concesus of analysis by the developers and the other contributors.

- `duplicate`: should be addressed together with some other issues.
- `invalid`: is not addressable as currently specified (not reproducible, not applicable anymore).
- `wontfix`: is not desirable to be addressed.

### Action Requests

Labels to identify required actions by contributors.

- `good first issue`: intended for newcomers.
- `help wanted`: intended for contributors.
- `question`: clarification is required.

### Complementary Work

Labels for complementary work that is missing,
but does not significantly change the [product behavior](#productdesign).

- `documentation`: adjustments in documentation (including demos).
- `testing`: improvements in testing.
- `portability`: correction for a platform in particular while it is known to work in some other platform.
(otherwise it should be labeled as a `bug`).
For adjustments particular to any of the following platforms,
use the following labels instead:
	- `linux`: GNU/Linux.
	- `windows`: Microsoft Windows.
	- `macos`: Apple MacOS.
	- `freebsd`: FreeBSD.
