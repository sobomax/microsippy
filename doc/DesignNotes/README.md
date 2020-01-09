# Design Notes

## Message Memory

Winking allocation scheme is employed to dynamically allocate structures
that are emitted during parsing process. All dynamic memory requirements of
the parser are fulfilled using pre-allocated array that is co-located with
on-wire message buffer and deallocated in one shot.

## Fast Header-Field Names Parser

```
[On-wire HF Name] -=7 bit=-> [De-capitalization / Bit-compaction] -=5 bit=->
  [Pre-computed Pearson Hash using 64-entries array] -=Canonical Header Id=->
  [Verification using strcasencmp()].
```
