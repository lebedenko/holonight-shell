# Phase 7 Triage-Only Verification Cycle — DESIGN

| Field | Value |
|---|---|
| Document ID | poc-remediation-phase7/DESIGN.md |
| Input | SPEC.md (approved) |
| Predecessor cycle | `docs/sdd/poc-readiness-review/` — original 11-unit audit (89 Confirmed Findings, all fixed across Phases 0–6; 101 Investigation Targets, unaddressed until now) |
| Output | 11 triage docs (`triage/U-01.md`…`triage/U-11.md`) + `REPORT.md`, all under `docs/sdd/poc-remediation-phase7/` |
| Constraint | Read-only. Zero source changes (REQ-C-01). |

---

## 1. Components

Phase 7 has four moving parts, all documentation-producing, none code-touching:

1. **Baseline manifest (pre-flight, one-time)** — a single file enumerating every production file Phases 0–6 touched, computed once by the orchestrator (not by any per-unit agent) and handed to all 11 triage agents identically. Exists so REQ-F-11 ("flag code-changed items") is answered from ground truth instead of 11 independent, possibly-inconsistent greps.
2. **Per-unit triage agents (×11, U-01…U-11)** — the core of the cycle. Each agent owns exactly one unit: it re-reads that unit's Investigation Targets section, re-examines the current code at the cited locations, and writes one `triage/U-XX.md`. Matches REQ-F-08's "one review agent per unit."
3. **Format-normalization pass (post-agent, pre-synthesis)** — a lightweight, mechanical check (not a re-review) that every `triage/U-XX.md` verdict table matches the REQ-F-09 schema exactly, before the synthesis step reads them. Exists to absorb the 11-way inconsistency risk called out in §6.
4. **Synthesis step (REPORT.md)** — a single pass over all 11 completed triage docs that aggregates Confirmed items, ranks them by severity/effort mirroring the original REPORT.md §7 roadmap style, tallies verdict counts, and states the Phase 8 handoff. Matches REQ-F-10.

There is no fifth "code-change" component anywhere in this cycle — by design (REQ-C-01/REQ-C-02), the only artifacts produced are the baseline manifest (ephemeral, scratch) and the two output directories under `docs/sdd/poc-remediation-phase7/`.

---

## 2. Data Flow

```
                    ┌─────────────────────────────────────────┐
                    │  Pre-flight: baseline manifest           │
                    │  git diff --stat 0b28776..HEAD -- libs/ apps/ │
                    │  → docs/sdd/poc-remediation-phase7/       │
                    │    .manifest/phase0-6-changed-files.txt   │
                    └───────────────────┬───────────────────────┘
                                         │ (handed to every agent, identical)
                                         ▼
   For each unit U-XX independently (parallel, see §4):
   ┌────────────────────────────────────────────────────────────────────┐
   │ 1. Read docs/sdd/poc-readiness-review/units/U-XX-*.md                │
   │    → parse its Investigation Targets section using the unit's        │
   │      actual heading format (§3.3 — 3 known variants)                 │
   │ 2. For each item ID:                                                 │
   │    a. Re-read the cited file(s)/line(s) in current HEAD              │
   │    b. Cross-check citation against the baseline manifest —           │
   │       if the file appears there, `git log -p <commit> -- <file>`     │
   │       to see exactly what Phases 0–6 changed there                   │
   │    c. Re-derive an independent judgment (NOT copy the original       │
   │       60–79 score) → assign one of 4 verdicts + write the            │
   │       mandatory fields that verdict requires (SPEC.md taxonomy)      │
   │ 3. Emit one row per item into a verdict table                        │
   │ 4. Write docs/sdd/poc-remediation-phase7/triage/U-XX.md              │
   │    (table + methodology note, REQ-NF-01)                             │
   └───────────────────────────────┬────────────────────────────────────┘
                                    │ (all 11 done)
                                    ▼
   ┌────────────────────────────────────────────────────────────────────┐
   │ Format-normalization pass (orchestrator, mechanical, not a re-review)│
   │  - grep-verify all 101 IDs present exactly once across the 11 files  │
   │  - grep-verify only the 4 taxonomy strings appear as verdicts        │
   │  - verify table column headers match the schema verbatim             │
   │  - kick back to the offending agent for exactly the broken row(s)    │
   │    if either check fails (not a wholesale re-run)                    │
   └───────────────────────────────┬────────────────────────────────────┘
                                    ▼
   ┌────────────────────────────────────────────────────────────────────┐
   │ Synthesis (single pass, orchestrator or one dedicated agent)         │
   │  - read all 11 triage/U-XX.md                                       │
   │  - pull every Confirmed row → one flat list with Unit+ID carried     │
   │  - rank by severity/effort (mirrors original REPORT.md §7 rubric)    │
   │  - tally verdict counts per unit and overall                         │
   │  - write REPORT.md                                                   │
   └────────────────────────────────────────────────────────────────────┘
```

