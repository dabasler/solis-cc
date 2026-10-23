# Git Release Workflow

The recommended approach is to keep qSOLIS v1.0.0 in the **same repository** as
the original plugin so the history, authorship, and project identity remain intact.
The legacy version can be preserved with a tag and optional maintenance branch.

## One-time legacy preservation

Before replacing the default branch with the modern port, tag the last legacy
commit. Adjust the tag name to match the version you historically used:

```bash
git checkout <last-legacy-commit>
git tag -a v0.9-legacy -m "Legacy Qt5-era qSOLIS before the 1.0 modernization"
git branch legacy-qsolis-v0.9
```

Push both if desired:

```bash
git push origin v0.9-legacy
git push origin legacy-qsolis-v0.9
```

## Publish v1.0.0

Bring the modern source onto the default branch, review the diff, then:

```bash
git add -A
git commit -m "Release qSOLIS 1.0.0"
git tag -a v1.0.0 -m "qSOLIS 1.0.0"
git push origin main
git push origin v1.0.0
```

If your default branch is `master`, use that instead of `main`.

## Version policy

Use semantic versioning for qSOLIS itself:

- `1.0.1`: bug fix with compatible result semantics;
- `1.1.0`: new backward-compatible features;
- `2.0.0`: intentionally breaking CLI/result-schema/scientific-semantics changes.

CloudCompare compatibility is separate from the qSOLIS version. Binary asset
names should encode the CloudCompare build they target.
