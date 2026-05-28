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
