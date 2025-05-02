# Contributing to Windows Container Tools Projects

@microsoft-github-policy-service agree company="Microsoft"

You can contribute to any of the tools with issues and PRs. Simply filing issues for problems you encounter is a great way to contribute. Contributing implementations is greatly appreciated.

## Reporting Issues

We always welcome bug reports, proposals and overall feedback. Here are a few tips on how you can make reporting your issue as effective as possible.

### Finding Existing Issues

Before filing a new issue, please search our [open issues](https://github.com/microsoft/windows-container-tools/issues) to check if it already exists.

If you do find an existing issue, please include your own feedback in the discussion. Do consider upvoting (👍 reaction) the original post, as this helps us prioritize popular issues in our backlog.

### Writing a Good Bug Report

Good bug reports make it easier for maintainers to verify and root cause the underlying problem. The better a bug report, the faster the problem will be resolved. Ideally, a bug report should contain the following information:

* A high-level description of the problem.
* A _minimal reproduction_, i.e. the smallest size of code/configuration required to reproduce the wrong behavior.
* A description of the _expected behavior_, contrasted with the _actual behavior_ observed.
* Information on the environment: OS/distro, CPU arch, SDK version, etc.
* Additional information, e.g. is it a regression from previous versions? are there any known workarounds?

### DOs and DON'Ts

Please do:

* **DO** follow our coding style.
* **DO** include tests when adding new features. When fixing bugs, start with
  adding a test that highlights how the current behavior is broken.
* **DO** keep the discussions focused. When a new or related topic comes up
  it's often better to create a new issue than to side track the discussion.
* **DO** feel free to blog, tweet, or share anywhere else about your contributions!

Please do not:

* **DON'T** make PRs for style changes.
* **DON'T** surprise us with big pull requests. For large changes, create
  a new discussion so we can agree on a direction before you invest a large amount
  of time. For bug fixes, create an issue.
* **DON'T** commit code that you didn't write. If you find code that you think is a good fit to add to any of the tools, file an issue and start a discussion before proceeding.
* **DON'T** submit PRs that alter licensing related files or headers. If you believe there's a problem with them, file an issue and we'll be happy to discuss it.

### Suggested Workflow

We use and recommend the following workflow:

1. Create an issue for your work.
    - You can skip this step for trivial changes.
    - Reuse an existing issue on the topic, if there is one.
    - Get agreement from the team and the community that your proposed change is a good one.
    - Clearly state that you are going to take on implementing it, if that's the case. You can request that the issue be assigned to you. Note: The issue filer and the implementer don't have to be the same person.
2. Create a personal fork of the repository on GitHub (if you don't already have one).
3. In your fork, create a branch off of main (`git checkout -b mybranch`).
    - Name the branch so that it clearly communicates your intentions, such as issue-123 or githubhandle-issue.
    - Branches are useful since they isolate your changes from incoming changes from upstream. They also enable you to create multiple PRs from the same fork.
4. Make and commit your changes to your branch.
5. Add new tests corresponding to your change, if applicable.
6. Build the repository with your changes.
    - Make sure that the builds are clean.
    - Make sure that the tests are all passing, including your new tests.
    - Fix any linting/styling issues, see details on how to [lint locally here](https://github.com/microsoft/windows-container-tools/wiki/Developing:-Running-Cpp-Lint-Locally-(Windows)).
7. Create a pull request (PR) against the [`microsoft/windows-container-tools`](https://github.com/microsoft/windows-container-tools/compare) repository's **main** branch.
    - State in the description what issue or improvement your change is addressing.
    - Check if all the tests are passing.
8. Wait for feedback or approval of your changes from the team.
9. When the team has signed off, and all checks are green, your PR will be merged.
    - The next official build will include your change.
    - You can delete the branch you used for making the change.

### Contributor License Agreement

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.opensource.microsoft.com.

When you submit a pull request, a CLA bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., status check, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

_*Parts of the guideline adopted from [.Net Monitor](https://github.com/dotnet/dotnet-monitor/blob/main/CONTRIBUTING.md)_
