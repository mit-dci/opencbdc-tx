# 2022-04-05: Force-push against `trunk`

<details>

<summary>Fixing the initial commit co-author list</summary>

## Motivation

At launch of `opencbdc-tx`, the initial commit included squashed work from several authors.
To preserve record of that contribution, we included a `Co-authored-by` trailer for each author.
Additionally, following our [Contribution Guide](https://github.com/mit-dci/opencbdc-tx/blob/trunk/docs/contributing.md#intellectual-property), a `Signed-off-by` trailer was also included.

The complete trailer list as it originally appeared in the initial commit is below:

```

Co-authored-by: James Lovejoy <jameslovejoy1@gmail.com>
Co-authored-by: Spencer Connaughton <spencer@spencerconnaughton.com>
Co-authored-by: Gert-Jaap Glasbergen <gertjaap@wadagso.com>
Co-authored-by: Cory Fields <cory-nospam-@coryfields.com>
Co-authored-by: Neha Narula <narula@gmail.com>
Co-authored-by: Sam Stuewe <stuewe@mit.edu>
Co-authored-by: Kevin Karwaski <kevin.karwaski@gmail.com>
Co-authored-by: Viktor Urvantsev <israelipaperboy@outlook.com>
Co-authored-by: jallen-frb <Jonathan.Allen@bos.frb.org>
Co-authored-by: Anders Brownworth <anders-github@evantide.com>

Signed-off-by: Sam Stuewe <stuewe@mit.edu>
```

Unfortunately, the new-line separating the `Co-authored-by` trailers and the `Signed-off-by` trailer is errant.
`git` (at time of writing) expects trailers to be in a single group (not separated by new-lines) at the end of the commit message (before the patch-section).
As a result, `git` (and, by extension, GitHub) read this commit as not including any `Co-authored-by` lines.

You can verify this locally by copying the above text to a file and asking `git` to parse it:

```terminal
$ git interpret-trailers --parse <the-file>
Signed-off-by: Sam Stuewe <stuewe@mit.edu>
```

In short, this single newline caused all our initial contributors to not receive credit for their contribution.
In speaking with GitHub Support and talking with our initial contributors, it has been determined the best way to solve this problem would be to amend the root commit to remove the errant newline.

## Correction

On 05 April 2022, `trunk`'s root commit is amended to include the following trailer list:

```

Co-authored-by: James Lovejoy <jameslovejoy1@gmail.com>
Co-authored-by: Spencer Connaughton <spencer@spencerconnaughton.com>
Co-authored-by: Gert-Jaap Glasbergen <gertjaap@wadagso.com>
Co-authored-by: Cory Fields <cory-nospam-@coryfields.com>
Co-authored-by: Neha Narula <narula@gmail.com>
Co-authored-by: Sam Stuewe <stuewe@mit.edu>
Co-authored-by: Kevin Karwaski <kevin.karwaski@gmail.com>
Co-authored-by: Viktor Urvantsev <israelipaperboy@outlook.com>
Co-authored-by: jallen-frb <Jonathan.Allen@bos.frb.org>
Co-authored-by: Anders Brownworth <anders-github@evantide.com>
Signed-off-by: Sam Stuewe <stuewe@mit.edu>
```

As this change rewrites history of a public branch, all clones and forks (created before 2022-04-05) need to rebase—forks additionally need to force-push each rebased branch once.
Luckily, as only the commit message has changed, no conflicts are possible, so it is a very simple rebase to perform.
General instructions for how to fix your copy can be found below.
If you need more thorough information, please see [`git`'s guide on recovering from an upstream rebase](https://git-scm.com/docs/git-rebase#_recovering_from_upstream_rebase).

## Action You Should Take

### Fixing a Clone

The following applies if you cloned the code directly from upstream (the `origin` remote points to https://github.com/mit-dci/opencbdc-tx).

For each local branch, switch to the branch and rebase it.

```terminal
$ git switch <branchname>
$ git rebase origin/trunk
```

### Fixing a Fork

The following applies if you forked the upstream repository and are working on a clone from that fork (the `origin` remote points to your fork URL).

If your clone does not already have a remote pointing to upstream, add one now:

```terminal
$ git remote add upstream https://github.com/mit-dci/opencbdc-tx
```

For each local branch, switch to the branch, rebase it, and force-push it to your fork:

```terminal
$ git switch <localbranchname>
$ git rebase upstream/trunk
$ git push --force origin <forkbranchname>
```

**Note:** `<localbranchname>` and `<forkbranchname>` are likely to the same unless you chose to manually name your local branch differently.

</details>
