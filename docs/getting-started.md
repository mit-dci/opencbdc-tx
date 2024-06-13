# Helpful Resources
* The [Project Readme](/README.md) includes a crash-course for getting setup with the code and running a few tests
* The [Contribution Guide](contributing.md) includes more thorough discussion of the project's guiding principles, governance model, and how to get involved
* The [Technical Reference](https://mit-dci.github.io/opencbdc-tx/) includes documentation for the code itself and how it works
* Code immediately merged may take a few minutes to go live as the docs get generated and page cache is cleared
* The [Architecture Guide](architecture.md) walks through the data model of transactions and the currently-implemented architectures
* The [Contribution Lifecycle Guide](lifecycle.md) takes you through what you can expect throughout the process of submitting a contribution to OpenCBDC

# Frequently Asked Questions

## What does a good commit look like?

<details>

A good commit message clearly communicates the goal of the included changes (the *why* rather than the *what*).
Chris Beams wrote a [great article](https://chris.beams.io/posts/git-commit/) on writing good commit messages.

A commit's contents should be very focused on accomplishing a single task (e.g., fixing a single bug).
In particular, you should strive for your commits to be [atomic](https://www.freshconsulting.com/insights/blog/atomic-commits/).

If there is an issue open that your contribution addresses, reference that issue number in the commit message's body text.

</details>

## How do I sign-off on my contributions?

<details>

OpenCBDC uses a [developer certificate of origin](https://developercertificate.org) (or DCO) to ensure that all contributions are made freely available under the same license as OpenCBDC's original code base.
To do that, when contributors submit code (or other changes that are reflected in any repository), they are required to “sign-off” their commits.

To sign off, you can just add the `-s` argument when you create your commit with `git commit` (i.e., use `git commit -s`).
This adds the following line to the bottom of your commit:

```
Signed-off-by: Your Name <your.email.address@example.com>
```

(You could manually type this out if you want to.)

</details>

## I've already created commits but forgot to add sign-offs; how do I fix them?

<details>

There are several options to add sign-offs retroactively:

### `--amend` your most-recent commit

If you only need to change the last commit you made, you can do the following:

```
$ git commit --amend --no-edit --signoff
$ git push --force origin <your-branch-name>
```

### `rebase` all the commits in your contribution (requires git version 2.13 or newer)

If you need to add a sign-off to each of the commits in your contribution, you can use `git rebase` to automatically add it to each one:

```
$ git rebase --signoff HEAD~X # replace X with the number of commits in your contribution
$ git push --force origin <your-branch-name>
```

### interactive `rebase`

If your version of git is older than 2.13 or you only need to add sign-offs to particular commits in your contribution, you can use an interactive rebase to choose the commits to modify:

```
$ git rebase -i HEAD~X # replace X with the number of commits in your contribution
```

A text editor will open showing your commits (make sure only your commits are listed; if not, exit the file, and rerun the rebase command with the correct value for `X`).
Mark all the commits that need a sign-off as “reword”.
The rebase will stop at each of these commits and let you run commands.

Run these two commands until the rebase is complete:

```
$ git commit --amend --no-edit --signoff
$ git rebase --continue
```

Now, force-push your branch:

```
$ git push --force origin <your-branch-name>
```

</details>

## Can I contribute without using docker (running some other environment, etc.)?

<details>

Absolutely!
However, we only officially support the included docker compose files (as they mirror our automated test environment).

After cloning the code, ``scripts/install-build-tools.sh`` needs to be ran just once. It will attempt to install the necessary software tools upon which the project relies on.  

Then, ``scripts/setup-dependencies.sh`` should be run to install libraries and dependencies
to will attempt to configure your environment.

**Note:** ``scripts/install-build-tools.sh`` only supports Ubuntu-based linux distributions and macOS (which depends on [Homebrew](https://brew.sh/)).
However, it can be used as a guide to understand what you must do to get your environment setup.

In short, ``scripts/install-build-tools.sh`` does the following:

* installs a couple packages needed for building and testing (e.g., clang, LLVM, cmake, make, lcov, googletest, git)
* installs the external dependencies:
    * [Google's LevelDB](https://github.com/google/leveldb)
    * [eBay's NuRaft](https://github.com/eBay/NuRaft)
* downloads a helper python script to run code linting and static analysis

**Note:** The code assumes it is running on Linux on an x86\_64 processor.
However, we generally tend towards keeping code portable, so any \*nix-like operating system on an x86\_64 processor may function well.

</details>

## What can I do to make it more likely my code will get merged quickly?

<details>

First and foremost, respond to feedback for your contributions quickly and cordially.
The faster any issues reviewers bring up are fixed, the faster we can merge your code!

However, here are several things you can do to make review as easy and quick as possible:

* Keep your working branch up-to-date with our main branch and free of merge conflicts
* Run ``./scripts/lint.sh`` and ``./scripts/test.sh`` and ensure both succeed before committing changes
* Run ``pylint $(git ls-files '*.py')`` for python code and seek scores close to 10.0. 
    * You can use a tool like [`act`](https://github.com/nektos/act) to run the CI locally and see if your changes would pass automated-review. ``act --list`` to see workflow jobs.
* Author [good commits](#what-does-a-good-commit-look-like)

</details>

## Are there easy things for me to get started on?

<details>

Definitely!
Take a look at our issue tracker's list of [good first issues](https://github.com/mit-dci/opencbdc-tx/labels/difficulty%2F01-good-first-issue).

</details>

## Is anyone available to give talks, presentations, or interviews?

<details>

Possibly!
Please send us [an email](mailto:dci-press@mit.edu) with your questions or requests and we will get back to you as soon as possible!

</details>
