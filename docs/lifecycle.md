Hello!
If you're here it means you're probably interested in contributing to OpenCBDC.
Welcome and we're so excited to work with you!
This document is meant to walk you through the entire process that happens when contributing to OpenCBDC.

# Setup git and GitHub (first time only)

OpenCBDC uses [`git`](https://git-scm.com/book/en/v2/Git-Internals-Git-References) as its [version control system](https://en.wikipedia.org/wiki/Version_control) (it underlies GitHub).
While you can explore the code and other aspects of the project with just a web-browser, in order to contribute code to the project and aid in development, you'll want to have `git` setup.

GitHub provides [a guide](https://docs.github.com/en/get-started/quickstart/set-up-git) to walk you through setting up the basics.

# Fork and Clone the Code (first time only)

Once you have `git` installed and configured, you should [fork the code](https://docs.github.com/en/get-started/quickstart/fork-a-repo).
This gives you a copy of the code that belongs to you (you can make changes to it freely without affecting the [“upstream”](https://stackoverflow.com/questions/2739376/definition-of-downstream-and-upstream/2739476#2739476) repositories).

This fork is the version of the code you should `clone` (which the above guide walks you through).

*After you complete this step, you're ready to start contributing and you shouldn't need to repeat these two first steps!*

# Discuss work and Propose a Solution

OpenCBDC is guided by a set of [Principles](contributing.md#OpenCBDCs-Guiding-Principles) and focuses on contributions in particular [Areas](contributing.md#Identified-Areas-of-Research).
The [Working Groups](contributing.md#The-Hub-and-Spoke-Model) each guide one of these areas.
If you're interested in contributing to an area guided by one of the Working Groups, you should attend their next meeting and propose some work you are interested in conducting.

The [Leaders](contributing.md#Leader) and other Participants will discuss with you some ideas and whether or not your contribution should be pursued right now (e.g., perhaps there is other work that needs to be finished before your contribution is possible; this is an excellent opportunity to try and contribute to that effort first!).
E.g., if you're interested in implementing contingent payments (e.g., only paying someone when they've completed a bit of work for you, but cryptographically ensuring they get paid for their labor upon completion), that would most likely be under the purview of the Programmability Working Group.

If your contribution wouldn't fall under the purview of any of the active Working Groups, you can open a [proposal](https://github.com/mit-dci/opencbdc-tx/labels/feedback%2Fproposal) on the main repository for the [Maintainers](contributing.md#Maintainer) to review.

Once you've determined where your work would be reviewed (either by a Working Group or by the Maintainers) and it's clear that your proposed contribution matches OpenCBDC's Principles, you can get started on implementing it!
It may be helpful to either modify your issue (if one was opened), or open a new issue that more specifically discusses the work you want to contribute.
In that issue, you can provide updates as you complete work, seek feedback from the general community about specific questions, and reference it when you submit your work for review and inclusion (for traceability).

# Implement

This is likely the longest part of the contribution life-cycle.
Here, you will write code, author tests, and run experiments.

While working on implementing, continue to attend the relevant community meetings and provide updates as you progress in your work.
These meetings are also an excellent opportunity to ask questions and seek help.

# Submit your work for Review

Your work is ready to be reviewed, so you need to [open a pull request](https://docs.github.com/en/github/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/creating-a-pull-request)!
By opening a pull-request, you're officially requesting Leaders (or Maintainers) to review and merge your contribution.

*Note*: When you open your pull request, remember to reference any relevant issues (e.g., proposals, discussions, related bugs, etc.).

# Respond to Feedback

Leaders (and Maintainers) will now look over your submitted code/tests/documentation/etc.
They will provide you with feedback (e.g., things that should be changed to be better inline with OpenCBDC), may ask questions (e.g., why you chose a particular implementation rather than some alternative), and may request that you make changes before they merge your contribution.

This is a **collaborative** process!
The Leaders and Maintainers are both trying to be helpful to you as a contributor (to both demonstrate the customary expectations of contributions as well as to help you hone your skills) and are aiming to curate merged contributions to best advance the project's research.
Engage in this process to help Leaders and Maintainers improve as well!

(Also note that the [better commits](getting-started.md#what-does-a-good-commit-look-like) you provide initially, the smoother all reviews will go!)

# Contribution Merged :tada:

Congratulations!
You've officially contributed code to OpenCBDC!

If your code was merged by a Working Group, the Leaders will work to get it into the Upstream OpenCBDC code base as soon as the batch of work relevant to your contribution is all ready for merging.

Celebrate and—when you're ready—[rinse and repeat](#Discuss-work-and-Propose-a-Solution)!
