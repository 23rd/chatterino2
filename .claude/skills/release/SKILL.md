---
name: release
description: >-
  Cut a release of this Chatterino fork: fetch upstream, rebase our commits on top of it,
  force-push to origin, then move the `latest` tag, which is what makes CI build, publish
  the release and point the update server at it. Use when asked to release, ship, publish,
  or update the fork against upstream.
---

# Releasing this fork

A release is **moving one tag**: `latest`. There is no tag per version.

The version is the build timestamp, `YYYY.MMDD.HHMM` (e.g. `2026.809.1425`), worked out
by the `version` job in `.github/workflows/build.yml` and handed to every other job. So
nothing in the working tree, and no tag, ever has to be bumped.

## Before touching anything

```bash
git status --porcelain            # must be empty
git rev-parse --abbrev-ref HEAD   # expect master
git remote -v | grep -E 'origin|upstream'
```

- Dirty tree → stop and ask. Never stash on the user's behalf.
- The submodules `lib/libcommuni`, `lib/serialize`, `lib/twitch-eventsub-ws/lib/date` and
  `tools/crash-handler` are usually shown as modified, and the untracked `lib/qtkeychain/`
  and `lib/websocketpp/` alongside them. That is pre-existing; **never commit them** -
  doing so would roll libcommuni back to a revision without `IrcTagsRef` and break the
  build for everyone.
- Not on `master` → stop and ask which branch to release.

## 1. Rebase onto upstream

```bash
git fetch upstream
git log --oneline upstream/master..HEAD    # our stack
git rebase upstream/master
```

If the rebase stops on a conflict: **stop and hand it to the user.** Do not resolve it
with `-X theirs`/`-X ours` or `git rebase --skip` - every commit in this stack is a
deliberate behaviour change, and silently dropping one ships a broken build.

After a clean rebase, confirm the stack survived:

```bash
git log --oneline upstream/master..HEAD | wc -l   # same count as before
```

## 2. Push the branch

```bash
git push --force-with-lease origin master
```

Force is expected - the rebase rewrote the commits. `--force-with-lease` rather than
`--force` so a push from another machine isn't clobbered.

## 3. Move the tag

```bash
git tag -f latest
git push -f origin latest
```

That is the whole release. Push the branch *before* the tag, so the tagged commit already
exists on the remote.

## 4. Watch CI

```bash
gh run list --repo 23rd/chatterino2 --limit 5
gh run watch <run-id> --repo 23rd/chatterino2
```

Roughly 25 minutes; Windows is the slow one.

## What moving the tag sets in motion

1. **`version`** computes `YYYY.MMDD.HHMM` once and outputs it. Every build job takes
   `CHATTERINO_FORK_VERSION` from that one value, so all platforms in a release report the
   same version instead of whatever the clock said when each job started.
2. **Build type is forced to stable** for any tag push. This matters: nightly builds
   refuse to auto-update and open a browser instead, and the `latest` tag carries no
   version for the "Determine build type" step to recognise.
3. **`create-release`** rewrites the single `latest` release in place, with the version in
   its name and every platform's binaries attached.
4. **`deploy`** rsyncs the artifacts to the VPS and rewrites `version/<os>/{stable,beta}`
   to the new version, deriving the download URLs from the `UPDATE_URL` secret so they
   can't drift from what the client can actually reach. It refuses to publish if those
   URLs don't respond.

Clients pick it up on their next check and install it silently
(`src/singletons/UpdateInstaller.cpp`).

## Notes

- **The VPS keeps no history.** The artifact filenames carry no version, so every deploy
  overwrites the previous one, and `.pdb` symbols are stripped before upload. Together
  with the single rolling GitHub release this means old builds are not archived anywhere -
  a deliberate choice, but worth remembering before hunting for an old binary or trying to
  symbolicate a crash from a previous version.
- Local builds have no version handed to them, so they report `0.0.0` and are marked as
  development builds that never auto-update. That is the intended guard, not a bug.
- Releasing the same commit twice is fine: the version comes from the clock, not the
  commit, so the second one still gets a higher version and reaches clients.