An Investigation Target's lifecycle, end to end:
`listed in U-XX-*.md's Investigation Targets section` → `parsed by the U-XX triage agent under that unit's specific heading format` → `re-examined against current HEAD + cross-checked against the baseline manifest` → `assigned exactly one verdict with that verdict's mandatory fields` → `written as one row in triage/U-XX.md` → *(if Confirmed)* `pulled verbatim into REPORT.md's ranked list, tagged with its Unit+ID so it remains traceable back to the row that justifies it`.

---

## 3. Interfaces / Formats

### 3.1 `triage/U-XX.md` — required structure

Each file has exactly two sections: a short methodology note (REQ-NF-01) and one verdict table.

```markdown
# U-XX Triage — <unit short name, copied from the source unit doc's title>

**Source**: docs/sdd/poc-readiness-review/units/U-XX-*.md (Investigation Targets section)
**Items in scope**: N (matches the count in SPEC.md §"Unit Breakdown" for this unit)
**Methodology**: <one sentence, e.g. "Static code review of all N cited locations against
current HEAD; cross-checked against docs/sdd/poc-remediation-phase7/.manifest/
phase0-6-changed-files.txt for Phase 0–6 overlap; no automated tool substituted,
matching the original review's methodology (REQ-NF-01).">

## Verdict Table

| Item ID | Title | Verdict | Confidence | Rationale / Checklist |
|---|---|---|---|---|
| I-01 | Disabled config entries lose fields | Confirmed | 88/100 (re-derived) | ... |
| I-02 | ... | Refuted | — | ... |
```

