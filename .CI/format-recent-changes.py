import calendar
from datetime import datetime, timezone
import os
import subprocess

def months_ago(when: datetime, months: int) -> datetime:
    """`when` shifted back by whole calendar months."""
    month = when.month - months
    year = when.year
    while month <= 0:
        month += 12
        year -= 1
    day = min(when.day, calendar.monthrange(year, month)[1])
    return when.replace(year=year, month=month, day=day)

def run_git_command(args: list[str]) -> str:
    p = subprocess.run(
        ["git", *args],
        cwd=os.path.dirname(os.path.realpath(__file__)),
        text=True,
        check=True,
        capture_output=True,
    )
    return p.stdout.strip()

def get_last_version_tag() -> str | None:
    try:
        tag = run_git_command([
            "describe",
            "--tags",
            "--abbrev=0",
            "--match",
            "v*"
        ])
    except subprocess.CalledProcessError:
        return None

    # Releases are built from a commit that *is* a version tag, and describe
    # happily returns that very tag - which would make the range empty and every
    # release note read "No changes since last release". Step back to the tag
    # before it so the body lists what this release actually contains.
    try:
        run_git_command([
            "describe",
            "--tags",
            "--exact-match",
            "--match",
            "v*"
        ])
    except subprocess.CalledProcessError:
        # HEAD isn't a release itself (a nightly), so the nearest tag is right.
        return tag

    try:
        return run_git_command([
            "describe",
            "--tags",
            "--abbrev=0",
            "--match",
            "v*",
            f"{tag}^"
        ])
    except subprocess.CalledProcessError:
        return None

# The fork's own commits are rebased on top of upstream for every single
# release, so they would otherwise fill each changelog with the same entries
# under a fresh date. What a release actually delivers is the upstream work.
# Matched against both the author name and the author email.
EXCLUDED_AUTHORS = {
    "dependabot[bot]",
    "23rd",
    "23rd@vivaldi.net",
}

def get_unreleased_commits():
    last_tag = get_last_version_tag()
    if last_tag:
        log_range = f"{last_tag}..HEAD"
    else:
        # No version tag at all - this fork releases by moving a single `latest`
        # tag, so there is nothing to diff against. Walk the whole history and
        # let the two-month window below do the bounding; a fixed count would
        # only ever return this fork's own commits, which are then filtered out
        # by author, leaving an empty changelog.
        log_range = "HEAD"
    limit = None
    args = [
        "log",
        log_range,
        "--pretty=format:%cI|%an|%ae|%s",
        "--no-merges",
    ]
    if limit:
        args.insert(1, f"-n{limit}")
    log_output = run_git_command(args)
    unreleased: list[tuple[datetime, str]] = []
    for line in log_output.splitlines():
        if not line.strip():
            continue
        date_str, author, email, subject = line.split("|", 3)
        if author.lower() in EXCLUDED_AUTHORS or email.lower() in EXCLUDED_AUTHORS:
            continue
        d = datetime.fromisoformat(date_str).astimezone(timezone.utc)
        content = f"- [{d.strftime('%Y-%m-%d')}] {subject}"
        unreleased.append((d, content))
    unreleased.sort(key=lambda it: it[0], reverse=True)
    return unreleased

# A release body only covers the last two months. Anything older has long since
# shipped in an earlier build and is just noise here.
MAX_AGE_MONTHS = 2

# Hard backstop on top of the age window. The body reaches
# ncipollo/release-action as an environment variable, and Linux caps a single env
# entry at MAX_ARG_STRLEN (128 KiB) - going over it makes execve fail with E2BIG
# ("Argument list too long") before the action even starts. GitHub separately
# rejects release bodies longer than 125k characters.
MAX_ENTRIES = 200

all_commits = get_unreleased_commits()

cutoff = months_ago(datetime.now(timezone.utc), MAX_AGE_MONTHS)
recent = [entry for entry in all_commits if entry[0] >= cutoff]
too_old_count = len(all_commits) - len(recent)

unreleased_lines = recent[:MAX_ENTRIES]
capped_count = len(recent) - len(unreleased_lines)

if len(unreleased_lines) == 0:
    print(f"No changes in the last {MAX_AGE_MONTHS} months.")

for _, line in unreleased_lines[:5]:
    print(line)

if len(unreleased_lines) > 5:
    print("<details><summary>More Changes</summary>\n")
    for _, line in unreleased_lines[5:]:
        print(line)
    print("</details>")

# Say what was left out rather than silently trimming.
omitted = []
if capped_count > 0:
    omitted.append(f"{capped_count} more from the last {MAX_AGE_MONTHS} months")
if too_old_count > 0:
    omitted.append(f"{too_old_count} older than {MAX_AGE_MONTHS} months")

if omitted:
    print(f"\n_...and {' and '.join(omitted)}, not listed._")
