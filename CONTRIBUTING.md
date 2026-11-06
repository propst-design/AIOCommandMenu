# Contributing

Bug reports, reproducible test cases, documentation, scenarios and focused code
changes are welcome. Before starting a feature, check the pinned **Roadmap** issue
and the open issues labelled `in progress`: those are already being worked on. To take
something on, comment on its issue (or open one) so it can be assigned to you, and for a
substantial feature discuss it there or in [Discord](https://discord.gg/WK8xGSztB) first.
Push work in progress early as a draft pull request so others can see it.
Report bugs and ideas with the GitHub **Bug report** / **Feature request** templates
or in Discord (#issues; see the README's "Reporting bugs").

## Development and releases

`main` is the shared development branch. It can include unfinished or unverified
changes ahead of Workshop. The README identifies the published Workshop version;
[CHANGELOG.md](CHANGELOG.md) separates unreleased work from released changes.
A source push does not publish a Workshop update. Official Workshop publication
is handled by the maintainer.

## Get your own working copy

Install Git, Arma Reforger and **Arma Reforger Tools** (Workbench) through Steam.
Use matching stable versions of the game and Tools (Reforger 1.9 at present).
Python 3 is used for source checks.
Fork [this repository](https://github.com/propst-design/AIOCommandMenu) on GitHub, then run
these commands from your Workbench `addons` directory (replace `YOUR-USERNAME`):

```powershell
git clone https://github.com/YOUR-USERNAME/AIOCommandMenu.git AIOCommandMenu
cd AIOCommandMenu
git remote add upstream https://github.com/propst-design/AIOCommandMenu.git
git switch -c fix-my-change
```

Keep the addon folder name shown above. Open Workbench Launcher, add the base-game
project if it is missing, then use **Add Existing Project** to register the cloned
`addon.gproj`. See Bohemia's
[project setup guide](https://community.bistudio.com/wiki/Arma_Reforger:Mod_Project_Setup).
Avoid having multiple copies with the same addon GUID in an active addon search
path. Confirm your test loads your source copy rather than an installed Workshop
copy. Preserve existing GUIDs and commit metadata for any new resources.

AiO is standalone and requires only the base game. Add its `addon.gproj` to
Workbench, open it and let Workbench build the local resource database.
Run `Play-AIO-GameMaster.cmd` for a local-source Game Master session. Possess a
soldier in an AI group to exercise the menu; the free Game Master camera has no
player squad. You do not need ACA or BBB for standalone development.

Relevant source: `Scripts/Game/AIO` (server tests are the `AIO_*Test.c` files there);
input bindings: `Configs/System`. The menu layout is `AIO_MenuTree.Build()` and the
order list is `AIO_EOrder`, both in `AIO_Orders.c`. Use the README for the menu's
player controls.
For optional integration, clone ACA and its BBB dependency as sibling folders;
`Play-AIO-with-ACA.cmd` then loads the local combination.

## Keep your work independent

Your branch and local files stay unchanged when the maintainer pushes to `main`.
Bring newer upstream changes in deliberately, after committing your own work:

```powershell
git fetch upstream
git merge upstream/main
```

Resolve any conflicts and retest. Do not force-push the shared `main`. For each
new task, start a fresh branch from an updated main:

```powershell
git switch main
git pull --ff-only upstream main
git switch -c another-fix
```

## Implement and test

`AIO_EOrder` values travel over RPC: append new values; never reorder or remove
existing ones. Validate group ownership and permissions on the server. Test
selected men, whole squads, mounted units, possession and order replacement.
AiO remains standalone: it must work in Conflict, Game Master and Arma
Commander: Actual without depending on any mod; talk to other mods only at
runtime. AiO is GPLv3: do not copy code from APL-SA mods (such as Arma
Commander: Actual or Better Battle Buddies), from APL-ND mods, or from any
other mod with an incompatible license.

Preserve resource GUIDs, license notices and existing line endings. Keep changes
focused and avoid unrelated formatting. Enforce Script must compile in Workbench;
be careful with engine API signatures and avoid unsupported ternary expressions.
After changing a `.conf` input file, let Workbench rebuild the resource database
before testing, or the new binding won't register.

Two Enforce Script traps bite often (CI checks both):

- **Ints win comparisons and division.** With an int on the left, `<`, `>=` and
  friends compare as ints (`6 >= 6.75` is true), and int / int divides as ints.
  Put the float first. `python tools/check_enforce_numbers.py` flags both.
- **The static-initializer budget is shared by every loaded mod.** A class-level
  static with a built value (`= {}`, `= new ...`, `= Foo()`) or a `const` inside a
  function adds to one capped engine function; a big mod list goes over the cap
  and the Game module stops compiling for players. Declare such statics bare and fill
  them in the class's `AIO_InitStatics()`, called from `AIO_StaticInit.Run()`
  (`AIO_StaticInit.c`) at game creation; move local consts to class level as
  `static const`. Plain literals (`static const float X = 2.5;`) cost nothing.
  `python tools/check_static_init.py` flags new ones.

Install **Arma Reforger Server** through Steam and run
`Start-AIO-Test-Launcher.cmd`. From a Git Bash window at the repository
root, request a relevant preset (listed in `tools/test-presets.txt`):

```bash
bash tools/runtest.sh "contrib-$(date +%Y%m%d-%H%M%S)" tasks 60
```

The helper prints diagnostic output; inspect the `[AIO] TEST ... PASS|FAIL` lines and `[AIO] TEST COMPLETE`
and check for failures rather than treating its exit code as acceptance. Choose the preset for the behavior you changed, such as `vehicles`, `supply` or
`leadership`; `compile` just boots a server with AiO to check that it compiles.
Integration presets ending in `-aca` require ACA and BBB sibling checkouts. The
test server uses port 2001: the launcher waits for another dedicated server to
close, and a game you host yourself must be closed first. Also exercise the menu in game: server tests
alone cannot prove keyboard navigation, feedback or joined-client behavior.

For multiplayer changes, include a joined-client check when possible and state
what remains untested. Distinguish compilation, test completion, physical gameplay
results and multiplayer acceptance. A blocked test is not a passing test.

Before submitting, stage only the intended files and check them:

```powershell
git diff --cached --stat
python tools/check_public_source.py
python tools/check_enforce_numbers.py
python tools/check_static_init.py
git diff --cached --check
```

The three checks also run on GitHub pull requests and pushes to `main`. The
public-source check catches common private/generated files and recognizable
sensitive text; none of them replaces a manual review or compiles/tests the game.

## Submit a pull request

Commit your focused change, push your work branch to your fork, then open a pull
request with **base: `propst-design/AIOCommandMenu`, branch: `main`**. Include the problem,
resulting behavior, reproduction steps, source commits, game/mod versions, tests
and remaining limitations. Draft pull requests are welcome for work in progress.
For paired changes in another mod, link the other pull request. The maintainer
reviews and merges accepted changes; no direct write access is required.
See [GitHub's contribution guide](https://docs.github.com/en/get-started/exploring-projects-on-github/contributing-to-a-project).

## Public files and licensing

Submit source, resource metadata, tests and useful contributor/player docs.
Keep generated caches, logs, local settings, credentials, personal paths and private
project records out of commits. Review staged file names and content before pushing;
sanitize any diagnostic excerpts you share. Do not commit extracted base-game or
third-party mod assets. By submitting a contribution, you agree to distribute it
under this project's existing [license](LICENSE.md); retain required attribution.