Column contract (REQ-F-09):
- **Item ID** — the *original* ID exactly as it appeared in the source unit doc (see §3.3 for how the 3 heading formats map to a single canonical ID string). Never renumbered, never reformatted with a unit prefix — `I-01` stays `I-01`, `I-001` stays `I-001`, `I-C1`/`I-Q1` stay as-is. This is what REQ-F-01's grep check keys on.
- **Title** — copied or lightly paraphrased from the source doc's heading text, kept short (one line, no line breaks) so the table stays scannable.
- **Verdict** — exactly one of the four taxonomy strings, case- and spelling-exact: `Confirmed`, `Refuted`, `Not-a-bug`, `Needs-Manual-Verification`. Nothing else may appear in this column (REQ-C-03).
- **Confidence** — a `NN/100` number for Confirmed items only (mandatory, ≥80, labeled "re-derived" per REQ-F-03); `—` for the other three verdicts (they don't carry a confidence score under the taxonomy — Refuted/Not-a-bug are binary determinations, Needs-Manual-Verification explicitly defers judgment).
- **Rationale / Checklist** — the verdict-specific mandatory content from SPEC.md's taxonomy (§"Verdict Taxonomy"). This column is allowed to be long — Markdown tables tolerate multi-sentence cells; when a Needs-Manual-Verification checklist needs actual numbered steps, render them as a `<br>1. …<br>2. …` sequence inside the cell (keeps one-row-per-item invariant intact, which the REQ-F-01 grep and REQ-F-02 verdict-count script both depend on) rather than breaking into a second table.

Every row's Rationale/Checklist cell additionally embeds, inline, the two other mandatory REQ-F-04/REQ-F-11 elements when applicable — there is no separate column for them, to keep the schema exactly 5 columns as REQ-F-09 specifies:
- For Confirmed: the current-code rationale *and* the explicit code-change note (REQ-F-04 point 2), e.g. "*Code-change note: original finding cited ConfigParsers.cpp:416; unchanged by Phases 0–6 (not in baseline manifest) — still exhibits the issue as originally described.*"
- For any verdict where the cited file **does** appear in the baseline manifest, a leading `[Phase N: <one-line summary of what changed]` tag (REQ-F-11) — see §3.1.1.

#### 3.1.1 REQ-F-11 flag format

When an item's cited file(s) appear in `.manifest/phase0-6-changed-files.txt`, the Rationale/Checklist cell must open with a bracketed tag identifying which of the 7 Phase commits (`d4f2310` Phase 0 … `cf76ba0` Phase 6) touched it and how that affects the finding:

```
[Phase 3: HyprlandIpcClient.cpp refactored for blocking-IO guard — original finding's
line numbers shifted (was :57, now :64); underlying issue unaddressed by that refactor]
Confirmed. Re-examined at HyprlandIpcClient.cpp:64 ...
```

The three allowed dispositions for this tag, per SPEC.md REQ-F-11 point 2, are literally the words **moot**, **partially addressed**, or **still applicable in new form** — one of these three must appear in the tag or the immediately following sentence, so the format-normalization pass and human spot-checks (REQ-F-11's acceptance criterion) can grep for them.

### 3.2 `REPORT.md` — required structure

Mirrors the original `poc-readiness-review/REPORT.md`'s tone and rubric (severity × effort, not confidence-only ranking), scoped down to synthesis-only (no architecture review section — that already exists and Phase 7 doesn't re-litigate it):

```markdown
# Phase 7 Triage Synthesis Report

| Field | Value |
|---|---|
| Document ID | poc-remediation-phase7/REPORT.md
| Inputs | 11 triage/U-XX.md documents
| Method | Per-unit re-examination against current HEAD (11 parallel read-only agents),
           cross-checked against Phases 0-6 commit history. Zero code changes (REQ-C-01).
| Status | Final — seeds Phase 8 remediation scope

## 1. Executive Summary
  <counts, headline Confirmed items, comparison to original 60-79 scores where notable>

## 2. Summary Tally  (REQ-F-10 point 4)
  | Unit | Confirmed | Refuted | Not-a-bug | Needs-Manual-Verification | Total |
  |---|---|---|---|---|---|
  | U-01 | .. | .. | .. | .. | 10 |
  ...
  | **Total** | .. | .. | .. | .. | **101** |

## 3. Confirmed Items — Ranked Remediation Candidates  (REQ-F-10 points 1-2)
  Ranked by Severity (High/Medium/Low, carried from the triage row) then Effort
  (S/M/L), same two-axis rubric the original REPORT.md §7 used — NOT by confidence
  score alone (confidence only gates *inclusion*, at >=80).

  ### High severity
  1. **U-XX [I-NN]** <title> (Confidence NN/100, Effort S) — <one-line why, pointer
     to triage/U-XX.md row for full rationale>
  ...
  ### Medium severity
  ...
  ### Low severity
  ...

## 4. Needs-Manual-Verification Roster
  Flat list of every such item across all 11 units, so a human running compositor
  checks has one place to start (not part of the Phase 8 code-change seed, but
  tracked since REQ-F-07 checklists are otherwise buried in 11 separate files).

## 5. Phase 8 Seed Statement  (REQ-F-10 point 3, explicit per SPEC.md)
  "This list [Section 3] seeds a future Phase 8 remediation cycle. No item in this
  report has been fixed; Phase 7 produced triage only, per REQ-C-01."

## 6. Methodology Notes
  <parenthetical: dispatch was parallel across all 11 units (see DESIGN.md §4);
  baseline manifest commit range 0b28776..cf76ba0>
```

Severity/effort values are **not re-derived** by the synthesis step. They are sourced from the original unit entry where present; where absent, the synthesis records a clearly marked inference in `REPORT.md` together with its rationale. These classifications are therefore auditable from the report, rather than assumed to exist in the five-column triage table (whose contract deliberately contains no Severity/Effort columns). The synthesis step's ordering follows the original REPORT.md §7 precedent: Critical/High first, corroborated-across-units items next even if individually Medium, then the rest.

### 3.3 Item-ID format normalization (cross-cutting concern for both file types)

The three heading formats observed in the source unit docs must all resolve to one clean `Item ID` cell value:

| Units | Source heading pattern | Canonical Item ID extracted |
|---|---|---|
| U-01 – U-08 | `#### [I-NN] Title` (bracketed, in a `####` heading) | `I-NN` (strip brackets/hash) |
| U-09, U-10 | `### I-NNN — Title (confidence)` (em-dash separator, `###` heading, 3-digit) | `I-NNN` (strip brackets: none present; keep as-is) |
| U-11 | `### I-CN` / `### I-QN — Title (confidence)` (two ID families: `C`=C++, `Q`=QML) | `I-CN` / `I-QN` (keep prefix — dropping it would collide `I-C1` and `I-Q1` into the same string) |

The triage agent for each unit is told explicitly which pattern its unit uses (see §4 dispatch brief) rather than asked to detect it — detection risk (regex false match, missed item) is exactly the kind of mechanical error the format-normalization pass exists to catch, so eliminating it at the brief level is cheaper than catching it downstream. No cross-unit renumbering is performed anywhere in Phase 7 — `REPORT.md` always cites `U-XX [I-NN]` (unit-qualified), so the fact that `I-01` recurs as an ID in ten different units is never ambiguous in context.

---

## 4. Dispatch Strategy

**All 11 per-unit triage agents are dispatched in parallel, in a single wave, not sequentially.**

This differs deliberately from the original readiness review's sequential-with-human-checkpoints pattern (`REVIEW-CHECKPOINTS.md` shows 11 individual approvals gating progress). The two workflows have different risk profiles:

- The original review was **first-contact analysis** — its findings became the historical record and directly seeded 7 remediation phases; a wrong or miscalibrated finding there would propagate silently through months of downstream work, so a human checkpoint after each unit let the reviewer course-correct the *next* unit's briefing before more work compounded the error (this is exactly what happened in U-11's "Ruled Out" section — a bad briefing was caught and fixed before entering the report).
- Phase 7 is **re-verification of already-scoped, already-bounded items** — 101 fixed IDs, no open-ended search space, no risk of one unit's miscalibration silently expanding into new false-positive categories in the next unit, because every agent's brief is the same fixed instruction set (§3.3's format table, the manifest, the taxonomy) regardless of which unit it's assigned. There's nothing for unit N's outcome to teach unit N+1's agent that isn't already in the shared brief.

Because this is read-only triage against a static baseline (current `HEAD`, frozen for the duration of the cycle — see risk note in §6 about mid-cycle commits), there is no merge-conflict or shared-mutable-state risk from parallelism: each agent reads its own unit's source doc and writes its own `triage/U-XX.md`, and no two agents' output files overlap. Parallel dispatch is a straightforward throughput win with no corresponding safety cost here, unlike the original review where sequential-with-checkpoints was buying something real (early error containment on a first-pass, high-fan-out analysis).

A single post-hoc checkpoint (the format-normalization pass, §1 component 3) replaces the original's 11 checkpoints, because the artifact being checked (a 5-column table with 4 allowed verdict strings) is mechanically verifiable — unlike the original review's open-ended "does this finding hold up" judgment calls, which genuinely needed a human in the loop after each unit.

### 4.1 What each agent receives

Every one of the 11 agents gets an identical-shape brief, differing only in the unit-specific fields:

1. **Fixed, shared across all 11:**
   - The four-verdict taxonomy and its mandatory-field requirements, verbatim from SPEC.md.
   - The `triage/U-XX.md` schema (§3.1) and the item-ID canonicalization table (§3.3).
   - The confidence-scoring guardrail (§5.2 below) — how to avoid rubber-stamping.
   - Path to the baseline manifest (`.manifest/phase0-6-changed-files.txt`) and the 7 Phase commit hashes/subjects, so it can run `git log -p <hash> -- <file>` itself for any of its unit's cited files that appear in the manifest.
   - Explicit reminder: read-only, `docs/sdd/poc-remediation-phase7/` output only, no code edits (REQ-C-01/REQ-C-02) — an agent that finds itself wanting to fix something writes that impulse into the Confirmed row's rationale instead.

2. **Unit-specific:**
   - Path to `docs/sdd/poc-readiness-review/units/U-XX-*.md` and which of the 3 heading formats (§3.3) it uses.
   - The exact item count expected for that unit (from SPEC.md's "Unit Breakdown": U-01:10 … U-11:11), so the agent can self-check it produced exactly that many rows before finishing.
   - The unit's source-file scope (copied from that unit doc's own "Scope" section) so the agent knows which `libs/`/`apps/` subtree to re-read.

### 4.2 Awareness of Phases 0–6 (REQ-F-11)

Agents do **not** independently reconstruct "what changed" via ad hoc `git log`/`git blame` exploration — that invites 11 different notions of "what counts as touched" and 11 different levels of thoroughness. Instead:

- The orchestrator computes the Phase 0–6 baseline manifest **once**, before dispatching any agent: `git diff --stat 0b28776..cf76ba0 -- libs/ apps/` (`0b28776` = the commit immediately preceding Phase 0's first commit `d4f2310`, i.e., the exact snapshot the original readiness review's findings were written against; `cf76ba0` = the Phase 6 remediation tip). This produces a flat list of ~134 changed production files, copied to `docs/sdd/poc-remediation-phase7/.manifest/phase0-6-changed-files.txt`. Any later production commits are separately checked against the frozen review baseline before publication, so the Phase 0–6 manifest is not incorrectly presented as a current-HEAD manifest.
- Every agent is hands the same file and the same 7 commit subjects (`d4f2310`…`cf76ba0`, "remediate phase 0..6 poc findings"). When an agent's cited location is in the manifest, it runs `git log -p <specific-phase-commit> -- <that file>` itself to see the actual diff before writing the REQ-F-11 tag — this keeps the *content* of "how did it change" agent-specific and accurate (grep-only manifest membership would be too coarse — a file can appear in the manifest for a change unrelated to the specific line the original finding cited), while keeping the *scope* of "did anything change here at all" identical and centrally computed.

---

## 5. Key Decisions & Rationale

### 5.1 Per-unit dispatch (not per-item, not monolithic)

- **Per-item** (101 separate agents) was rejected: most Investigation Targets are 5-15 lines of source-doc text with a 1-3 sentence rationale — dispatch overhead would dwarf the work, and an item-level agent has no natural way to self-check "did I cover the right count for my unit" (REQ-F-01's completeness check), which a unit-level agent gets for free by comparing its output table's row count to the unit's known target count.
- **Monolithic** (1 agent for all 101 items) was rejected: it re-introduces the exact sequential-single-context-window problem the original review split into 11 units to avoid — a single agent re-reading 101 items across 11 unrelated source trees (Wayland/D-Bus C++ in U-01 vs. QML animation timing in U-09) loses the unit-scoped focus that let the original review's agents reason deeply about one coherent subsystem at a time. It also produces one giant turn with no natural checkpoint for the format-normalization pass to react to partial output.
- **Per-unit** matches the existing document boundary (`U-XX-*.md` already exists, one per unit), matches REQ-F-08's literal wording, and lets each agent's unit-file-scope (already recorded in the source doc's own "Scope" section) double as its git-diff and re-read boundary — no new scoping work invented for Phase 7.

### 5.2 Operationalizing "fresh confidence score" (REQ-F-03) so it isn't rubber-stamped

The failure mode SPEC.md is guarding against: an agent reads "68/100" in the source doc, decides the finding still looks right, and writes "82/100 (re-derived)" without doing anything that would justify a *specific* number 14 points higher than the original. DESIGN.md addresses this with two concrete requirements baked into every agent's brief (not left as an unenforced instruction):

1. **The re-derivation must cite something the original write-up did not have**: either (a) a specific current-code observation absent from the original (e.g., a line-number shift, a sibling pattern comparison, a cross-reference the original review's confidence-capping — "capped at 10 per skill protocol," per U-01 — excluded), or (b) an explicit statement that re-reading confirms the *exact* original reasoning holds with no new information, in which case the confidence should land close to the original (not artificially inflated) — a Confirmed verdict requires ≥80, so an item whose fresh re-read produces no new evidence and whose original was, say, 65/100, is a strong candidate for **Needs-Manual-Verification** or a lower-confidence write-up that doesn't clear the Confirmed bar, not an inflated Confirmed.
2. **The score must decompose**, in the rationale cell, into the same two axes the original review used to explain its own numbers (visible throughout the unit docs' prose: "clear code citation" pushes scores up, "subtle reasoning or timing dependency" pushes them down — REQ-NF-02's explicit text). An agent writing a bare number with no such breakdown fails the format-normalization pass's spot-check criterion (mirrors REQ-F-03/REQ-NF-02's human-reviewer acceptance criteria, applied mechanically first).

This makes "why 88 and not 80 or 95" a required, checkable sentence rather than a number the agent could have picked without re-reading anything.

### 5.3 Guardrails against the four-verdict taxonomy becoming a dumping ground

The obvious failure mode: every item lands in **Needs-Manual-Verification**, because it's the only verdict that requires no confident judgment call — just a checklist. SPEC.md's REQ-NF-03 ("no guessed verdicts" for that bucket) already implicitly discourages *casual* use, but DESIGN.md adds an explicit decision rule to each agent's brief, applied in this fixed order, so "I'm not sure" isn't a legal reason to pick that bucket:

1. **Ask first: can this be resolved by reading code alone, with no compositor/runtime state involved?** If yes, it is never Needs-Manual-Verification — it must land in Confirmed, Refuted, or Not-a-bug based on the static re-read, exactly like the original review's Confirmed Findings (which included plenty of concurrency/lifecycle reasoning without live testing). This eliminates the largest share of potential misuse, since the majority of the 101 items (per the unit docs sampled: race conditions, missing validation, duplicated logic, dead code, unlogged failures) are statically resolvable.
2. **Needs-Manual-Verification is reserved for the specific classes SPEC.md's taxonomy names**: visual regressions, pointer/wheel/tap handler behavior, timing-sensitive UI interactions — i.e., exactly the category CLAUDE.md's own "Manual testing protocol" memory and its `WheelHandler`/`TapHandler` gotchas describe as *impossible to verify any other way in this environment* (no headless input simulation available here). An agent reaching for this verdict must name which of these classes the item belongs to in the rationale cell; "not sure" or "seems risky to call" alone is not a qualifying reason and should be rejected by a human reviewer sampling per REQ-F-07's acceptance criterion.
3. **Refuted and Not-a-bug are actively encouraged as legitimate, common outcomes**, not just Confirmed — the brief states explicitly that the original review's own docs already model this (U-09's "Ruled Out" sections, U-11's ColumnLayout-margin false-positive cluster) and that a Phase 7 agent finding zero Refuted/Not-a-bug items across a 7-11 item unit should treat that as a signal to re-check its own rigor, not a badge of thoroughness.

### 5.4 New `triage/` directory vs. updating unit docs in-place

SPEC.md already settles this (REQ-F-09 names the path explicitly), but the alternative was real and is worth recording: editing `docs/sdd/poc-readiness-review/units/U-XX-*.md` in place (e.g., appending verdicts under each Investigation Target) would keep everything about one item in one place, but was rejected for reasons the grill surfaced:
- The readiness-review documents are the closed, historical record of a completed cycle (`REVIEW-CHECKPOINTS.md` shows all 11 already ✅ Approved) — mutating them retroactively breaks their value as an audit trail of "what the reviewer knew and when."
- REQ-C-01/REQ-C-02 scope Phase 7's writes to its own directory; touching a different cycle's directory would blur that boundary and complicate the git-diff-based acceptance check (REQ-F-01's grep, the "zero changes outside `docs/sdd/poc-remediation-phase7/`" checklist item) — a reviewer would have to separately verify the *edits* to `poc-readiness-review/` are additive-only rather than simply confirming no files changed there at all.
- A separate `triage/` directory lets REPORT.md cite `triage/U-XX.md` as a self-contained, complete artifact per unit, matching the pattern the original REPORT.md itself used when citing the unit docs.

### 5.5 Ranking Confirmed items by severity/effort, not confidence

REQ-F-10 explicitly asks for the same rubric as the original REPORT.md's roadmap (§7 there), which sequences by severity/effort/architectural-dependency/blast-radius — not by raw confidence score. DESIGN.md carries this forward rather than introducing a confidence-sorted list, because confidence in this cycle measures "how sure are we this is real," which is orthogonal to "how much does fixing it matter" — a 95/100-confidence Low-severity dead-code removal should not rank above an 82/100-confidence High-severity silent-failure path. Confidence remains a *gate* (≥80 to even appear as Confirmed) but not a *sort key*.

---

## 6. Alternatives Considered

| Alternative | Why not chosen |
|---|---|
| Sequential dispatch with human checkpoints after each unit (mirroring the original review) | Rejected per §4 — Phase 7's fixed, bounded, static-baseline re-verification task has no analogous "miscalibration compounds forward" risk; sequential dispatch would trade ~11x wall-clock time for a safety property this task doesn't need. |
| Single mega-agent across all 101 items | Rejected per §5.1 — loses unit-scoped focus, no natural checkpoint boundary, and the original review's own unit split (justified by DESIGN.md's file-count/subsystem-coherence reasoning) already solved this problem once; no reason to re-solve it worse here. |
| In-place edits to `poc-readiness-review/units/U-XX-*.md` | Rejected per §5.4 — breaks the closed historical record, complicates the REQ-C-02 boundary check. SPEC.md already decided this; recorded here per the task's request to document it as a real alternative. |
| Per-agent independent `git log`/`git blame` exploration for REQ-F-11 (no shared manifest) | Rejected per §4.2 — 11 independently-scoped greps of the same commit range produce inconsistent "was this touched" judgments (different agents drawing the file-vs-line boundary differently); a single centrally-computed manifest removes that inconsistency while still letting each agent do its own line-level diff reading for the files that do match. |
| Confidence-sorted REPORT.md ranking | Rejected per §5.5 — confidence is a gate, not a priority signal; severity/effort matches REQ-F-10's explicit instruction to mirror the original roadmap's rubric. |
| Skip the format-normalization pass; rely solely on human REQ-F-01/F-02 spot-checks | Rejected — REQ-F-01's acceptance criterion is a *grep across all 11 files*, which is cheap to run mechanically the moment all agents finish, catching a missing/duplicate ID or a stray non-taxonomy verdict string before a human ever opens a file; deferring that entirely to manual spot-checking risks a malformed row surviving into REPORT.md's aggregation, where it's harder to trace back. |

---

## 7. Known Risks

1. **Stale line-number citations.** Code has moved since the original review (134 files changed across Phases 0–6); an agent re-citing a line number from the *source* unit doc without re-locating the actual current line risks citing the wrong code, or code that no longer exists at that offset. Mitigation: every agent brief requires re-reading the *current* file at the cited location before writing a verdict, and the REQ-F-11 tag format (§3.1.1) explicitly captures the old-vs-new line-number shift when the file is in the manifest, making the discrepancy visible rather than silently wrong. This does not fully eliminate the risk for files *not* in the manifest that nonetheless shifted for unrelated reasons (e.g. a neighboring function was reformatted) — a residual risk accepted as inherent to any point-in-time re-verification.
2. **Misjudging "already fixed by Phases 0–6."** An agent could see a file in the manifest and assume the *specific finding* was addressed, when the phase actually touched an unrelated part of the same file (or vice versa — assume "not in manifest" means "definitely untouched," when the manifest's `git diff --stat` file-level granularity can miss a Phase that touched the file but the diff since then reverted/rewrote it further). Mitigation: §4.2's requirement that agents run `git log -p` on the *specific* commit for files that do match, rather than trusting file-level presence alone; the three-way disposition requirement (moot/partially addressed/still applicable, §3.1.1) forces the agent to state its reasoning rather than silently assume.
3. **11-way inconsistent verdict-table formatting.** Even with an identical schema in every brief, natural-language agents drift (extra columns, verdict strings with trailing punctuation, checklist steps as prose instead of numbered lists). Mitigation: the dedicated format-normalization pass (§1 component 3, §4 closing paragraph) is a mechanical, non-judgmental check run once after all 11 agents finish, targeted at exactly this risk — cheaper and more reliable than asking each agent to self-police formatting under a general "be careful" instruction.
4. **Confidence re-scoring as unfounded numeric theater.** Covered in depth in §5.2 — the risk that "re-derived" becomes a rubber stamp. Mitigation is structural (decomposition requirement, explicit "new information or explicit reconfirmation" rule), not just a reminder, specifically because a reminder alone was judged insufficient by the original grill.
5. **Needs-Manual-Verification as an escape hatch.** Covered in §5.3. Residual risk: an agent genuinely uncertain about a borderline item (not clearly static-resolvable, not clearly a UI-timing issue) may still default there under ambiguity. Mitigation caps this at "acceptable if named and justified" — the decision-rule ordering in §5.3 makes the *default* be a static verdict, with Needs-Manual-Verification requiring an affirmative, named reason to select, inverting the ease-of-least-resistance direction.
6. **Review baseline goes stale mid-cycle.** If a production commit lands between the initial review and publication, the original static conclusions may no longer describe current HEAD. Mitigation: freeze and record the review commit, then run a post-baseline impact pass across every changed production file. Re-triage every item whose cited or supporting code changed and record the result under `.manifest/`; if no affected item changes verdict, publish that evidence alongside REPORT.md. REQ-C-01's git-diff-based acceptance check still independently confirms Phase 7 itself made no production-file changes.
7. **Item-count self-check false confidence.** §4.1's "expected count per unit" self-check catches an agent that skipped or double-counted an item, but not one that silently substituted a *wrong* item for a missing one while still hitting the target count (e.g., mis-parsing U-11's `I-C`/`I-Q` split and merging two distinct items into one row, then inventing an extra to compensate). Mitigation: the format-normalization pass's REQ-F-01 grep checks *ID* presence, not just row count, which catches this specific failure mode — a merged/invented ID either duplicates an existing one or fails to match any ID in the source doc, both of which the grep surfaces.
