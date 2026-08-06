# SDD Tasks — poc-remediation-phase7

## Component 1: Baseline Manifest (Pre-flight)

- [x] T-001: Compute Phases 0–6 baseline manifest of changed files
  - REQs: REQ-F-11, REQ-C-01, REQ-C-02
  - Check: `docs/sdd/poc-remediation-phase7/.manifest/phase0-6-changed-files.txt` exists; contains output of `git diff --stat 0b28776..cf76ba0 -- libs/ apps/`; includes a header comment listing the 7 Phase commit hashes (d4f2310 through cf76ba0) and their subjects; file is readable by all 11 triage agents before they begin work.

---

## Component 2: Per-Unit Triage Agents (×11, parallel dispatch)

**Note:** The following 11 tasks (T-002 through T-012) represent independent per-unit triage agents and may be executed in parallel per DESIGN.md §4. None depend on another's output — each agent writes its own `triage/U-XX.md` and does not read any other unit's triage doc.

### Unit U-01

- [x] T-002: Triage U-01 Foundational Core (10 items)
  - REQs: REQ-F-01, REQ-F-02, REQ-F-03, REQ-F-04, REQ-F-05, REQ-F-06, REQ-F-07, REQ-F-08, REQ-F-09, REQ-F-11, REQ-NF-01, REQ-NF-02, REQ-NF-03, REQ-C-01, REQ-C-02, REQ-C-03
  - Check: `docs/sdd/poc-remediation-phase7/triage/U-01.md` exists; contains a verdict table with exactly 10 rows (one per item ID I-01 through I-10), each with a non-empty Verdict cell containing exactly one of {Confirmed, Refuted, Not-a-bug, Needs-Manual-Verification}; each Confirmed row includes a confidence score ≥80 labeled as re-derived; each Refuted/Not-a-bug/Needs-Manual-Verification row includes verdict-specific rationale per SPEC.md taxonomy; methodology note cites static code review matching REQ-NF-01; items affected by Phases 0–6 changes are flagged with [Phase N: …] tag per DESIGN.md §3.1.1 using the baseline manifest.

### Unit U-02

- [x] T-003: Triage U-02 Presentation Surfaces & Shell Composition Root (10 items)
  - REQs: REQ-F-01, REQ-F-02, REQ-F-03, REQ-F-04, REQ-F-05, REQ-F-06, REQ-F-07, REQ-F-08, REQ-F-09, REQ-F-11, REQ-NF-01, REQ-NF-02, REQ-NF-03, REQ-C-01, REQ-C-02, REQ-C-03
  - Check: `docs/sdd/poc-remediation-phase7/triage/U-02.md` exists; contains a verdict table with exactly 10 rows (one per item ID I-01 through I-10), each with a non-empty Verdict cell containing exactly one of {Confirmed, Refuted, Not-a-bug, Needs-Manual-Verification}; each Confirmed row includes a confidence score ≥80 labeled as re-derived; each Refuted/Not-a-bug/Needs-Manual-Verification row includes verdict-specific rationale per SPEC.md taxonomy; methodology note cites static code review matching REQ-NF-01; items affected by Phases 0–6 changes are flagged with [Phase N: …] tag per DESIGN.md §3.1.1.

### Unit U-03

- [x] T-004: Triage U-03 Power, Idle & Brightness Management (10 items)
  - REQs: REQ-F-01, REQ-F-02, REQ-F-03, REQ-F-04, REQ-F-05, REQ-F-06, REQ-F-07, REQ-F-08, REQ-F-09, REQ-F-11, REQ-NF-01, REQ-NF-02, REQ-NF-03, REQ-C-01, REQ-C-02, REQ-C-03
  - Check: `docs/sdd/poc-remediation-phase7/triage/U-03.md` exists; contains a verdict table with exactly 10 rows (one per item ID I-01 through I-10), each with a non-empty Verdict cell containing exactly one of {Confirmed, Refuted, Not-a-bug, Needs-Manual-Verification}; each Confirmed row includes a confidence score ≥80 labeled as re-derived; each Refuted/Not-a-bug/Needs-Manual-Verification row includes verdict-specific rationale per SPEC.md taxonomy; methodology note cites static code review matching REQ-NF-01; items affected by Phases 0–6 changes are flagged with [Phase N: …] tag per DESIGN.md §3.1.1.

### Unit U-04

