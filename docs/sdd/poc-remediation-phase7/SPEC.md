# Phase 7 Triage-Only Verification Cycle — SPEC

**Cycle Goal:** Re-verify all 101 Investigation Targets from the Readiness Review (`docs/sdd/poc-readiness-review/`) against current codebase state. Assign each a final verdict (Confirmed, Refuted, Not-a-bug, or Needs-Manual-Verification) with fresh confidence scoring and rationale. Output triage documents per unit and a synthesis REPORT.md. **Zero code modifications.**

---

## Scope & Source

- **Source:** `docs/sdd/poc-readiness-review/units/U-01-foundational-core.md` through `U-11-settings.md`
- **Items:** 101 Investigation Targets with 60–79% confidence from the original review
- **Unit Breakdown:** U-01:10, U-02:10, U-03:10, U-04:10, U-05:8, U-06:10, U-07:10, U-08:8, U-09:7, U-10:7, U-11:11
- **Codebase Reference Point:** holonight-shell as of the current working tree (incorporating all Phases 0–6 remediation commits)

---

## Verdict Taxonomy

Four mutually-exclusive verdicts, each with mandatory requirements:

### 1. **Confirmed**
A re-examination of current code confirms the finding as a real issue worth fixing.
- **Mandatory:** Fresh confidence score (re-derived from current code, NOT copied from the original 60–79 write-up)
- **Mandatory:** Updated rationale explaining why current code still exhibits the issue
- **Mandatory:** Explicitly state any code changes since the original finding that affected this item

### 2. **Refuted**
Re-examination shows the finding is a false positive; the original reasoning no longer holds.
- **Mandatory:** Specific explanation of why the finding is invalid (e.g., "original assessment misread the dataflow", "the exact failure path cannot execute", "the race condition was already prevented by lock X")
- **Mandatory:** Cite the relevant code section or pattern that disproves the finding

### 3. **Not-a-bug**
Behavior is confirmed as intentional design, not a defect.
- **Mandatory:** Justification: why this behavior is acceptable/deliberate/correct
- **Mandatory:** Reference to design doc, code comment, or architectural rationale that explains the decision

### 4. **Needs-Manual-Verification**
The item requires interaction with a live Hyprland compositor session to verify (visual regressions, pointer/wheel/tap handler behavior, timing-sensitive UI interactions).
- **Mandatory:** Concrete, step-by-step manual verification checklist (not a guessed pass/fail)
- **Mandatory:** Prerequisite compositor state / UI configuration needed to run the checklist
- **Mandatory:** Expected behavior and failure modes to watch for
- Per project convention (CLAUDE.md, memory), this checklist defers final verdict to future human-in-loop execution; Phase 7 does NOT guess a pass/fail

---

## Functional Requirements

### REQ-F-01: Scope Completeness

**Ubiquitous:** The triage cycle shall process all 101 Investigation Targets, one per item ID, sourced from the 11 unit documents U-01 through U-11.

**Acceptance Criterion:** A grep across all 11 final triage documents (`triage/U-01.md` through `triage/U-11.md`) confirms all 101 item IDs from the source unit documents appear exactly once in a verdict table, with no missing ID and no duplicate ID.

---

### REQ-F-02: Four-Verdict Taxonomy Application

**Ubiquitous:** Each Investigation Target shall be assigned exactly one verdict from {Confirmed, Refuted, Not-a-bug, Needs-Manual-Verification}.

**Acceptance Criterion:** A script validates that every row in every triage document's verdict table contains exactly one of the four verdict strings; no item lacks a verdict, and no item has multiple verdicts.

---

### REQ-F-03: Confirmed Verdict — Fresh Confidence Scoring

**Event-driven:** When an item is assigned a Confirmed verdict, the triage agent shall derive a fresh confidence score from re-examination of the current code, independent of the original 60–79% score.

**Acceptance Criterion:** Every Confirmed item's triage entry displays a confidence score explicitly labeled "re-derived" or with a note indicating it is current-code-based; spot-check 5–10 Confirmed items across units to confirm the rationale explains how the fresh code review arrived at its score.

---

### REQ-F-04: Confirmed Verdict — Rationale & Code-Change Tracking

**Ubiquitous:** Each Confirmed item shall include:
1. Updated rationale explaining the current-code issue and why it persists
2. An explicit note of any code changes since the original finding that affected this item (e.g., "original finding pointed to method X; method X was refactored in Phase 5 fix Y but still exhibits the issue in its new form")

