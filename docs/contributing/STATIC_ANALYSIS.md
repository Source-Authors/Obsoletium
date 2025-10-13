# Static Analysis

The project uses PVS-Studio and Coverity to scan Obsoletium source code and to report potential
issues in the C/C++ code base.

Those who have been added to the [Obsoletium coverity project][] can receive emails
when there are new issues reported as well as view all current issues
through <https://scan9.scan.coverity.com/reports.htm>.

Any collaborator can ask to be added to the Obsoletium coverity project
by opening an issue in the [Source-Authors/Obsoletium][] repository titled
`Please add me to coverity`. A member of the build WG with admin access will
verify that the requester is an existing collaborator as listed in the
[collaborators section][] on the Source-Authors/Obsoletium project repo. Once validated the
requester will be added to the coverity project.

For PVS-Studio see [Use PVS-Studio to analyze open-source projects][].

[Obsoletium coverity project]: https://scan.coverity.com/projects/source-authors-obsoletium
[Source-Authors/Obsoletium]: https://github.com/Source-Authors/Obsoletium/
[collaborators section]: https://github.com/Source-Authors/Obsoletium/blob/master/README.md#collaborators
[Use PVS-Studio to analyze open-source projects]: https://pvs-studio.com/en/blog/posts/1283/