# Design

Use the shared separator contract; no application-local rendering helper. Boundary owners select
alignment. Layouts use implicit logical occupancy. Preserve unrelated work and raw dividers outside
this migration. See the umbrella [initiative](../../../../docs/initiatives/crisp-separator-joins/README.md).

Audio/Network popup separators now use implicit physical-pixel occupancy without caller DPR division;
Network fades and Audio tests use inherited opacity. Keyboard hints use the one-pixel default.
