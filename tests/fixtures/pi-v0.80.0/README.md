# pi v0.80.0 reference fixtures

This directory is the compatibility-evidence root for pi-cpp.

## Canonical upstream baseline

```text
repository: https://github.com/earendil-works/pi
tag:        v0.80.0
commit:     f08e968c83d92bce5f5fd2f7f20ef37f8cf04a39
```

The exact commit is intentionally recorded in addition to the tag so fixture generation is reproducible even if a local clone has other refs or a different default branch.

## Compatibility rule

Behavior conflicts are resolved in this order:

```text
pi v0.80.0 observable behavior
    > explicit pi-cpp compatibility spec
    > Tau v0.4.1 implementation reference
```

Tau fixtures may be used for engineering regression only. They are not canonical compatibility evidence.

## Planned fixture groups

```text
pi-v0.80.0/
├── README.md
├── message/
├── events/
└── traces/
```

Each generated fixture set must record:

1. deterministic input;
2. upstream source path / harness entrypoint;
3. generation command;
4. exact baseline commit;
5. normalization rules;
6. expected stable fields.

## Normalization policy

Normalization is allow-list based. It may remove or canonicalize only nondeterministic fields such as generated ids, timestamps, durations, and temporary filesystem paths when those fields are not themselves under test.

The following must not be normalized away when relevant:

- event type and ordering;
- `contentIndex`;
- message/content fields;
- text/thinking deltas;
- tool-call id/name/arguments aggregation;
- `stopReason`;
- stable usage fields.

## Status

The baseline metadata is pinned. Real message/event traces and the minimal TypeScript reference harness are added incrementally in v0.0.2 T0/T3 as deterministic cases are selected.
