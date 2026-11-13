# Example server files

Nothing here is needed: every gametype runs with no files at all, on its
built-in settings. These are starting points for a dedicated server.

| File | Copy to | |
|---|---|---|
| `server.cfg` | your game folder | the server: name, slots, the mode it starts in, admins, voting |
| `rulesets/ffa.cfg` (`duel`, `tdm`, `ctf`) | `rulesets/` in your game folder | every built-in setting of that gametype, commented out, and a map rotation of stock maps; uncomment what you change |
| `rulesets/ctf_instagib.cfg` | `rulesets/` | a variant: Instagib CTF, voted beside the four |
| `public-server.cfg` | nowhere: copy lines from it | settings public servers often change |

The game folder is `baseq2/` for a Windows server, or the mod's own folder
on Linux (docs/admin.md, "Installing"). The full guide is docs/admin.md.
