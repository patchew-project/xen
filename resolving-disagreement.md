# Resolving Disagreement

This guide provides Best Practice on resolving disagreement, such as
* Gracefully accept constructive criticism
* Focus on what is best for the community
* Resolve differences in opinion effectively

## Theory: Paul Graham's hierarchy of disagreement
Paul Graham proposed a **disagreement hierarchy** in a 2008 essay 
**[How to Disagree](http://www.paulgraham.com/disagree.html)**, putting types of
arguments into a seven-point hierarchy and observing that *moving up the
disagreement hierarchy makes people less mean, and will make most of them happier*.
Graham also suggested that the hierarchy can be thought of as a pyramid, as the 
highest forms of disagreement are rarer.

| ![Graham's Hierarchy of Disagreemen](https://upload.wikimedia.org/wikipedia/commons/a/a3/Graham%27s_Hierarchy_of_Disagreement-en.svg) |
| *A representation of Graham's hierarchy of disagreement from [Loudacris](http://www.createdebate.com/user/viewprofile/Loudacris) modified by [Rocket000](https://en.wikipedia.org/wiki/User:Rocket000)* |

In the context of the Xen Project we strive to **only use the top half** of the hierarchy.
**Name-calling** and **Ad hominem** arguments are not acceptable within the Xen
Project.

## Issue: Scope creep

One thing which occasionally happens during code review is that a code reviewer
asks or appears to ask the author of patch to implement additional functionality.

This could take for example the form of
> Do you think it would be useful for the code to do XXX? 
> I can imagine a user wanting to do YYY (and XXX would enable this)

That potentially adds additional work for the code author, which they may not have
the time to perform. It is good practice for authors to consider such a request in terms of
* Usefulness to the user
* Code churn, complexity or impact on other system properties
* Extra time to implement and report back to the reviewer

If you believe that the impact/cost is too high, report back to the reviewer. To resolve
this, it is advisable to
* Report your findings
* And then check whether this was merely an interesting suggestion, or something the
reviewer feels more strongly about

In the latter case, there are typically several common outcomes
* The **author and reviewer agree** that the suggestion should be implemented
* The **author and reviewer agree** that it may make sense to defer implementation
* The **author and reviewer agree** that it makes no sense to implement the suggestion

The author of a patch would typically suggest their preferred outcome, for example
> I am not sure it is worth to implement XXX
> Do you think this could be done as a separate patch in future?

In cases, where no agreement can be found, the best approach would be to get an
independent opinion from another maintainer or the project's leadership team.

## Issue: [Bikeshedding](https://en.wiktionary.org/wiki/bikeshedding)

Occasionally discussions about unimportant but easy-to-grasp issues can lead to
prolonged and unproductive discussion. The best way to approach this is to
try and **anticipate** bikeshedding and highlight it as such upfront. However, the
format of a code review does not always lend itself well to this approach, except
for highlighting it in the cover letter of a patch series.

However, typically Bikeshedding issues are fairly easy to recognize in a code review,
as you will very quickly get different reviewers providing differing opinions. In this case
it is best for the author or a reviewer to call out the potential bikeshedding issue using
something like

> Looks we have a bikeshedding issue here
> I think we should call a quick vote to settle the issue

Our governance provides the mechanisms of [informal votes](https://xenproject.org/developers/governance/#informal-votes-or-surveys) or
[lazy voting](https://xenproject.org/developers/governance/#lazyconsensus) which lend
themselves well to resolve such issues.

## Issue: Small functional issues

The most common area of disagreements which happen in code reviews, are differing
opinions on whether small functional issues in a patch series have to be resolved or
not before the code is ready to be submitted. Such disagreements are typically caused
by different expectations related to the level of perfection a patch series needs to fulfil
before it can be considered ready to be committed.

To explain this better, I am going to use the analogy of some building work that has
been performed at your house. Let's say that you have a new bathroom installed.
Before paying your builder the last instalment, you perform an inspection and you find
issues such as
* The seals around the bathtub are not perfectly event
* When you open the tap, the plumbing initially makes some loud noise
* The shower mixer has been installed the wrong way around

In all these cases, the bathroom is perfectly functional, but not perfect. At this point
you have the choice to try and get all the issues addressed, which in the example of
the shower mixer may require significant re-work and potentially push-back from your
builder. You may have to refer to the initial statement of work, but it turns out it does
not contain sufficient information to ascertain whether your builder had committed to
the level of quality you were expecting.

Similar situations happen in code reviews very frequently and can lead to a long
discussion before it can be resolved. The most important thing is to **identify**
a disagreement as such early and then call it out. Tips on how to do this, can be found
[here](communication-practice.md#Misunderstandings).

At this point, you will understand why you have the disagreement, but not necessarily
agreement on how to move forward. An easy fix would be to agree to submit the change
as it is and fix it in future. In a corporate software engineering environment this is the
most likely outcome, but in open source communities additional concerns have to be
considered.
* Code reviewers frequently have been in this situation before with the most common
  outcome that the issue is then never fixed. By accepting the change, the reviewers
  have no leverage to fix the issue and may have to spend effort fixing the issue
  themselves in future as it may impact the product they built on top of the code.
* Conversely, a reviewer may be asking the author to make too many changes of this
  type which ultimately may lead the author to not contribute to the project again.
* An author, which consistently does not address **any** of these issues may end up
  getting a bad reputation and may find future code reviews more difficult.
* An author which always addresses **all** of these issues may end up getting into
  difficulties with their employer, as they are too slow getting code upstreamed.

None of these outcomes are good, so ultimately a balance has been found. At the end
of the day, the solution should focus on what is best for the community, which may
mean asking for an independent opinion as outlined in the next section.

## Resolution: Asking for an independent opinion

Most disagreements can be settled by
* Asking another maintainer or committer to provide an independent opinion on the
  specific issue in public to help resolve it
* Failing this an issue can be escalated to the project leadership team, which is
  expected to act as referee and make a decision on behalf of the community

If you feel uncomfortable with this approach, you may also contact
mediation@xenproject.org to get advice. See our [Communication Guide](communication-guide.md)
for more information.

## Decision making and conflict resolution in our governance

Our [governance](https://xenproject.org/developers/governance/#decisions) contains
several proven mechanisms to help with decision making and conflict resolution.

See
* [Expressing agreement and disagreement](https://xenproject.org/developers/governance/#expressingopinion)
* [Lazy consensus / Lazy voting](https://xenproject.org/developers/governance/#lazyconsensus)
* [Informal votes or surveys](https://xenproject.org/developers/governance/#informal-votes-or-surveys)
* [Leadership team decisions](https://xenproject.org/developers/governance/#leadership)
* [Conflict resolution](https://xenproject.org/developers/governance/#conflict)
