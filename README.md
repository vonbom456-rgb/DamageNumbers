# DamageNumbers v1.0 (ASE / ArkApi 3.56)

Server-side damage display plugin for ARK: Survival Evolved.

Features:
- green dealt damage
- red incoming damage
- players, dinos and structures
- large on-screen notifications
- thousands separators
- `/damage on|off`
- `DamageNumbers.Reload`

Install:
`ShooterGame/Binaries/Win64/ArkApi/Plugins/DamageNumbers/`
with `DamageNumbers.dll`, `config.json`, `PluginInfo.json`.

Important:
This v1.0 uses Ark's server notification RPC, so the exact screen position is controlled by the ASE client.
It reproduces the large overlapping-number style, but exact pixel placement like a custom client HUD cannot be guaranteed without a client mod.
