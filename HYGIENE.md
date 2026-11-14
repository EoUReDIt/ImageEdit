# Repo hygiene: cleaning up orphan branches

How to keep this repository tidy when several AI sessions each work on their
own branch. Written for a non-technical owner: the agent does the git work, the
owner answers the questions in section 3.

## 1. Why this matters

Every cloud session gets its own branch (`claude/<random-name>`). When a session
ends, its branch stays behind. Over time:

- Work gets split across branches nobody remembers (this already happened: the
  handoff lived on one branch while a new session started on another).
- A new session can't find the latest findings and starts from stale notes.
- It becomes unclear which branch is "the truth".

An **orphan branch** here means a branch whose session is finished and whose
work is either already merged, copied elsewhere, or abandoned.

## 2. Snapshot (2026-11-14)

| Branch | Commits not on `main` | Last commit | What it holds |
|---|---|---|---|
| `main` | — | Initial commit | README only |
| `claude/compassionate-fermi-1dzo7z` | 18 | Sprint 3: E5 result | **All project work**: handoff, sprints, ledger, roadmap, tools, results |
| `claude/sweet-knuth-4ql77j` | 2 | Concept review brief | `CONCEPT-REVIEW-BRIEF.md`, this file |

Nothing is orphaned yet, but nothing has reached `main` either. `main` is
effectively empty, so every new session starts with no context.

Refresh the snapshot with:

```bash
git fetch --prune origin
```

```bash
for b in $(git branch -r | grep -v HEAD); do echo "$b | $(git log -1 --format='%h %cs %s' $b) | ahead of main: $(git rev-list --count origin/main..$b)"; done
```

## 3. Questions for the owner (the agent asks these, one branch at a time)

The agent must not delete or merge anything until the owner answers. For each
branch, the agent first explains in plain words what the branch contains, then asks:

**A. Is this work still wanted?**
- Keep it (merge into `main`)
- Keep it as a record but stop working on it (leave it, or tag it)
- Throw it away (delete)

**B. Is it finished, or is a session still using it?**
- Finished
- Still active (leave alone)
- Not sure (agent checks the session list and the last commit date)

**C. Does anything on it exist nowhere else?**
The agent answers this with a check (section 4) and reports. The owner only
confirms: "Yes, it's safe to delete" or "No, keep it".

**D. Which branch is the single source of truth going forward?**
- `main` (recommended: every new session starts from it)
- A named working branch

**E. How should future sessions find the latest handoff?**
- Always on `main` (merge after each sprint)
- On the working branch named in `main`'s README

**F. How often should we tidy up?**
- After each sprint (recommended)
- Monthly
- Only when asked

**G. Before deleting, keep a backup tag?**
- Yes: tag it `archive/<branch-name>` first (recommended; costs nothing)
- No

## 4. Checks the agent runs before asking

For each branch, report:

1. **Ahead/behind `main`:** `git rev-list --left-right --count origin/main...origin/<branch>`
2. **Already merged?** `git branch -r --merged origin/main`
3. **Unique files:** `git diff --stat origin/main...origin/<branch>`
4. **Last activity:** `git log -1 --format=%cs origin/<branch>`
5. **Open pull request?** Check GitHub for a PR from that branch.
6. **Live session?** Check whether a running session is using that branch.
7. **Copies elsewhere?** E.g. the review brief summarises findings, but it is a
   summary, not a replacement for the original files.

## 5. Safe procedure (agent, after owner approval)

1. Tag first: `git tag archive/<branch> origin/<branch>` and `git push origin archive/<branch>`
2. Merge wanted work into `main` through a pull request (the owner approves the
   merge; the agent never merges on its own).
3. Delete only after the merge is confirmed, or after the owner says "throw it away":
   `git push origin --delete <branch>`
4. Update the handoff with where things now live.

Never: delete a branch with an open PR or a live session; rewrite history on
someone else's branch; delete without a tag unless the owner said no to G.

## 6. Recommendation for now

1. Merge `claude/compassionate-fermi-1dzo7z` into `main` through a PR, so the
   handoff and all findings live on `main`.
2. Merge `claude/sweet-knuth-4ql77j` too (brief + this file).
3. Tag both as `archive/...`, then delete them once merged.
4. Add one line to `README.md`: "Start here: HANDOFF.md".
5. Repeat the section 3 questions at the end of each sprint.
