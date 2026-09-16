# Licensing and attribution

The [MIT licence](LICENSE) applies to original project contributions for which
contributors hold copyright, with its warranty and liability disclaimers. It does
not relicense dependencies, derived framework code, game content or trademarks.

## PSXRecomp

Copyright (c) 2026 Matthew Stan. The framework uses **PolyForm Noncommercial 1.0.0**
with the clarification included in [its licence](psxrecomp/LICENSE).
The project scaffold, CMake integration, codegen_setup files, framework artwork
and patches retain the applicable upstream terms. This is not an unrestricted
MIT-only distribution; commercial rights to the framework are not granted here.

Upstream: https://github.com/mstan/psxrecomp
Pinned revision: d3e91e07b56c0b672be6136e9fce6af541e9d1e9
Game-specific and shared fixes are reproducible patches under `patches/`.

## recomp-ui

Copyright (c) 2026 Matthew Stanley. Licensed under [MIT](recomp-ui/LICENSE).
Upstream: https://github.com/mstan/recomp-ui
Pinned revision: 028fa5c238265090a6596d1256168bb0b69b0e60

## Supporting dependencies

Retain [PSXRecomp's attribution](psxrecomp/THIRD_PARTY_ATTRIBUTION.md) and the
licences supplied with SDL, fmt, toml11, ELFIO, rabbitizer, libchdr, compression
libraries and other dependencies. OpenBIOS comes from PCSX-Redux and includes
uC-sdk code; see [OpenBIOS.LICENSE](psxrecomp/bios/OpenBIOS.LICENSE). The framework
supplies this licensed replacement BIOS, not a Sony PlayStation BIOS.

## Game content

Ridge Racer Revolution, its game content and trademarks belong to their respective
owners. This is an unofficial project. No game discs, extracted game data,
generated game C or playable game builds are distributed in this source repository.
Supply your own supported USA disc. No rights to game content are granted.
