# Course records

Each module has a `cards.json` file. Its `schema_version` is 1. `lessons` maps
lesson filename to its first-pass minutes. `cards` maps stable card ID to
`lesson`, `type`, `topic`, `objective` (the numbered module objective),
`minutes`, initial `box`, `once`, `source`, `concepts`, and `grading`. Grading
has a `mode` of `self`, `regex`, or `check`; automated modes carry the original
`accept` expression and, when present, the `reject` expression. `concepts`
maps lesson-and-concept IDs to their source links. Card minutes include the
reading and practice before that card, so they sum to the lesson estimate.

The core question always determines the card grade and schedule. A card's
optional Stretch prompt is discussed after core feedback; its StretchKey is an
ungraded extension and never changes the core grade, box, due date or streak.

`claims.json` keeps one JSON record per checked claim or story. Each record
has `lesson`, `where`, `claim`, `source_urls`, and `verification`; it preserves the
working verification notes from the lesson rewrite. The module manifests
remain short descriptions of scope, objectives and time.