**Acceptance Criterion:** A human reviewer reads 5–10 Confirmed items and confirms that the rationale is specific to current code (not generic copy-paste from the original finding) and any prior changes are cited.

---

### REQ-F-05: Refuted Verdict — Explicit False-Positive Reasoning

**Ubiquitous:** Each Refuted item shall state the specific reason the original finding is invalid, with a code citation.

**Acceptance Criterion:** A human reviewer samples 3–5 Refuted items and confirms that the reasoning is concrete (e.g., "lock X prevents the race", "the error path is unreachable because check Y short-circuits it") and not a dismissal ("seems fine now").

---

### REQ-F-06: Not-a-bug Verdict — Intentional-Design Justification

**Ubiquitous:** Each Not-a-bug item shall cite the architectural rationale or design decision that makes the behavior intentional.

**Acceptance Criterion:** A human reviewer samples 3–5 Not-a-bug items and confirms each cites a comment, CLAUDE.md excerpt, memory note, or SDD doc explaining why the design is correct.

---

### REQ-F-07: Needs-Manual-Verification Verdict — Concrete Checklist

**Ubiquitous:** Each Needs-Manual-Verification item shall include:
1. A numbered, step-by-step checklist of manual actions (e.g., "1. Launch shell with `task run`", "2. Move mouse to screen corner", "3. Observe tooltip appearance")
2. Prerequisite compositor state or UI configuration required before starting
3. Expected behavior and specific failure modes to watch for

**Acceptance Criterion:** A human reviewer samples 3–5 Needs-Manual-Verification items and confirms each checklist is actionable without ambiguity (not "test to see if it looks right", but "observe Y under conditions Z and confirm X does/does not happen").

---

### REQ-F-08: Per-Unit Verification via Review Agents

**Event-driven:** When Phase 7 triage begins, the process shall deploy one review agent per unit (U-01 through U-11), each re-examining that unit's Investigation Targets against the current codebase.

**Acceptance Criterion:** A REPORT.md synthesis document lists the 11 units and confirms each agent completed its triage; per-unit agent logs (if retained) show execution timestamps and verdict tallies (e.g., "U-03: 4 Confirmed, 3 Refuted, 2 Not-a-bug, 1 Needs-Manual-Verification").

---

### REQ-F-09: Output File Structure — Per-Unit Triage Docs

**Ubiquitous:** The triage cycle shall produce 11 triage documents at `docs/sdd/poc-remediation-phase7/triage/U-01.md` through `triage/U-11.md`.

**Acceptance Criterion:** All 11 files exist at the specified paths; each file includes a verdict table with columns: Item ID, Title, Verdict, Confidence (where applicable), Rationale/Checklist.

---

### REQ-F-10: Output File Structure — Synthesis REPORT.md

