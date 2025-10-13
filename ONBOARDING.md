# Onboarding

This document is an outline of the things we tell new collaborators at their
onboarding session.

## One week before the onboarding session

* If the new Collaborator is not yet a member of the Source-Authors GitHub organization,
  confirm that they are using [two-factor authentication][]. It will not be
  possible to add them to the organization if they are not using two-factor
  authentication. If they cannot receive SMS messages from GitHub, try
  [using a TOTP mobile app][].

## Fifteen minutes before the onboarding session

Prior to the onboarding session, add the new Collaborator to
  [the collaborators team](https://github.com/orgs/Source-Authors/teams/source-authors-collaborators).

## Onboarding session

* This session will cover:
  * [local setup](#local-setup)
  * [project goals and values](#project-goals-and-values)
  * [managing the issue tracker](#managing-the-issue-tracker)
  * [reviewing pull requests](#reviewing-pull-requests)
  * [landing pull requests](#landing-pull-requests)

## Local setup

* git:
  * Make sure you have whitespace=fix: `git config --global --add
    apply.whitespace fix`
  * Always create a branch in your own GitHub fork for pull requests
    * Branches in the `Source-Authors/Obsoletium` repository are only for release lines
  * Add the canonical Obsoletium repository as `upstream` remote:
    * `git remote add upstream git@github.com:Source-Authors/Obsoletium.git`
  * To update from `upstream`:
    * `git checkout main`
    * `git fetch upstream HEAD`
    * `git reset --hard FETCH_HEAD`
  * Make a new branch for each pull request you submit.
  * Membership: Consider making your membership in the Source-Authors GitHub
    organization public. This makes it easier to identify collaborators.
    Instructions on how to do that are available at
    [Publicizing or hiding organization membership][].

* Notifications:
  * Use <https://github.com/notifications> or
    set up email
  * Watching the main repository will flood your inbox (several hundred
    notifications on typical weekdays), so be prepared

## Project goals and values

* Collaborators are the collective owners of the project
  * The project has the goals of its contributors

* There are some higher-level goals and values
  * Empathy towards users matters (this is in part why we onboard people)
  * Generally: try to be nice to people!
  * The best outcome is for people who come to our issue tracker to feel like
    they can come back again.

* You are expected to follow _and_ hold others accountable to the
  [Code of Conduct][].

## Managing the issue tracker

* You have (mostly) free rein; don't hesitate to close an issue if you are
  confident that it should be closed.
  * Be nice about closing issues! Let people know why, and that issues and pull
    requests can be reopened if necessary.

* See [Labels][].
  * Please add the [`author-ready`][] label for pull requests, if applicable.

* See [Who to CC in the issue tracker][who-to-cc].
  * This will come more naturally over time
  * For many of the teams listed there, you can ask to be added if you are
    interested
    * Some are WGs with some process around adding people, others are only there
      for notifications

## Reviewing pull requests

* The primary goal is for the codebase to improve.

* Secondary (but not far off) is for the person submitting code to succeed. A
  pull request from a new contributor is an opportunity to grow the community.

* Review a bit at a time. Do not overwhelm new contributors.
  * It is tempting to micro-optimize. Don't succumb to that temptation. Techniques that provide improved performance today may be
    unnecessary in the future.

* Be aware: Your opinion carries a lot of weight!

* Nits (requests for small changes that are not essential) are fine, but try to
  avoid stalling the pull request.
  * Identify them as nits when you comment: `Nit: change foo() to bar().`
  * If they are stalling the pull request, fix them yourself on merge.

* Insofar as possible, issues should be identified by tools rather than human
  reviewers. If you are leaving comments about issues that could be identified
  by tools but are not, consider implementing the necessary tooling.

* Minimum wait for comments time
  * There is a minimum waiting time which we try to respect for non-trivial
    changes so that people who may have important input in such a distributed
    project are able to respond.
  * For non-trivial changes, leave the pull request open for at least 48 hours.
  * If a pull request is abandoned, check if they'd mind if you took it over
    (especially if it just has nits left).

* Approving a change
  * Collaborators indicate that they have reviewed and approve of the changes in
    a pull request using GitHub's approval interface
  * Some people like to comment `LGTM` (“Looks Good To Me”)
  * You have the authority to approve any other collaborator's work.
  * You cannot approve your own pull requests.
  * When explicitly using `Changes requested`, show empathy – comments will
    usually be addressed even if you don't use it.
    * If you do, it is nice if you are available later to check whether your
      comments have been addressed
    * If you see that the requested changes have been made, you can clear
      another collaborator's `Changes requested` review.
    * Use `Changes requested` to indicate that you are considering some of your
      comments to block the pull request from landing.

* What belongs in Obsoletium:
  * Opinions vary – it's good to have a broad collaborator base for that reason!
  * If Obsoletium itself needs it (due to historical reasons), then it belongs in
    Obsoletium.
    * That is to say, `url` is there because of `http`, `freelist` is there
      because of `http`, etc.
  * Things that cannot be done outside of core, or only with significant pain
    such as `async_hooks`.

* Continuous Integration (CI) Testing:
  * <https://github.com/Source-Authors/Obsoletium/actions>
    * It runs automatically.

## Landing pull requests

See the Collaborator Guide: [Landing pull requests][].

Commits in one pull request that belong to one logical change should
be squashed. It is rarely the case in onboarding exercises, so this
needs to be pointed out separately during the onboarding.

## Final notes

* Don't worry about making mistakes: everybody makes them, there's a lot to
  internalize and that takes time (and we recognize that!)
* Almost any mistake you could make can be fixed or reverted.
* The existing collaborators trust you and are grateful for your help!
* If you are interested in helping to fix coverity reports consider requesting
  access to the projects coverity project as outlined in [static-analysis][].

[Code of Conduct]: https://github.com/Source-Authors/Obsoletium/blob/HEAD/docs/contributing/CODE_OF_CONDUCT.md
[Labels]: docs/contributing/COLLABORATOR_GUIDE.md#labels
[Landing pull requests]: docs/contributing/COLLABORATOR_GUIDE.md#landing-pull-requests
[Publicizing or hiding organization membership]: https://docs.github.com/en/account-and-profile/how-tos/setting-up-and-managing-your-personal-account-on-github/managing-your-membership-in-organizations/publicizing-or-hiding-organization-membership
[`author-ready`]: doc/contributing/collaborator-guide.md#author-ready-pull-requests
[static-analysis]: docs/contributing/STATIC_ANALYSIS.md
[two-factor authentication]: https://docs.github.com/en/authentication/securing-your-account-with-two-factor-authentication-2fa
[using a TOTP mobile app]: https://docs.github.com/en/authentication/securing-your-account-with-two-factor-authentication-2fa/configuring-two-factor-authentication
[who-to-cc]: docs/contributing/COLLABORATOR_GUIDE.md#who-to-cc-in-the-issue-tracker
