Welcome to OpenCBDC!
This document is intended for anyone interested in OpenCBDC to offer guidelines for how to best understand and contribute to the project.
It does not include thorough technical descriptions of the work done so far.
To get more familiar with the system itself, please read through our [Getting Started Guide](getting-started.md) and our [Architecture Overview](architecture.md).

Central banks have traditionally played an integral role in supporting safe, low-cost, and accessible payment systems.
With the rise of digital payments, cryptocurrency, and various payment innovations enabled by blockchain technology, the world is rethinking how money is (and should be) designed.
Central Banks are currently considering the potential utility of Central Bank Digital Currencies (CBDCs).
Some Central Banks have [already started issuing](https://cbdctracker.org/) CBDCs, and many others have begun experiments.
This project is an opportunity for programmers worldwide to help tackle the most pressing problems and answer the most difficult questions surrounding the development and implementation of CBDCs.

# OpenCBDC's Guiding Principles

OpenCBDC is a *technical* endeavor to research, develop, design, and test software for hypothetical Central Bank Digital Currencies (CBDCs).
Below are the core principles guiding all avenues of the project's research and development.
*Note*: These are not necessarily reflective of stances or positions of any collaborator.

1. **Make as few policy assumptions as possible.**
   The policy decisions surrounding CBDCs are still very much in-flux, and will differ significantly between the jurisdictions of interested parties.
   Assumptions that policy will allow for specific functionality are acceptable so long as they do not constrain parallel work from making different assumptions.
1. **Modularity and choice are paramount.**
   When functionality or data formats are specified centrally, it constrains choices made throughout the rest of the system.
   To enable deep exploration of many, vastly different implementations, contributions should push data and functionality to the edges of the system.
1. **Empower users' agency and access.**
   The decisions made in this project may come to directly affect individual people and organizations; that impact cannot be forgotten.
   Technical decisions should prioritize user privacy and self-determination.
1. **Keep external software dependencies to a minimum.**
   Their functionality (including design decisions and bugs) might become obstacles to be worked-around or constraints on future work.
   Strike a balance between leveraging established solutions and creating new purpose-built implementations for OpenCBDC.
1. **The best work happens in the open.**
   Beyond [Linus's Law](https://en.wikipedia.org/wiki/Linus%27s_law), the diversity of ideas from external collaborators can help broaden the horizon, and ultimately result in better solutions.
   In addition to being released open-source, code and proposals should be developed in the open and be open to external contributions.
1. **Patches are welcome!**
   Each contribution's value is considered on its own (not based on contract, affiliation, or organization) and on how it aligns with these principles and progresses OpenCBDC's [Identified Areas of Research](#Identified-Areas-of-Research).
   Any interested party is welcome to constructively contribute.

The presented order of the above principles does *not* signify their relative importance.
Should a decision lead to any conflict between these principles, significant feedback and comment should be required before choosing to privilege one principle at the expense of another.

# Identified Areas of Research

The below are OpenCBDC's initial eight areas of focus for contributions.
These will change and evolve as the project progresses.
*Note*, the examples given beneath each area are purely illustrative and do not necessarily represent any preference or policy stance of any collaborator; any research in these areas will be considered for inclusion.

- **Privacy** - How can we balance user privacy with auditability and compliance?
    - Confidential Transactions
    - Support for shielded pools with strong privacy
    - Compliance controls for private transactions
- **Programmability** - What types of programmability can a CBDC support and with what implications?
    - Contingent Payments
    - Smart Contracts
- **Offline Payments** - How can we support secure offline payments?
    - Secure Hardware
    - Queuing/Syncing
- **Interoperability** - How do CBDCs interact with other currencies?
    - Physical exchange
    - Currency conversion
    - Cross-chain transactions
    - APIs with existing Banking systems
    - Mobile App Providers
- **Hardening and Security** - How can CBDC be minted, revoked, and spent with strong security-guarantees?
    - Spam Prevention (e.g., Rate Limiting, Fees, Identity)
    - Tamper-detection
    - Secure Minting/Burning
    - Formal Methods and Verification
- **User Experience** - What does it look like for users to actually *use* a CBDC?
    - Different/New/Novel use-cases
    - Wallets
    - Transaction Flows
- **Policy Implications** - What are the implications of and mechanisms for CBDCs to interact with current policy tools, or enable new ones?
    - Interest Rates
    - Roles of intermediaries
- **Architecture** - What are the benefits and trade-offs of various architectures and designs?
    - Alternative architectures (e.g., e-cash)
    - The Unspent funds Hash Set (UHS)
    - Existence Validation
    - Atomicity
    - Consistency
    - Networking
    - Storage
    - Performance
    - Fault Tolerance
    - Monitoring
    - Disaster Recovery

# Intellectual Property

Contributions exploring the technical aspects of CBDCs from any interested party are welcome.
To facilitate collaboration and contribution, we aim to ensure that the code here is freely available for further research and implementation.

To that end, all contributions submitted are required to contain a “Developer Certificate of Origin” (DCO).
All you must do to provide a DCO is include the following line in each of your commits:

```
Signed-off-by: Your Name <your.email.address@example.com>
```

**Note**: if using the `git` command-line client, you can simply pass `-s` to `git commit` and this line will automatically be added for you!
If you have made commits that you need to change after the fact, consult our [FAQ](getting-started.md#how-do-i-sign---off-on-my-contributions).
Additionally, the email address must match your email address on GitHub.

When you include this line in your commit, you attest to the following (available at https://developercertificate.org as well):

```
Developer Certificate of Origin
Version 1.1

Copyright (C) 2004, 2006 The Linux Foundation and its contributors.

Everyone is permitted to copy and distribute verbatim copies of this
license document, but changing it is not allowed.


Developer's Certificate of Origin 1.1

By making a contribution to this project, I certify that:

(a) The contribution was created in whole or in part by me and I
    have the right to submit it under the open source license
    indicated in the file; or

(b) The contribution is based upon previous work that, to the best
    of my knowledge, is covered under an appropriate open source
    license and I have the right under that license to submit that
    work with modifications, whether created in whole or in part
    by me, under the same open source license (unless I am
    permitted to submit under a different license), as indicated
    in the file; or

(c) The contribution was provided directly to me by some other
    person who certified (a), (b) or (c) and I have not modified
    it.

(d) I understand and agree that this project and the contribution
    are public and that a record of the contribution (including all
    personal information I submit with it, including my sign-off) is
    maintained indefinitely and may be redistributed consistent with
    this project or the open source license(s) involved.
```

**Note**: You do *not* lose copyright on the work you submit. Your work is yours!

# Licensing

OpenCBDC's code is licensed under the [MIT License](../COPYING).
As indicated by the text of the DCO, when you contribute code to OpenCBDC, you are agreeing that your code may be redistributed as a part of OpenCBDC under the same MIT license.

# Collaboration Model

OpenCBDC was initially derived from Project Hamilton, a collaboration between the [MIT Digital Currency Initiative (DCI)](https://dci.mit.edu/) and the [Federal Reserve Bank of Boston (FRBB)](https://www.bostonfed.org/), and development of the core transaction processing engine was worked on by both organizations.
However, as OpenCBDC itself will operate in the open (maintained by the DCI) and include other contributors, we need a more structured collaboration model.
We expect that the Collaboration Model below is a starting point and that it will evolve and be revised as needs change or become more clear.

Once quarterly, there will be a project meta-collaboration meeting where changes to the below model (e.g., adding or removing Maintainers, officially recognizing new Working Groups, more fundamental collaboration changes, etc.) can be suggested and discussed.
After being suggested and discussed, anyone who wishes to make the proposal will open a [Logistics Change](https://github.com/mit-dci/opencbdc-tx/issues/new/choose) where the finer details can be workshopped, and eventually be adopted.

## Roles

As the project expands, many different people may want to contribute.
Below are some roles we expect to fill (more may become useful or necessary):

### Maintainer

Maintainers are those contributors that have been given permission to push commits into the OpenCBDC repositories (also referred to as “upstream”).
They are able to freely push commits to upstream repositories at their own judgement; but by convention, all commits go through the pull-request flow (generally being reviewed by one other maintainer before merge).

The maintainers also serve as technical advisors to working group leaders, coordinators across the various working groups, and guides for the project as a whole.

Additionally, once-quarterly, maintainers will hold individual meetings with collaborating Central Banks to get feedback and updates on their priorities.
The maintainers will synthesize the findings from these meetings into a project-wide agenda to ensure the working groups are focused on important topics of interest.

### Leader

Leaders are the working group-level analog of maintainers.
They direct the efforts of their working group, curate contributions to their working group's repositories, and pull-request changes from their repositories to be merged upstream.
They will have regular meetings with the maintainers (which will not be open to other contributors) where they can seek technical advice, expertise, and guidance.

Initially, leaders will be appointed by the maintainers, but the process for contributors becoming leaders will evolve as the project progresses.

### Contributor

Someone who has helped OpenCBDC make technical progress (contributing could be opening issues, offering advice, troubleshooting bugs, running infrastructure, writing code, etc.).
Contributors can participate in one or more working groups to help consider, discuss, and implement work as guided by the working groups' leaders and the maintainers.

## The Hub-and-Spoke Model

1. There will be several maintainers (initially, one from the DCI) who will curate the core repositories and contributions to them.
   Additionally, the maintainers will serve as technical resources coordinating the working groups and their contributions, and as agenda setters for the project overall.
1. There will be 2–6 working groups—each WG will be directed by one to two leaders appointed by the maintainers (from the DCI or a collaborator)—focused on one of the [Identified Areas of Research](#Identified-Areas-of-Research).
   Each working group will maintain its own repository (or repositories) to track their work; the leaders will merge code into their main branches and (as pieces of work are deemed ready) pull-request changes to be merged upstream (to be then maintained by the maintainers).
   Leaders must keep their forks in-sync with upstream.
1. Other contributors may start “unofficial” working groups.
   If there is enough interest in an unofficial working group's research and development (and provided that they have remained inline with [OpenCBDC's Principles](#OpenCBDCs-Guiding-Principles) and in-sync with upstream), the maintainers may pursue officially recognizing the working group.
1. Both individuals and institutions—including those in the public, private, and academic sectors—are welcome and encouraged to contribute to OpenCBDC.
   If a particular bit of work is under the purview of one of the official working groups, it should be proposed to that working group and its leaders; otherwise, they are free to submit the work directly to upstream.

Though not ideal, it is likely that an official working group will eventually pursue work that affects the contributions of other official working groups.
When such situations arise, the affected working groups should raise a concern on the pull-request and work with the maintainers to resolve any conflicts.

# How to Get Involved

## Contributing Code

### Standards for Merging Contributions

OpenCBDC is open to evaluating *any* contribution which falls into one of the [Identified Areas of Research](#Identified-Areas-of-Research) and meets [our Principles](#OpenCBDCs-Guiding-Principles).
Other contributions which are in-line with our Principles, but fall outside any of the listed Identified Areas of Research are also welcome, but may be treated as lower priority.

OpenCBDC Maintainers and Working Group Leaders will collaborate with contributors to get constructive work merged (or referred to the appropriate Working Group for review).
However, work that does not align with our Principles, or does not advance the research efforts of OpenCBDC may be rejected.
It is a good idea to propose large changes in Working Group meetings before embarking on their implementation (to make sure OpenCBDC is interested in that work in-advance).

Additionally, in some circumstances, review will halt until more work is complete.
For instance, contributions will not be merged if they include changes which…

* do not include associated tests added to the test suite,
* reduce overall code coverage,
* fail Continuous Integration for any reason (e.g., compiling fails, tests fail, etc.), or
* have conflicts which prevent merging after [automatic rebase](https://github.blog/2016-09-26-rebase-and-merge-pull-requests)

### Style

We use `clang-tidy` to enforce a baseline level of consistency in the code base.
This enforces reasonably modern use of C++ as well as ensuring some good practices in code structure.

Additionally, `clang-tidy` will ensure that contributed code is formatted with `clang-format`.
Before submitting code for review, you should ensure you run `clang-format` on each source file you modify in your contribution (or the Continuous Integration tasks will fail).

### Testing

We use [GoogleTest](https://google.github.io/googletest) to setup our test suite, and [LCOV](https://github.com/linux-test-project/lcov) for generating code coverage metrics.
Our build system (and our development team) only targets Linux running on `x86_64` processors.
The docker configurations included in the project serve as de-facto specifications of supported environments; though, with a bit of extra effort, it should be possible to run OpenCBDC code on many similar environments.

All contributions should include (or update) any tests relevant to the modified code.
You can use the provided `./scripts/test.sh` to run the full test suite (and generate a full coverage report).
Additionally, any contribution which results in reduced overall code coverage will be put on-hold for review until the tests have been added or updated.

### How to write a good commit message

See our [FAQs](getting-started.md#what-does-a-good-commit-look-like) for general advice as well as some links to helpful resources.

### Resolving Requested Changes

If Maintainers or Leaders have any concerns or suggestions about proposed changes, they will comment them inline either as points of discussion or with suggestions for changes.
Engage in these discussions to address any concerns, and if you make any changes to your contributions to resolve the issues, make sure you rebase (cf. [Git's walkthrough on rebasing](https://git-scm.com/book/en/v2/Git-Branching-Rebasing)) your changes so each change is as clean and unitary as possible.
Once you have rebased your branch to include the requested changes, you can update your pull-request and mark the suggestions as resolved.
When you have resolved all the suggestions, or need more feedback, request a review from the same maintainer(s) who previously reviewed your pull-request.

## Experimentation

Much of the purpose of OpenCBDC is to serve as a laboratory where Central Banks can run experiments.
A large part of that is running test code on distributed infrastructure to offer some insight to performance scaling characteristics of different solutions.
Infrastructure and infrastructure management as well as proposals for different tests that may be useful to run are all useful and welcome!

## Other Ways

### Identify bugs

If you find a bug (or other defect like a performance problem), [open an issue](https://docs.github.com/en/issues/tracking-your-work-with-issues/creating-an-issue) on the [issue tracker](https://github.com/mit-dci/opencbdc-tx/issues/new/choose)!

### Comment on proposals

The [issue tracker](https://github.com/mit-dci/opencbdc-tx/issues) of this core repository will be used to organize and propose current and future work, discuss design trade-offs, propose particular features, etc.
Join the discussion!
Remember, diverse ideas breed diverse solutions!

**N.B.:** all official communication channels under OpenCBDC are subject to moderation.
Please see the [Code of Conduct](code_of_conduct.md) for more information on acceptable behavior.

### Read up and Speak Up!

Each Working Group will provide frequent meetings to go over open projects and pull-requests, seek review, and solicit feedback on new ideas (e.g., before they are officially proposed on the issue tracker).
These communication channels will be moderated in-accordance with the [Code of Conduct](code_of_conduct.md), but should also generally be visible to the public.