**Ubiquitous:** The triage cycle shall produce a synthesis document at `docs/sdd/poc-remediation-phase7/REPORT.md` that:
1. Aggregates all Confirmed items across the 11 units
2. Ranks Confirmed items by severity/effort (using the same rubric as the original Phase 6 REPORT.md's remediation roadmap)
3. Explicitly notes that this list seeds a future Phase 8 remediation cycle
4. Includes a summary tally (Confirmed / Refuted / Not-a-bug / Needs-Manual-Verification counts)

**Acceptance Criterion:** The REPORT.md file exists; grep confirms it lists all Confirmed items; spot-check 3–5 items to confirm severity/effort ranking is justified by the rationale in the per-unit triage docs.

---

### REQ-F-11: Flag Code-Changed Items

**Ubiquitous:** Any Investigation Target whose underlying code was already modified by Phases 0–6 fixes shall be flagged in the triage doc with an explicit note identifying:
1. Which phase/PR touched the code
2. How the change affects the original finding (moot, partially addressed, or still applicable in new form)

**Acceptance Criterion:** A human reviewer compares the triage docs against the commit history of Phases 0–6 and confirms that items touching shared code sections have the required flags; spot-check at least 5 items.

---

## Non-Functional Requirements

### REQ-NF-01: Verification Method — Deep Code Analysis Per Unit

**Ubiquitous:** Each unit's triage shall employ the same review methodology as the original readiness review (comprehensive code reading, dataflow analysis, concurrency reasoning, memory-safety checks).

**Acceptance Criterion:** Each per-unit triage doc includes a brief methodology note (e.g., "Static code review of all XXX references in U-03 source files; dataflow analysis of YYY signal routing; no automated tool substituted"); comparison of triage rationales against originals shows similar analytical depth.

---

### REQ-NF-02: Confidence Scoring — Explicit Methodology

**Ubiquitous:** Each Confirmed item's confidence score shall be derived using the same scale as the original review (with 80/100 as the threshold for Confirmed; 60–79 as Investigation Targets).

**Acceptance Criterion:** Spot-check 5–10 Confirmed scores; confirm they are ≥80 and justified by the rationale (higher scores for items with clear code citations, lower scores for items with subtle reasoning or timing dependencies).

---

### REQ-NF-03: Needs-Manual-Verification — No Guessed Verdicts

**State-driven:** While an item requires live compositor interaction, the triage cycle shall NOT assign a tentative Confirmed/Refuted/Not-a-bug verdict based on static reasoning alone.

**Acceptance Criterion:** Items flagged as Needs-Manual-Verification do not include a guessed alternative verdict in parentheses or as a fallback; the checklist is the sole deliverable for those items.

---

## Constraint Requirements

### REQ-C-01: No Source Code Modifications

**Unwanted-behavior:** If a triage agent attempts to modify any source code file (.cpp, .h, .qml, CMakeLists.txt, or other production code), the cycle shall abort with an error and flag the violation.

**Acceptance Criterion:** Git diff at the end of Phase 7 contains zero changes to `apps/`, `libs/`, `protocols/`, `tests/`, or `src/` directories; only new/modified files under `docs/sdd/poc-remediation-phase7/` are present.

---

### REQ-C-02: Fixed Output Location

**Ubiquitous:** All triage output shall be written to `docs/sdd/poc-remediation-phase7/` only.

**Acceptance Criterion:** No triage files appear outside this directory; spot-check the directory tree to confirm.

---

### REQ-C-03: Four-Verdict Taxonomy is Exclusive

**Ubiquitous:** The triage cycle shall use the four-verdict taxonomy and no other framework (e.g., no "Partial", "Under Review", or "Deferred" verdicts outside the four named categories).

**Acceptance Criterion:** A regex search of all triage docs confirms only {Confirmed, Refuted, Not-a-bug, Needs-Manual-Verification} appear as verdict strings; no other verdict text is present.

---

## Acceptance Checklist (Phase 7 Complete)

- [ ] All 101 Investigation Targets from the 11 source units have been triaged
- [ ] 11 triage documents exist at `triage/U-01.md` through `triage/U-11.md`, each with a verdict table
- [ ] `REPORT.md` exists and aggregates Confirmed items with severity/effort ranking
- [ ] No item is missing a verdict; no item has multiple verdicts
- [ ] Every Confirmed item has a fresh confidence score (≥80) re-derived from current code
- [ ] Every Refuted item cites why the original finding is invalid
- [ ] Every Not-a-bug item cites architectural rationale
- [ ] Every Needs-Manual-Verification item includes a concrete, step-by-step checklist
- [ ] Items affected by Phases 0–6 code changes are flagged with phase/PR reference
- [ ] Git diff shows zero changes to production code (`apps/`, `libs/`, `protocols/`, `tests/`, `src/`)
- [ ] Only documentation files under `docs/sdd/poc-remediation-phase7/` have been created/modified

---

## Glossary

- **Investigation Target:** An item flagged in the Readiness Review with 60–79% confidence (requiring deeper verification before any code change)
- **Confirmed Item:** An Investigation Target re-verified as a real issue in current code (≥80% confidence); candidate for Phase 8 remediation
- **Triage:** The process of re-examining and assigning verdicts; produces no code changes, only documentation
- **Readiness Review:** The original audit cycle (`docs/sdd/poc-readiness-review/`), predating remediation Phases 0–6, that reviewed all 361 source files and produced 89 Confirmed Findings (fixed across Phases 0–6) plus the 101 Investigation Targets triaged in this Phase 7 cycle
- **Verdict:** The final classification (Confirmed, Refuted, Not-a-bug, Needs-Manual-Verification) assigned to each item
- **Phases 0–6 Remediation:** Earlier fix cycles that addressed the Readiness Review's Confirmed Findings; code may have changed in ways affecting Phase 7 Investigation Targets
