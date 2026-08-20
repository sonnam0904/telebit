# [2.11.0](https://github.com/sonnam0904/telebit/compare/v2.10.0...v2.11.0) (2026-08-20)


### Features

* fix tone placement, add missing rimes, gate the double-tone escape, ([6c0e8fc](https://github.com/sonnam0904/telebit/commit/6c0e8fcd052ee6eb0537ba46b87711e05e2f5895))

# [2.10.0](https://github.com/sonnam0904/telebit/compare/v2.9.0...v2.10.0) (2026-08-10)


### Features

* **doctor:** check input-method modules for natively installed apps ([3e60dbe](https://github.com/sonnam0904/telebit/commit/3e60dbe18e651977204681d834b8b557a3f1ec04))
* update APT repository to support additional Debian and Fedora versions ([22ce0c4](https://github.com/sonnam0904/telebit/commit/22ce0c4c48066080e31ab6c6b9aa5be593c05503))

# [2.9.0](https://github.com/sonnam0904/telebit/compare/v2.8.1...v2.9.0) (2026-08-08)


### Features

* New CLI:  `telebit doctor` ([4c60649](https://github.com/sonnam0904/telebit/commit/4c60649ba40a5d56a5b6211d36dc48f01a992864))

## [2.8.1](https://github.com/sonnam0904/telebit/compare/v2.8.0...v2.8.1) (2026-08-06)


### Bug Fixes

* **cmake:** Added safeguards in the configuration saving process to prevent silent data loss during writes. ([31fea3b](https://github.com/sonnam0904/telebit/commit/31fea3babf9d7b74d552bc4559f7711805551a9f))

# [2.8.0](https://github.com/sonnam0904/telebit/compare/v2.7.0...v2.8.0) (2026-08-05)


### Features

* **packaging:** select fcitx5 session-wide via environment.d drop-in ([ec8e3e2](https://github.com/sonnam0904/telebit/commit/ec8e3e2a3d8fb3fc3fde4245e8ff0f191e9b7a11))

# [2.7.0](https://github.com/sonnam0904/telebit/compare/v2.6.2...v2.7.0) (2026-08-04)


### Features

* support for Anthropic Claude Messages API ([22e759e](https://github.com/sonnam0904/telebit/commit/22e759e0e3a07436f5a6aeee4be7d632c560f619))

## [2.6.2](https://github.com/sonnam0904/telebit/compare/v2.6.1...v2.6.2) (2026-08-03)


### Bug Fixes

* adjust Docker build script to handle file permissions ([5b76409](https://github.com/sonnam0904/telebit/commit/5b76409414cd6b7f61b1ca3797fa162a42aea9f5))
* update release configuration and documentation for Ubuntu 26.04 support ([1955698](https://github.com/sonnam0904/telebit/commit/19556983fddfd2d35f68b907548e240098bb5295))

## [2.6.1](https://github.com/sonnam0904/telebit/compare/v2.6.0...v2.6.1) (2026-07-31)


### Bug Fixes

* add support for default preedit programs (chrome) in Telebit Fcitx5 ([8644c9e](https://github.com/sonnam0904/telebit/commit/8644c9eff7af88bd108b354666c40a303b9b6d79))

# [2.6.0](https://github.com/sonnam0904/telebit/compare/v2.5.1...v2.6.0) (2026-07-28)


### Features

* enhance Vietnamese syllable processing with trailing hat escape and update release configuration ([1c24a54](https://github.com/sonnam0904/telebit/commit/1c24a547a6cc7583d11ae30bf7ff3cc3bfa8f758))

## [2.5.1](https://github.com/sonnam0904/telebit/compare/v2.5.0...v2.5.1) (2026-07-23)


### Bug Fixes

* enhance Vietnamese syllable processing for đ-initial rimes ([1582526](https://github.com/sonnam0904/telebit/commit/158252625664a13bff71e6765ef487dc7afd05ce))

# [2.5.0](https://github.com/sonnam0904/telebit/compare/v2.4.0...v2.5.0) (2026-07-22)


### Bug Fixes

* update installation scripts and README to include libcurl dependencies for .deb and RPM builds ([3ddf9d9](https://github.com/sonnam0904/telebit/commit/3ddf9d96df18dc802a19514cc74a2451a2e3ae63))


### Features

* Enhanced the Telebit fcitx5 addon to support AI features, including clipboard context and skill-based prompts. ([a89f848](https://github.com/sonnam0904/telebit/commit/a89f848c629f1b90b280f2eb967a5ceaa3bb86a5))

# [2.4.0](https://github.com/sonnam0904/telebit/compare/v2.3.3...v2.4.0) (2026-07-21)


### Features

* implement caching and auto-capitalization for Vietnamese syllable input ([56a1d6c](https://github.com/sonnam0904/telebit/commit/56a1d6cb8022b1bf57621b4cc9fb783fe4c384ae))

## [2.3.3](https://github.com/sonnam0904/telebit/compare/v2.3.2...v2.3.3) (2026-07-15)


### Bug Fixes

* add missing open-glide rimes (ưu, ươu, uyu, oeo, uây) to spell-check table ([db77e4d](https://github.com/sonnam0904/telebit/commit/db77e4d47da006dd5b47efe28b9a5db1635291c8))

## [2.3.2](https://github.com/sonnam0904/telebit/compare/v2.3.1...v2.3.2) (2026-07-13)


### Bug Fixes

* run apt repo publishing inline in the release job instead of a separate release-triggered job ([7c4000b](https://github.com/sonnam0904/telebit/commit/7c4000b48a4d552f0684ddb121db0df64abffc7c))

## [2.3.1](https://github.com/sonnam0904/telebit/compare/v2.3.0...v2.3.1) (2026-07-13)


### Bug Fixes

* prevent Jekyll processing of static assets in APT repository ([d99393a](https://github.com/sonnam0904/telebit/commit/d99393a9bd5e3ab0ab038019e22c8bc2ffb7324c))

# [2.3.0](https://github.com/sonnam0904/telebit/compare/v2.2.0...v2.3.0) (2026-07-13)


### Features

* enhance APT repository setup and publishing process ([3c13fb1](https://github.com/sonnam0904/telebit/commit/3c13fb1db51bf77055149bdd95749670ee3596a9))

# [2.2.0](https://github.com/sonnam0904/telebit/compare/v2.1.0...v2.2.0) (2026-07-13)


### Features

* enhance Vietnamese input handling with VNI support and modern tone style ([7f174f4](https://github.com/sonnam0904/telebit/commit/7f174f46cbdb7f9a514b2e4c5cfd1ccaa35a2063))

# [2.1.0](https://github.com/sonnam0904/telebit/compare/v2.0.3...v2.1.0) (2026-06-01)


### Features

* enhance TelebitFcitx5Engine with program tracking and configuration management ([ade508d](https://github.com/sonnam0904/telebit/commit/ade508d71a9a1e4ffb6e8bf7c0b14966f06fe21e))

## [2.0.3](https://github.com/sonnam0904/telebit/compare/v2.0.2...v2.0.3) (2026-05-28)


### Bug Fixes

* add special case handling for "gif" in Telex conversion ([c50823e](https://github.com/sonnam0904/telebit/commit/c50823e4810362b46d0da4958e50ab3ee09e87ef))

## [2.0.2](https://github.com/sonnam0904/telebit/compare/v2.0.1...v2.0.2) (2026-05-28)


### Bug Fixes

* improve state management for input contexts in the TelebitFcitx5Engine class. ([312b0ab](https://github.com/sonnam0904/telebit/commit/312b0abd79229d617c787547c3fac6d916c6364c))

## [2.0.1](https://github.com/sonnam0904/telebit/compare/v2.0.0...v2.0.1) (2026-04-03)


### Bug Fixes

* update repository links ([8c9f6e0](https://github.com/sonnam0904/telebit/commit/8c9f6e07c7ec3adb8b8eeaa93431a95aa016efac))

# [2.0.0](https://github.com/sonnam0904/telebit/compare/v1.3.1...v2.0.0) (2026-04-03)


* chore!: rebrand vnkey to Telebit (telebit-fcitx5) ([a831acf](https://github.com/sonnam0904/telebit/commit/a831acf702ea8bf79517c745ba254303230daacd))
* chore!: rebrand vnkey to Telebit (telebit-fcitx5) ([e23b14c](https://github.com/sonnam0904/telebit/commit/e23b14c44b75f25a88b6b7f037786a27cfea4ede))


### BREAKING CHANGES

* This is a full product and integration rename. Anything
that depended on the old vnkey / vnkey-fcitx5 names, file paths, fcitx5 addon
and input method IDs, shared object name, user addon config keys, CMake
target names, packaging metadata, or version override environment variables
must be updated. Existing users should remove the old input method if
needed, install the new addon, add Telebit (telebit-fcitx5) in
fcitx5-configtool, and restart fcitx5; prior per-IME settings are not
migrated automatically.
* This is a full product and integration rename. Anything
that depended on the old vnkey / vnkey-fcitx5 names, file paths, fcitx5 addon
and input method IDs, shared object name, user addon config keys, CMake
target names, packaging metadata, or version override environment variables
must be updated. Existing users should remove the old input method if
needed, install the new addon, add Telebit (telebit-fcitx5) in
fcitx5-configtool, and restart fcitx5; prior per-IME settings are not
migrated automatically.

## [1.3.1](https://github.com/sonnam0904/telebit/compare/v1.3.0...v1.3.1) (2026-03-30)


### Bug Fixes

* **release:** align RPM asset version with DEB releases ([f66622b](https://github.com/sonnam0904/telebit/commit/f66622badb2b72e6e0b6d5fdacf450f985994a88))

# [1.3.0](https://github.com/sonnam0904/telebit/compare/v1.2.4...v1.3.0) (2026-03-30)


### Features

* add RPM packaging support via CPack and GitHub Actions workflow ([3eff391](https://github.com/sonnam0904/telebit/commit/3eff3915b39fdfaf3573232bdba4796c59e72637))

## [1.2.4](https://github.com/sonnam0904/telebit/compare/v1.2.3...v1.2.4) (2026-03-29)


### Bug Fixes

* **engine:** allow Enter key to commit buffer and send messages immediately (Thank you hthienloc) ([272bad8](https://github.com/sonnam0904/telebit/commit/272bad8aa825a1b58a36aca465d794852cfe5c0b))

## [1.2.3](https://github.com/sonnam0904/telebit/compare/v1.2.2...v1.2.3) (2026-03-28)


### Bug Fixes

* improve .deb file handling in build script ([75683b5](https://github.com/sonnam0904/telebit/commit/75683b5ff2b532e8e74a945c316a4cfedf560326))
* update build process for dual-target .deb packages ([2a52cee](https://github.com/sonnam0904/telebit/commit/2a52ceeeeaf9ef0c933a036d084e7cc64976e4b5))

## [1.2.2](https://github.com/sonnam0904/telebit/compare/v1.2.1...v1.2.2) (2026-03-28)


### Bug Fixes

* update build process for Debian packages and enhance README instructions ([89afc8e](https://github.com/sonnam0904/telebit/commit/89afc8ea35ac694bb758cc174d56789e5b5c1369))

## [1.2.1](https://github.com/sonnam0904/telebit/compare/v1.2.0...v1.2.1) (2026-03-17)


### Bug Fixes

* **telex:** allow literal typing of "dd" using "ddd" escape in Telex conversion ([dcc105d](https://github.com/sonnam0904/telebit/commit/dcc105d409661d40df1da7882a00be3a2fc87a66))

# [1.2.0](https://github.com/sonnam0904/telebit/compare/v1.1.1...v1.2.0) (2026-03-12)


### Features

* **rime:** enhance main vowel table with additional vowels and tone placement rules ([2ae7f56](https://github.com/sonnam0904/telebit/commit/2ae7f56eb2e8a6375f72712f8ccd2a5dd2c7c9db))

## [1.1.1](https://github.com/sonnam0904/telebit/compare/v1.1.0...v1.1.1) (2026-03-12)


### Bug Fixes

* add support for additional Vietnamese words in Telex conversion ([dcbdcb6](https://github.com/sonnam0904/telebit/commit/dcbdcb6cf6e63eb9845d05546717912539c67a40))

# [1.1.0](https://github.com/sonnam0904/telebit/compare/v1.0.0...v1.1.0) (2026-03-12)


### Features

* handle non-contiguous triple vowels in Telex conversion ([bf03b85](https://github.com/sonnam0904/telebit/commit/bf03b85e972c22059f2e42b6572267ae60ed0dda))

# 1.0.0 (2026-03-12)


### Features

* Ađd semantic release ([3e71c46](https://github.com/sonnam0904/telebit/commit/3e71c46c325707c4b66a9abf95651e4df0c08ccd))
* vietnamese engine ([6fcbdff](https://github.com/sonnam0904/telebit/commit/6fcbdff5999de9e19ee2cf9af6d1260b807ebe36))

# Changelog

All notable changes to this project will be documented in this file.

The format is based on Keep a Changelog, and this project adheres to Semantic Versioning.