- [x] T-005: Triage U-04 Session, Window Identity, Network & Audio Services (10 items)
  - REQs: REQ-F-01, REQ-F-02, REQ-F-03, REQ-F-04, REQ-F-05, REQ-F-06, REQ-F-07, REQ-F-08, REQ-F-09, REQ-F-11, REQ-NF-01, REQ-NF-02, REQ-NF-03, REQ-C-01, REQ-C-02, REQ-C-03
  - Check: `docs/sdd/poc-remediation-phase7/triage/U-04.md` exists; contains a verdict table with exactly 10 rows (one per item ID I-01 through I-10), each with a non-empty Verdict cell containing exactly one of {Confirmed, Refuted, Not-a-bug, Needs-Manual-Verification}; each Confirmed row includes a confidence score ≥80 labeled as re-derived; each Refuted/Not-a-bug/Needs-Manual-Verification row includes verdict-specific rationale per SPEC.md taxonomy; methodology note cites static code review matching REQ-NF-01; items affected by Phases 0–6 changes are flagged with [Phase N: …] tag per DESIGN.md §3.1.1.

### Unit U-05

- [x] T-006: Triage U-05 Desktop Integration, Appearance & Portal (8 items)
  - REQs: REQ-F-01, REQ-F-02, REQ-F-03, REQ-F-04, REQ-F-05, REQ-F-06, REQ-F-07, REQ-F-08, REQ-F-09, REQ-F-11, REQ-NF-01, REQ-NF-02, REQ-NF-03, REQ-C-01, REQ-C-02, REQ-C-03
  - Check: `docs/sdd/poc-remediation-phase7/triage/U-05.md` exists; contains a verdict table with exactly 8 rows (one per item ID I-01 through I-08), each with a non-empty Verdict cell containing exactly one of {Confirmed, Refuted, Not-a-bug, Needs-Manual-Verification}; each Confirmed row includes a confidence score ≥80 labeled as re-derived; each Refuted/Not-a-bug/Needs-Manual-Verification row includes verdict-specific rationale per SPEC.md taxonomy; methodology note cites static code review matching REQ-NF-01; items affected by Phases 0–6 changes are flagged with [Phase N: …] tag per DESIGN.md §3.1.1.

### Unit U-06

- [x] T-007: Triage U-06 Launcher & Notifications Services (10 items)
  - REQs: REQ-F-01, REQ-F-02, REQ-F-03, REQ-F-04, REQ-F-05, REQ-F-06, REQ-F-07, REQ-F-08, REQ-F-09, REQ-F-11, REQ-NF-01, REQ-NF-02, REQ-NF-03, REQ-C-01, REQ-C-02, REQ-C-03
  - Check: `docs/sdd/poc-remediation-phase7/triage/U-06.md` exists; contains a verdict table with exactly 10 rows (one per item ID I-01 through I-10), each with a non-empty Verdict cell containing exactly one of {Confirmed, Refuted, Not-a-bug, Needs-Manual-Verification}; each Confirmed row includes a confidence score ≥80 labeled as re-derived; each Refuted/Not-a-bug/Needs-Manual-Verification row includes verdict-specific rationale per SPEC.md taxonomy; methodology note cites static code review matching REQ-NF-01; items affected by Phases 0–6 changes are flagged with [Phase N: …] tag per DESIGN.md §3.1.1.

### Unit U-07

- [x] T-008: Triage U-07 Calendar & Weather Services (10 items)
  - REQs: REQ-F-01, REQ-F-02, REQ-F-03, REQ-F-04, REQ-F-05, REQ-F-06, REQ-F-07, REQ-F-08, REQ-F-09, REQ-F-11, REQ-NF-01, REQ-NF-02, REQ-NF-03, REQ-C-01, REQ-C-02, REQ-C-03
  - Check: `docs/sdd/poc-remediation-phase7/triage/U-07.md` exists; contains a verdict table with exactly 10 rows (one per item ID I-01 through I-10), each with a non-empty Verdict cell containing exactly one of {Confirmed, Refuted, Not-a-bug, Needs-Manual-Verification}; each Confirmed row includes a confidence score ≥80 labeled as re-derived; each Refuted/Not-a-bug/Needs-Manual-Verification row includes verdict-specific rationale per SPEC.md taxonomy; methodology note cites static code review matching REQ-NF-01; items affected by Phases 0–6 changes are flagged with [Phase N: …] tag per DESIGN.md §3.1.1.

