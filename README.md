# Obsoletium

[![Build](https://github.com/Source-Authors/Obsoletium/actions/workflows/build.yml/badge.svg)](https://github.com/Source-Authors/Obsoletium/actions/workflows/build.yml)
[![Coverity](https://img.shields.io/coverity/scan/32472.svg)](https://scan.coverity.com/projects/source-authors-obsoletium)
[![Repo Size](https://img.shields.io/github/repo-size/Source-Authors/Obsoletium.svg)](https://github.com/Source-Authors/Obsoletium)

## Notes

Valve, the Valve logo, Half-Life, the Half-Life logo, the Lambda logo, Steam, the Steam logo, Team Fortress, the Team Fortress logo, Opposing Force, Day of Defeat, the Day of Defeat logo, Counter-Strike, the Counter-Strike logo, Source, the Source logo, Counter-Strike: Condition Zero, Portal, the Portal logo, Dota, the Dota 2 logo, and Defense of the Ancients are trademarks and/or registered trademarks of Valve Corporation. All other trademarks are property of their respective owners. See https://store.steampowered.com/legal for Valve Corporation legal details.

This repo is based on Valve's Source Engine from 2018. Please, use this for studying only, not for commercial purposes.
See https://partner.steamgames.com/doc/sdk/uploading/distributing_source_engine if you want to distribute something using Source Engine.

## Abstract

Obsoletium is an open-source, cross-platform game engine.

The Obsoletium project uses an [open governance model](./GOVERNANCE.md).

Contributors are expected to act in a collaborative manner to move
the project forward. We encourage the constructive exchange of contrary
opinions and compromise.

**This project has a [Code of Conduct][].**

## Table of contents

* [Support](#support)
* [Release types](#release-types)
  * [Download](#download)
    * [Current releases](#current-releases)
    * [Custom releases](#custom-releases)
  * [Verifying binaries](#verifying-binaries)
* [Building Obsoletium](#building-obsoletium)
* [Security](#security)
* [Contributing to Obsoletium](#contributing-to-obsoletium)
* [Current project team members](#current-project-team-members)
  * [Collaborators](#collaborators)
  * [Triagers](#triagers)
  * [Release keys](#release-keys)
* [License](#license)
* [Tools](#tools)

## Support

Looking for help? Check out the
[instructions for getting support](/docs/contributing/SUPPORT.md).

## Release types

* **Current**: Under active development. Code for the Current release is in the
  branch for its major version number (for example,
  [v1.x](https://github.com/Source-Authors/Obsoletium/tree/v1.x)). Obsoletium releases a new
  major version every 12 months, allowing for breaking changes.
* **Custom**: You can build code from the Current branch on your own. Use with caution.

Current releases follow [semantic versioning](https://semver.org). A
member of the Release Team [signs](#release-keys) each Current release.

### Download

Binaries, installers, and source tarballs are available at
<https://github.com/Source-Authors/Obsoletium/releases>.

#### Current releases

<https://github.com/Source-Authors/Obsoletium/releases>

#### Custom releases

Use your own naming schema.

### Verifying binaries

Downloads contain a `SHASUMS256.txt.asc` file with SHA checksums for the
files and the releaser PGP signature.

You can import the releaser keys in your default keyring, see
[Release keys](#release-keys) for commands to how to do that.

Then, you can verify the files you've downloaded locally:

```bash
curl -fsO "https://github.com/Source-Authors/Obsoletium/releases/tag/${VERSION}/SHASUMS256.txt.asc" \
&& gpgv --keyring="${GNUPGHOME:-~/.gnupg}/pubring.kbx" --output SHASUMS256.txt < SHASUMS256.txt.asc \
&& shasum --check SHASUMS256.txt --ignore-missing
```

## Building Obsoletium

See [BUILDING.md](./BUILDING.md) for instructions on how to build Obsoletium from
source and a list of supported platforms.

## Security

For information on reporting security vulnerabilities in Obsoletium, see
[SECURITY.md](./SECURITY.md).

## Contributing to Obsoletium

* [Contributing to the project][]

## Current project team members

For information about the governance of the Obsoletium project, see
[GOVERNANCE.md](./GOVERNANCE.md).

### Collaborators

* [dimhotepus](https://github.com/dimhotepus) -
  **Dmitry Tsarevich** <<dimhotepus@gmail.com>> (he/him)
* [RaphaelIT7](https://github.com/RaphaelIT7) -
  **Raphael** <<raphael.i.t@outlook.de>> (he/him)

Collaborators follow the [Collaborator Guide](./docs/contributing/COLLABORATOR_GUIDE.md) in
maintaining the Obsoletium project.

### Triagers

* [dimhotepus](https://github.com/dimhotepus) -
  **Dmitry Tsarevich** <<dimhotepus@gmail.com>> (he/him)

Triagers follow the [Triage Guide](./docs/contributing/ISSUES.md#triaging-a-bug-report) when
responding to new issues.

### Release keys

Primary GPG keys for Obsoletium Releasers:

* **Dmitry Tsarevich** <<dimhotepus@gmail.com>>
  `D80E14A4A52C46278F7B7B5C3B77A450568588BA`

You can import them from a public key server. Have in mind that
the project cannot guarantee the availability of the server nor the keys on
that server.

```bash
gpg --keyserver hkps://keys.openpgp.org --recv-keys D80E14A4A52C46278F7B7B5C3B77A450568588BA # Dmitry Tsarevich
```

See [Verifying binaries](#verifying-binaries) for how to use these keys to
verify a downloaded file.

## License

Obsoletium is available under the
[SOURCE 1 SDK LICENSE](https://github.com/ValveSoftware/source-sdk-2013/blob/master/LICENSE).  Obsoletium also includes
external libraries that are available under a variety of licenses.  See
[LICENSE](https://github.com/Source-Authors/Obsoletium/blob/master/LICENSE) for the full
license text.

## Tools

* [PVS-Studio](https://pvs-studio.com/en/pvs-studio/?utm_source=website&utm_medium=github&utm_campaign=open_source) - static analyzer for C, C++, C#, and Java code.

[Code of Conduct]: https://github.com/Source-Authors/Obsoletium/blob/HEAD/docs/contributing/CODE_OF_CONDUCT.md
[Contributing to the project]: ./CONTRIBUTING.md
