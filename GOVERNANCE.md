# Obsoletium Project Governance

* [Triagers](#triagers)
* [Collaborators](#collaborators)
  * [Collaborator activities](#collaborator-activities)
* [Collaborator nominations](#collaborator-nominations)
  * [Who can nominate Collaborators?](#who-can-nominate-collaborators)
  * [Ideal Nominees](#ideal-nominees)
  * [Nominating a new Collaborator](#nominating-a-new-collaborator)
  * [Onboarding](#onboarding)

## Triagers

Triagers assess newly-opened issues in the [Source-Authors/Obsoletium][] and [Source-Authors/source-physics][]
repositories. The GitHub team for Obsoletium triagers is _@source-authors/issue-triage_.
Triagers are given the "Triage" GitHub role and have:

* Ability to label issues and pull requests
* Ability to comment, close, and reopen issues and pull requests

See:

* [List of triagers](./README.md#triagers)
* [A guide for triagers](./docs/contributing/ISSUES.md#triaging-a-bug-report)

## Collaborators

Obsoletium core collaborators maintain the [Source-Authors/Obsoletium][] and [Source-Authors/source-physics][] GitHub repositories.
The GitHub team for Obsoletium core collaborators is @source-authors/collaborators.
Collaborators have:

* Commit access to the [Source-Authors/Obsoletium][] and [Source-Authors/source-physics][] repositories
* Access to the GitHub continuous integration (CI) jobs

Both collaborators and non-collaborators may propose changes to the Obsoletium
source code. The mechanism to propose such a change is a GitHub pull request.
Collaborators review and merge (_land_) pull requests.

One collaborator must approve a pull request before the pull request can land. Approving a pull request indicates that the collaborator accepts
responsibility for the change. Approval must be from collaborators who are not
authors of the change.

If a collaborator opposes a proposed change, then the change cannot land. Often, discussions or further changes
result in collaborators removing their opposition.

See:

* [List of collaborators](./README.md#current-project-team-members)
* [A guide for collaborators](./docs/contributing/COLLABORATOR_GUIDE.md)

### Collaborator activities

* Helping users and novice contributors
* Contributing code and documentation changes that improve the project
* Reviewing and commenting on issues and pull requests
* Participation in working groups
* Merging pull requests

Collaborators can remove inactive collaborators or provide them with _emeritus_
status. Emeriti may request that the collaborator restore them to active status.

A collaborator is automatically made emeritus (and removed from active
collaborator status) if it has been more than 12 months since the collaborator
has authored or approved a commit that has landed.

## Collaborator nominations

### Who can nominate Collaborators?

Existing Collaborators can nominate someone to become a Collaborator.

### Ideal Nominees

Nominees should have significant and valuable contributions across the Source-Authors
organization.

Contributions can be:

* Opening pull requests.
* Comments and reviews.
* Opening new issues.
* Participation in other projects, teams, and working groups of the Source-Authors
  organization.

#### The Authenticity of Contributors

The Obsoletium project does not require that contributors use their legal names or
provide any personal information verifying their identity.

It is not uncommon for malicious actors to attempt to gain commit access to
open-source projects in order to inject malicious code or for other nefarious
purposes. Anyone nominating
a new collaborator should take reasonable steps to verify that the contributions
of the nominee are authentic and made in good faith. This is not always easy,
but it is important.

### Nominating a new Collaborator

To nominate a new Collaborator:

* Open an issue in the [Source-Authors/Obsoletium][] repository. Provide a summary of
   the nominee's contributions (see below for an example). Mention
   @source-authors/collaborators in the issue to notify other collaborators about
   the nomination.

Example of list of contributions:

* Commits in the [Source-Authors/Obsoletium][] repository.
  * Use the link `https://github.com/Source-Authors/Obsoletium/commits?author=GITHUB_ID`
* Pull requests and issues opened in the [Source-Authors/Obsoletium][] repository.
  * Use the link `https://github.com/Source-Authors/Obsoletium/issues?q=author:GITHUB_ID`
* Comments on pull requests and issues in the [Source-Authors/Obsoletium][] repository
  * Use the link `https://github.com/Source-Authors/Obsoletium/issues?q=commenter:GITHUB_ID`
* Reviews on pull requests in the [Source-Authors/Obsoletium][] repository
  * Use the link `https://github.com/Source-Authors/Obsoletium/pulls?q=reviewed-by:GITHUB_ID`
* Help provided to end-users and novice contributors
* Pull requests and issues opened throughout the Source-Authors organization
  * Use the link `https://github.com/search?q=author:GITHUB_ID+org:Source-Authors`
* Comments on pull requests and issues throughout the Source-Authors organization
  * Use the link `https://github.com/search?q=commenter:GITHUB_ID+org:Source-Authors`
* Participation in other projects, teams, and working groups of the Source-Authors
  organization
* Other participation in the wider Source Engine community

The nomination passes if no collaborators oppose it (as described in the
following section) after one week.

#### How to review a collaborator nomination

A collaborator nomination can be reviewed in the same way one would review a PR
adding a feature:

* If you see the nomination as something positive to the project, say so!
* If you are neutral, or feel you don't know enough to have an informed opinion,
  it's certainly OK to not interact with the nomination.
* If you think the nomination was made too soon, or can be detrimental to the
  project, share your concerns. See the section "How to oppose a collaborator
  nomination" below.

Our goal is to keep gate-keeping at a minimal, but it cannot be zero since being
a collaborator requires trust (collaborators can start CI jobs, use their veto,
push commits, etc.), so what's the minimal amount is subjective, and there will
be cases where collaborators disagree on whether a nomination should move
forward.

Refrain from discussing or debating aspects of the nomination process
itself directly within a nomination private discussion or public issue.
Such discussions can derail and frustrate the nomination causing unnecessary
friction. Move such discussions to a separate issue or discussion thread.

##### How to oppose a collaborator nomination

An important rule of thumb is that the nomination process is intended to be
biased strongly towards implicit approval of the nomination. This means
discussion and review around the proposal should be more geared towards "I have
reasons to say no..." as opposed to "Give me reasons to say yes...".

Given that there is no "Request for changes" feature in discussions and issues,
try to be explicit when your comment is expressing a blocking concern.
Similarly, once the blocking concern has been addressed, explicitly say so.

Explicit opposition would typically be signaled as some form of clear
and unambiguous comment like, "I don't believe this nomination should pass".
Asking clarifying questions or expressing general concerns is not the same as
explicit opposition; however, a best effort should be made to answer such
questions or addressing those concerns before advancing the nomination.

Opposition does not need to be public. Ideally, the comment showing opposition,
and any discussion thereof, should be done in the private discussion _before_
the public issue is opened. Opposition _should_ be paired with clear suggestions
for positive, concrete, and unambiguous next steps that the nominee can take to
overcome the objection and allow it to move forward. 

Remember that all private discussions about a nomination will be visible to
the nominee once they are onboarded.

### Onboarding

After the nomination passes, the new collaborator should be onboarded. See
[the onboarding guide](./ONBOARDING.md) for details of the onboarding
process.

[Source-Authors/Obsoletium]: https://github.com/Source-Authors/Obsoletium
[Source-Authors/source-physics]: https://github.com/Source-Authors/source-physics