### Unit U-08

- [x] T-009: Triage U-08 Topbar QML (8 items)
  - REQs: REQ-F-01, REQ-F-02, REQ-F-03, REQ-F-04, REQ-F-05, REQ-F-06, REQ-F-07, REQ-F-08, REQ-F-09, REQ-F-11, REQ-NF-01, REQ-NF-02, REQ-NF-03, REQ-C-01, REQ-C-02, REQ-C-03
  - Check: `docs/sdd/poc-remediation-phase7/triage/U-08.md` exists; contains a verdict table with exactly 8 rows (one per item ID I-01 through I-08), each with a non-empty Verdict cell containing exactly one of {Confirmed, Refuted, Not-a-bug, Needs-Manual-Verification}; each Confirmed row includes a confidence score ≥80 labeled as re-derived; each Refuted/Not-a-bug/Needs-Manual-Verification row includes verdict-specific rationale per SPEC.md taxonomy; methodology note cites static code review matching REQ-NF-01; items affected by Phases 0–6 changes are flagged with [Phase N: …] tag per DESIGN.md §3.1.1.

### Unit U-09

- [x] T-010: Triage U-09 RightSidebar + Popups QML (7 items)
  - REQs: REQ-F-01, REQ-F-02, REQ-F-03, REQ-F-04, REQ-F-05, REQ-F-06, REQ-F-07, REQ-F-08, REQ-F-09, REQ-F-11, REQ-NF-01, REQ-NF-02, REQ-NF-03, REQ-C-01, REQ-C-02, REQ-C-03
  - Check: `docs/sdd/poc-remediation-phase7/triage/U-09.md` exists; contains a verdict table with exactly 7 rows (one per item ID I-001 through I-007, using the unit's 3-digit ID format per DESIGN.md §3.3), each with a non-empty Verdict cell containing exactly one of {Confirmed, Refuted, Not-a-bug, Needs-Manual-Verification}; each Confirmed row includes a confidence score ≥80 labeled as re-derived; each Refuted/Not-a-bug/Needs-Manual-Verification row includes verdict-specific rationale per SPEC.md taxonomy; methodology note cites static code review matching REQ-NF-01; items affected by Phases 0–6 changes are flagged with [Phase N: …] tag per DESIGN.md §3.1.1.

### Unit U-10

- [x] T-011: Triage U-10 Overlay Content & Shared QML Building Blocks (7 items)
  - REQs: REQ-F-01, REQ-F-02, REQ-F-03, REQ-F-04, REQ-F-05, REQ-F-06, REQ-F-07, REQ-F-08, REQ-F-09, REQ-F-11, REQ-NF-01, REQ-NF-02, REQ-NF-03, REQ-C-01, REQ-C-02, REQ-C-03
  - Check: `docs/sdd/poc-remediation-phase7/triage/U-10.md` exists; contains a verdict table with exactly 7 rows (one per item ID I-001 through I-007, using the unit's 3-digit ID format per DESIGN.md §3.3), each with a non-empty Verdict cell containing exactly one of {Confirmed, Refuted, Not-a-bug, Needs-Manual-Verification}; each Confirmed row includes a confidence score ≥80 labeled as re-derived; each Refuted/Not-a-bug/Needs-Manual-Verification row includes verdict-specific rationale per SPEC.md taxonomy; methodology note cites static code review matching REQ-NF-01; items affected by Phases 0–6 changes are flagged with [Phase N: …] tag per DESIGN.md §3.1.1.

### Unit U-11

- [x] T-012: Triage U-11 Settings & Configuration (11 items)
  - REQs: REQ-F-01, REQ-F-02, REQ-F-03, REQ-F-04, REQ-F-05, REQ-F-06, REQ-F-07, REQ-F-08, REQ-F-09, REQ-F-11, REQ-NF-01, REQ-NF-02, REQ-NF-03, REQ-C-01, REQ-C-02, REQ-C-03
  - Check: `docs/sdd/poc-remediation-phase7/triage/U-11.md` exists; contains a verdict table with exactly 11 rows (one per item ID drawn from the C++ (I-C*) and QML (I-Q*) families per DESIGN.md §3.3), each with a non-empty Verdict cell containing exactly one of {Confirmed, Refuted, Not-a-bug, Needs-Manual-Verification}; each Confirmed row includes a confidence score ≥80 labeled as re-derived; each Refuted/Not-a-bug/Needs-Manual-Verification row includes verdict-specific rationale per SPEC.md taxonomy; methodology note cites static code review matching REQ-NF-01; items affected by Phases 0–6 changes are flagged with [Phase N: …] tag per DESIGN.md §3.1.1.

---

## Component 3: Format-Normalization Pass

- [x] T-013: Validate triage document schema and ID completeness
  - REQs: REQ-F-01, REQ-F-02, REQ-F-09, REQ-C-03
  - Check: A mechanized grep pass verifies (1) all 101 item IDs from the source unit docs appear exactly once across the 11 triage files (zero missing, zero duplicate; IDs canonicalized per DESIGN.md §3.3: U-01–U-08 as [I-NN], U-09–U-10 as I-NNN, U-11 as I-CN/I-QN); (2) every row's Verdict column contains exactly one of {Confirmed, Refuted, Not-a-bug, Needs-Manual-Verification} with no trailing punctuation or variations; (3) every triage file has exactly 5 column headers matching `| Item ID | Title | Verdict | Confidence | Rationale / Checklist |` verbatim; (4) every Confirmed row has a Confidence cell with `NN/100 (re-derived)` format where NN ≥ 80; (5) Refuted/Not-a-bug/Needs-Manual-Verification rows have `—` in Confidence cell; if any check fails, output identifies the specific offending row(s) for agent remediation rather than a wholesale re-run.

---

## Component 4: Synthesis

- [x] T-014: Generate REPORT.md aggregating Confirmed items and verdict tally
  - REQs: REQ-F-01, REQ-F-10, REQ-F-11
  - Check: `docs/sdd/poc-remediation-phase7/REPORT.md` exists; Section 2 "Summary Tally" contains a verdict-count table with one row per unit (U-01 through U-11) plus a Total row, columns Confirmed/Refuted/Not-a-bug/Needs-Manual-Verification each containing the numeric count for that unit; Section 3 "Confirmed Items — Ranked Remediation Candidates" lists every Confirmed item across all 11 units grouped by Severity (High/Medium/Low) then sub-ranked by Effort (S/M/L), with each entry citing `U-XX [I-NN]` format and a one-line pointer to `triage/U-XX.md`; Section 4 "Needs-Manual-Verification Roster" lists all such items; Section 5 includes the exact Phase 8 Seed Statement from SPEC.md REQ-F-10 point 3; Section 6 "Methodology Notes" cites parallel dispatch per DESIGN.md §4 and the baseline manifest commit range; every Confirmed item is traced back to its corresponding row in a specific `triage/U-XX.md` file without loss of rationale or confidence score.

---

## Component 5: Final Acceptance Checklist

- [x] T-015: Verify Phase 7 cycle acceptance against SPEC.md checklist
  - REQs: REQ-C-01, REQ-C-02, REQ-C-03, REQ-F-01, REQ-F-02, REQ-F-03, REQ-F-04, REQ-F-05, REQ-F-06, REQ-F-07, REQ-F-08, REQ-F-09, REQ-F-10, REQ-F-11, REQ-NF-01, REQ-NF-02, REQ-NF-03
  - Check: All 11 acceptance checklist items from SPEC.md are satisfied: (1) all 101 Investigation Targets triaged with exactly one verdict each; (2) 11 triage docs exist at `triage/U-01.md` through `triage/U-11.md` each with a verdict table; (3) `REPORT.md` exists and aggregates Confirmed items with severity/effort ranking; (4) no item lacks a verdict, no item has multiple verdicts; (5) every Confirmed item has a confidence score ≥80 re-derived from current code (spot-check 5–10); (6) every Refuted item cites why the finding is invalid (spot-check 3–5); (7) every Not-a-bug item cites architectural rationale (spot-check 3–5); (8) every Needs-Manual-Verification item includes a concrete, step-by-step checklist (spot-check 3–5); (9) items affected by Phases 0–6 changes are flagged with phase/PR reference and disposition tag {moot, partially addressed, still applicable in new form} (spot-check 5+); (10) git diff shows zero modifications to `apps/`, `libs/`, `protocols/`, `tests/`, `src/` directories — only new/modified files under `docs/sdd/poc-remediation-phase7/`; (11) only documentation files under `docs/sdd/poc-remediation-phase7/` exist outside the baseline manifest's ephemeral `.manifest/` subdirectory.
