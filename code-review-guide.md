# Code Review Guide

This document highlights what reviewers such as maintainers and committers look
for when reviewing your code. It sets expectations for code authors and provides
a framework for code reviewers.

This document does **not cover** the following topics:
* [Communication Best Practice](communication-practice.md)
* [Resolving Disagreement](resolving-disagreement.md)
* [Patch Submission Workflow](https://wiki.xenproject.org/wiki/Submitting_Xen_Project_Patches)
* [Managing Patch Submission with Git](https://wiki.xenproject.org/wiki/Managing_Xen_Patches_with_Git)

## What we look for in Code Reviews
When performing a code review, reviewers typically look for the following things

### Is the change necessary to accomplish the goals?
* Is it clear what the goals are?
* Do we need to make a change, or can the goals be met with existing
  functionality?

### Architecture / Interface
* Is this the best way to solve the problem?
* Is this the right part of the code to modify?
* Is this the right level of abstraction?
* Is the interface general enough? Too general? Forward compatible?

### Functionality
* Does it do what it’s trying to do?
* Is it doing it in the most efﬁcient way?
* Does it handle all the corner / error cases correctly?

### Maintainability / Robustness
* Is the code clear? Appropriately commented?
* Does it duplicate another piece of code?
* Does the code make hidden assumptions?
* Does it introduce sections which need to be kept **in sync** with other sections?
* Are there other **traps** someone modifying this code might fall into?

**Note:** Sometimes you will work in areas which have identified maintainability
and/or robustness issues. In such cases, maintainers may ask you to make additional
changes, such that your submitted code does not make things worse or point you
to other patches are already being worked on.

### System properties
In some areas of the code, system properties such as
* Code size
* Performance
* Scalability
* Latency
* Complexity
* &c
are also important during code reviews.

### Style
* Comments, carriage returns, **snuggly braces**, &c
* See [CODING_STYLE](https://xenbits.xenproject.org/gitweb/?p=xen.git;a=blob;f=CODING_STYLE)
  and [tools/libxl/CODING_STYLE](https://xenbits.xenproject.org/gitweb/?p=xen.git;a=blob;f=tools/libxl/CODING_STYLE)
* No extraneous whitespace changes

### Documentation and testing
* If there is pre-existing documentation in the tree, such as man pages, design
  documents, etc. a contributor may be asked to update the documentation alongside
  the change. Documentation is typically present in the
  [docs](https://xenbits.xen.org/gitweb/?p=xen.git;a=tree;f=docs) folder.
* When adding new features that have an impact on the end-user,
  a contributor should include an update to the
  [SUPPORT.md](https://xenbits.xen.org/gitweb/?p=xen.git;a=tree;f=docs) file.
  Typically, more complex features require several patch series before it is ready to be
  advertised in SUPPORT.md
* When adding new features, a contributor may be asked to provide tests or
  ensure that existing tests pass

#### Testing for the Xen Project Hypervisor
Tests are typically located in one of the following directories
* **Unit tests**: [tools/tests](https://xenbits.xenproject.org/gitweb/?p=xen.git;a=tree;f=tools/tests)
or [xen/test](https://xenbits.xenproject.org/gitweb/?p=xen.git;a=tree;f=xen/test)<br>
  Unit testing is hard for a system like Xen and typically requires building a subsystem of
  your tree. If your change can be easily unit tested, you should consider submitting tests
  with your patch.
* **Build and smoke test**: see [Xen GitLab CI](https://gitlab.com/xen-project/xen/pipelines)<br>
  Runs build tests for a combination of various distros and compilers against changes
  committed to staging. Developers can join as members and test their development
  branches **before** submitting a patch.
* **XTF tests** (microkernel-based tests): see [XTF](https://xenbits.xenproject.org/docs/xtf/)<br>
  XTF has been designed to test interactions between your software and hardware.
  It is a very useful tool for testing low level functionality and is executed as part of the
  project's CI system. XTF can be easily executed locally on xen.git trees.
* **osstest**: see [README](https://xenbits.xenproject.org/gitweb/?p=osstest.git;a=blob;f=README)<br>
  Osstest is the Xen Projects automated test system, which tests basic Xen use cases on
  a variety of different hardware. Before changes are committed, but **after** they have
  been reviewed. A contributor’s changes **cannot be applied to master** unless the
  tests pass this test suite. Note that XTF and other tests are also executed as part of
  osstest.

### Patch / Patch series information
* Informative one-line changelog
* Full changelog
* Motivation described
* All important technical changes mentioned
* Changes since previous revision listed
* Reviewed-by’s and Acked-by’s dropped if appropriate

More information related to these items can be found in our
[Patch submission Guide](https://wiki.xenproject.org/wiki/Submitting_Xen_Project_Patches).

## Reviewing for Patch Authors

The following presentation by George Dunlap, provides an excellent overview on how
we do code reviews, specifically targeting non-maintainers.

As a community, we would love to have more help reviewing, including from **new
community members**. But many people
* do not know where to start, or
* believe that their review would not contribute much, or
* may feel intimidated reviewing the code of more established community members

The presentation demonstrates that you do not need to worry about any of these
concerns. In addition, reviewing other people's patches helps you
* write better patches and experience the code review process from the other side
* and build more influence within the community over time

Thus, we recommend strongly that **patch authors** read the watch the recording or
read the slides:
* [Patch Review for Non-Maintainers slides](https://www.slideshare.net/xen_com_mgr/xpdds19-keynote-patch-review-for-nonmaintainers-george-dunlap-citrix-systems-uk-ltd)
* [Patch Review for Non-Maintainers recording - 20"](https://www.youtube.com/watch?v=ehZvBmrLRwg)
