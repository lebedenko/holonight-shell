# Specification: POC Readiness Review Process

## Executive Summary

The holonight-shell POC codebase (~438 C++/QML files across libs/ and apps/) shall undergo a comprehensive, multi-stage read-only audit to produce an aggressive findings report suitable for consumption by a future remediation SDD pipeline. This specification defines the review process, its granularity (10–14 logical review units), tooling (qt-cpp-review + qt-qml-review skills), finding triage scheme (Severity + Effort), and deliverables (REPORT.md with architecture review, unit findings, gaps analysis, and stale-backlog context).

---

## 1. Introduction & Context

**Project**: holonight-shell — C++23/Qt6 Wayland shell using wlr-layer-shell and ext-workspace protocols. Early-stage POC, not production-ready.

**Codebase Scope**:
- ~438 C++/QML source files
- 5 foundational libraries: holonight-core, holonight-platform, holonight-config, holonight-surfaces, holonight-services
- 14 service subsystems: audio, brightness, calendar, idle, kde-compat, launcher, mime, network, notifications, portal, session, session-integration, weather, weather-icon
- 2 applications: shell (11 QML areas: Background, Controls, Launcher, Notifications, Popups, RightSidebar, Topbar, Tray, Utility, WeatherIcon, Widgets) and settings (standalone binary)
- ~48 prior SDD feature directories (SPEC/DESIGN/TASKS documentation)

**Review Objective**: Identify structural, architectural, and maintainability issues across the entire codebase to inform a future remediation pipeline. This cycle is 100% read-only; no code changes or task execution occur.

---

## 2. Goals & Non-Goals

### Goals (What This Review Achieves)

The review shall:
- Comprehensively audit the entire codebase for architectural, design, and code-quality issues
- Provide aggressive, unconstrained refactoring recommendations (backward compatibility NOT a concern)
- Partition the codebase into logical review units suitable for deep, focused analysis
- Triage findings by severity and remediation effort for future SDD prioritization
- Synthesize cross-cutting architectural gaps and weak design decisions
- Document pre-existing stale backlog items as context (not as new findings to address)
- Produce a final comprehensive audit report ready for a future remediation pipeline

### Non-Goals (What This Review Excludes)

The review explicitly does NOT:
- Modify, write, or commit any code (documentation-only)
- Resume or execute unchecked TASKS.md items from prior SDD cycles (stale backlog is noted, not acted upon)
- Achieve exhaustive per-function line-coverage analysis (targets high-confidence structural findings only, per qt-cpp-review/qt-qml-review >80/100 threshold)
- Preserve backward compatibility or existing API contracts (assumes full refactoring freedom for a pre-production POC)
- Debug specific runtime issues, user-reported bugs, or crash scenarios (focuses on code quality and architecture)
- Validate the correctness of prior SPEC/DESIGN documents (uses them as context only; judges current code independently)

---

## 3. Review Process Overview

### Pipeline Stages

The POC readiness review executes as a multi-checkpoint process:

1. **Stage 1 (Planning)**: This SPEC document — define review granularity, acceptance criteria, tooling
2. **Stage 2 (Design)**: Produce DESIGN.md with final 10–14 review unit groupings and source-directory assignments
3. **Stage 3 (Unit-by-Unit Analysis)**: Iteratively apply qt-cpp-review and qt-qml-review skills to each unit, with human checkpoints between units
4. **Stage 4 (Synthesis & Reporting)**: Aggregate all unit findings, identify architecture-level gaps, document stale backlog, produce final REPORT.md

---

## 4. Functional Requirements

### REQ-F-1: Review Unit Partitioning

**Ubiquitous**: The review process shall partition the entire holonight-shell codebase into **10–14 logical review units**, each grouping related C++ libraries, services, or QML component areas for cohesive deep analysis.

*Rationale*: The codebase contains ~28 distinct subsystems/QML areas. Analyzing each individually would yield unbounded tool invocations; grouping them into 10–14 thematic units balances focused deep analysis with reasonable scope per cycle.

*Example grouping patterns*: 
- "Topbar QML + Weather Services" (combining multiple related QML sections + dependent services)
- "Session Management + Brightness + Idle" (small interdependent services grouped together)
- "Core Platform + Layer-Shell Protocol" (foundational infrastructure)

**Acceptance Criterion**: A DESIGN.md document lists all 10–14 units by name, their constituent source directories (absolute paths relative to project root), constituent C++ headers/sources and QML file locations, and the architectural rationale for the grouping. Every source directory in libs/ and apps/ appears in exactly one unit; no directory is omitted or duplicated.

---

### REQ-F-2: Prior Artifact Context Integration

**Event-driven**: When a review unit is analyzed, the process shall first read and summarize relevant prior SPEC.md and DESIGN.md documents from docs/sdd/{feature}/ directories that relate to the unit's scope, to establish architectural context and design intent.

*Rationale*: Prior SDD cycles documented the intended architecture and design decisions for features. Understanding this intent allows the current code review to identify divergence (doc-drift) and contextualize findings within the feature's history.

**Acceptance Criterion**: The unit's findings report includes a "Prior Context" subsection naming and summarizing the consulted prior SPEC/DESIGN documents. Any identified mismatch between prior design intent and current implementation is flagged as a finding (category: "doc-drift", severity/effort as appropriate).

---

### REQ-F-3: Deep C++ Code Analysis

**Ubiquitous**: The review process shall apply the qt-cpp-review skill to every review unit that contains C++ source code (headers, .cpp files, CMake definitions).

*Rationale*: qt-cpp-review runs deterministic linting (60+ rules, clang-tidy style) plus 6 parallel deep-analysis agents covering:
- Model contracts (ownership, lifetime, move semantics, const-correctness)
- Threading (data races, synchronization, re-entrancy)
- API correctness (Qt signal/slot contracts, D-Bus marshaling, memory management)
- Error handling (exception safety, nullptr checks, resource cleanup)
- Performance (allocations, copies, algorithmic complexity)

**Acceptance Criterion**: Every C++-containing unit receives qt-cpp-review output. The output explicitly documents (a) which source directories were analyzed, (b) which findings (if any) exceed the >80/100 confidence threshold, or (c) an explicit statement "No findings above confidence threshold in this unit". Deterministic linting passes (no warnings) or documents known pre-existing violations.

---

### REQ-F-4: Deep QML Code Analysis

**Ubiquitous**: The review process shall apply the qt-qml-review skill to every review unit that contains QML source code (.qml files under apps/shell/qml/ or other QML-containing directories).

*Rationale*: qt-qml-review runs deterministic linting (qmllint rules) plus 6 parallel deep-analysis agents covering:
- Bindings and reactivity (unintended binding loops, stale bindings, performance)
- Layout correctness (anchoring/layout logic, z-order, nested-layout centering gotchas)
- Loader/Repeater delegate behavior (memory leaks, state isolation, model/view sync)
- State machine correctness (state transitions, conflicting state rules)
- Performance (image loading, repainting, animation efficiency)

**Acceptance Criterion**: Every QML-containing unit receives qt-qml-review output. The output explicitly documents (a) which source directories were analyzed, (b) which findings exceed the >80/100 confidence threshold, or (c) an explicit statement "No findings above confidence threshold in this unit". Deterministic linting passes (no warnings) or documents known pre-existing violations.

---

### REQ-F-5: Finding Metadata & Triage

**Ubiquitous**: Every finding produced by qt-cpp-review or qt-qml-review and carried forward to the final report shall include the following metadata:

1. **Severity**: Critical | High | Medium | Low
   - Critical: core functionality broken, data loss risk, security vulnerability, deadlock/crash
   - High: major feature incomplete, significant performance degradation, architectural layering violation
   - Medium: code quality issue, maintainability debt, suboptimal design, minor feature gap
   - Low: style/convention deviation, documentation gap, minor inefficiency

2. **Effort Estimate**: S (Small, ≤1–2 hours) | M (Medium, 4–8 hours) | L (Large, 1–3 days) | XL (Extra-Large, ≥3 days)

3. **Location**: Absolute file path(s) and line number(s) where applicable

4. **Rationale**: A concrete failure scenario, misuse pattern, or architectural weakness (never an assertion; always independently verifiable)

5. **Suggested Direction**: A proposed fix or refactoring approach. For POC codebase: can be aggressive/breaking; backward compatibility NOT required.

**Acceptance Criterion**: Every finding in the final report includes all five metadata fields. No finding lacks rationale or suggested direction. Severity and effort assignments are consistent across all findings (no arbitrary variations for similar issues).

---

### REQ-F-6: Architecture-Level Gaps & Cross-Cutting Issues

**Ubiquitous**: The review process shall synthesize individual unit findings and identify cross-cutting architectural gaps, systemic weak decisions, and layering violations that span multiple units and are not reducible to a single unit's findings.

*Examples of cross-cutting issues*:
- Pervasive anti-patterns used across multiple services (e.g., all services implement their own caching strategy instead of a shared pattern)
- Missing abstraction layers (e.g., all QML components directly invoke C++ APIs instead of a mediator layer)
- Circular dependencies between service tiers
- Inconsistent error-handling strategies across similar subsystems
- Threading model gaps (e.g., all services spawn threads independently instead of using a pool)

**Acceptance Criterion**: Final report includes an "Architecture & Design Gaps" section (≥500 words) that identifies 3–8 systemic issues. Each issue includes severity/effort triage (as per REQ-F-5), affected units, rationale, and suggested architectural direction. This section is distinct from and supplementary to per-unit findings.

---

### REQ-F-7: Stale Backlog Context Section

**Ubiquitous**: The review process shall document pre-existing incomplete work (unchecked TASKS.md items from prior SDD cycles) as a separate "Stale Backlog Context" section, explicitly stating they are NOT part of this review's scope.

*Rationale*: Several feature directories in docs/sdd/{feature}/ have known unchecked TASKS (e.g., portal-service with 12, idle-management with 7). Transparency on these known gaps is required to prevent conflating them with newly discovered issues.

**Acceptance Criterion**: Final report includes a "Stale Backlog Context" section that lists:
- Feature directory name
- Count of unchecked TASKS items
- A sample of 1–2 task titles (for context)
- Explicit statement: "These items are pre-existing incomplete work from prior SDD cycles and are NOT addressed by this review cycle."

---

### REQ-F-8: Comprehensive Audit Report Deliverable

**Ubiquitous**: The review process shall produce a final comprehensive audit report document (REPORT.md) that integrates all unit findings and synthesis work into a single, ready-to-use artifact for a future remediation SDD pipeline.

The report shall contain the following sections:

1. **Executive Summary** (1–2 pages): High-level overview of review scope, unit count, key findings categories, and recommended remediation strategy

2. **Architecture & Component Review** (2–3 pages): Current architecture overview (layering, technologies, cross-cutting patterns, known strengths)

3. **Review Unit Inventory** (1 page): Tabular summary of all 10–14 units (name, source paths, purpose, C++/QML breakdown)

4. **Per-Unit Deep Findings** (main body): For each unit:
   - Unit name and scope
   - Prior context (docs consulted, doc-drift notes)
   - Findings (grouped by category: design/architecture, correctness, performance, maintainability)
   - Summary of qt-cpp-review and qt-qml-review sign-off

5. **Architecture & Design Gaps** (≥500 words): Systemic cross-cutting issues (as per REQ-F-6)

6. **Stale Backlog Context** (as per REQ-F-7)

7. **Remediation Roadmap** (1–2 pages): Suggested prioritization strategy (by severity/effort matrix, architectural impact, dependency order)

**Acceptance Criterion**: REPORT.md is complete, self-contained, and ready for handoff to a remediation pipeline. It explicitly references all 438 source files (by unit assignment), synthesizes all findings, and is ≥10 pages. Report is produced in Markdown with clear section hierarchy and is checked into docs/sdd/poc-readiness-review/.

---

## 5. Non-Functional Requirements

### REQ-NF-1: Review Granularity (Unit Count & Coverage)

**Constraint**: The review shall partition the codebase into **10–14 review units**, targeting an average of **31–44 source files per unit** (438 files ÷ 10–14 units).

*Rationale*: Too few units (5–7) risks unbounded scope per unit and loss of cohesive analysis; too many (>20) fragments the codebase, prevents meaningful synthesis, and increases overhead. 10–14 units with 31–44 files each provide a tight sweet spot.

**Acceptance Criterion**: DESIGN.md specifies final unit count (must be 10–14) and provides per-unit file counts. Any unit with <20 or >60 files must include justification in DESIGN.md. Arithmetic check: sum of all per-unit file counts ≥ 438 (no omissions).

---

### REQ-NF-2: 100% Source Directory Coverage (Blocker Condition)

**Unwanted-Behavior**: If any source directory in libs/ or apps/ is not assigned to exactly one review unit (neither omitted nor duplicated), the process shall flag it as a blocker and halt before proceeding to analysis.

*Rationale*: Incomplete coverage leaves gaps in the audit; overlapping assignments create redundant analysis and confusion.

**Acceptance Criterion**: Pre-analysis directory audit script enumerates all libs/ and apps/ subdirectories and validates 1:1 assignment to units. If violations found, generates diagnostic report and halts with clear error message. Checklist passes before any unit analysis begins.

---

### REQ-NF-3: Unit-by-Unit Checkpoint Workflow (Human Gating)

**Event-driven**: When a review unit's findings analysis is complete, the process shall pause and request human review/approval before proceeding to the next unit.

*Rationale*: Allows stakeholder feedback and mid-pipeline direction adjustment (e.g., re-scope a unit, reprioritize issue types, address misclassifications). Sequential, human-gated execution prevents irreversible divergence.

**Acceptance Criterion**: Process design documentation specifies checkpoint handoff points. Unit findings are delivered to a human reviewer; review is marked as "approved", "re-analyze", or "adjust scope" before proceeding. Process logs document checkpoint sequence and any adjustments made.

---

### REQ-NF-4: Prior Artifact Scope (Context vs. Constraint)

**Constraint**: Prior SPEC.md and DESIGN.md documents from docs/sdd/{feature}/ directories shall inform and enrich the contextual understanding of each unit's architectural intent, but shall NOT constrain the review's judgment of the current code.

*Rationale*: The codebase may have drifted from its original design; the review must audit the current reality, not validate old plans. Design documents provide historical context, not acceptance criteria.

**Acceptance Criterion**: Every unit review documents prior SPEC/DESIGN context consulted (by file path). Any identified doc-drift (current code differs from design intent) is explicitly flagged as a finding (category: "doc-drift", severity/effort as appropriate). Findings are based on current code, not prior documents.

---

### REQ-NF-5: Sequential Execution (No Parallelization)

**Constraint**: The review process shall execute units sequentially (one at a time), not in parallel.

*Rationale*: Sequential execution with human checkpoints between units allows mid-pipeline feedback and direction adjustment; parallelization would eliminate this flexibility.

**Acceptance Criterion**: Process design specifies serial execution order. Final report documents unit analysis sequence and any adjustments made between units based on interim feedback.

---

### REQ-NF-6: Tool Confidence Threshold

**Constraint**: Findings reported by qt-cpp-review and qt-qml-review shall meet the tools' built-in **>80/100 confidence threshold**. Speculative or lower-confidence findings are either excluded or explicitly flagged with caveats and confidence scores.

*Rationale*: Maintains signal-to-noise ratio and prevents drowning the future remediation pipeline in false positives.

**Acceptance Criterion**: Tool output includes confidence scores for each finding. Final report includes only findings with >80 confidence, or explicitly marks lower-confidence findings with their scores and caveats (e.g., "70/100 confidence — may be a false positive if X assumption is wrong").

---

## 6. Constraints (REQ-C)

### REQ-C-1: Read-Only Audit (No Code Modifications)

**Constraint**: This review cycle shall NOT modify, write, commit, or deploy any code. All outputs are documentation only (SPEC.md, DESIGN.md, unit findings documents, REPORT.md).

*Rationale*: The goal is to identify issues for a future remediation pipeline. Fixing issues now would prevent the systematic, checkpoint-gated remediation process the user intends.

**Acceptance Criterion**: Git status at end of review shows no staged or committed code changes. Only new .md documentation files appear (untracked or in a single commit tagged "docs: add poc-readiness-review findings"). No modifications to C++ or QML source files.

---

### REQ-C-2: Aggressive Refactoring Recommendations (No Backward-Compat Constraint)

**Constraint**: All findings and suggested fixes shall assume unlimited freedom to refactor, restructure, or replace code. No finding shall be softened, deferred, or qualified on the grounds that a change would break existing API contracts, backward compatibility, or user-facing features.

*Rationale*: holonight-shell is a pre-production POC. Backward compatibility is not a concern. Cleanliness, maintainability, and architectural soundness take priority over preserving early (possibly misguided) design decisions.

**Acceptance Criterion**: Suggested fixes in findings are aggressive where architecturally warranted. Examples of acceptable suggestions: "split this 400-line god class into 5 focused classes", "eliminate this interdependent service by merging its contract into a facade", "rewrite this QML component tree to use a proper model-delegate pattern". Rationale for any suggested breaking change does NOT cite backward compatibility as a constraint.

---

### REQ-C-3: No Resumption of Prior SDD Tasks

**Constraint**: Unchecked TASKS.md items from prior SDD cycles (docs/sdd/{feature}/TASKS.md) shall NOT be resumed, executed, validated, or included in the scope of this review. They are noted as pre-existing context only in the "Stale Backlog Context" section.

*Rationale*: This review is a POC-wide readiness audit, not a continuation of prior feature SDD pipelines. Resuming old task work would extend scope unboundedly and conflate two distinct initiatives.

**Acceptance Criterion**: Final report's "Stale Backlog Context" section explicitly lists pre-existing unchecked TASKS by feature directory and count, with a clear statement: "These items are pre-existing incomplete work from prior SDD cycles and are NOT addressed by this review cycle." No old TASKS are analyzed, attempted, or addressed as findings.

---

### REQ-C-4: High-Confidence Findings Only

**Constraint**: Findings shall be limited to those verified above the >80/100 confidence threshold of the underlying tools (qt-cpp-review, qt-qml-review). Uncertain, speculative, or tool-lint-only findings are excluded unless explicitly marked as "lower confidence" with caveats.

*Rationale*: A high-quality findings report avoids false positives and maintains credibility for a future remediation pipeline.

**Acceptance Criterion**: All findings in final report have >80/100 confidence per tool output. Lower-confidence findings (if included) are clearly marked as such with confidence score, caveats, and reasoning for inclusion despite lower score.

---

## 7. Acceptance Criteria (Master Checklist)

| ID | Requirement | Acceptance Criterion | Owner/Validator |
|----|-------------|----------------------|-----------------|
| F-1 | Unit Partitioning | DESIGN.md lists 10–14 units with 100% libs/apps/ coverage, no omissions or duplicates | Design stage |
| F-2 | Prior Context | Unit findings reference consulted prior SPEC/DESIGN docs; doc-drift flagged | Reviewer |
| F-3 | C++ Analysis | Every C++-containing unit has qt-cpp-review output with findings or explicit "no findings" | Tool sign-off |
| F-4 | QML Analysis | Every QML-containing unit has qt-qml-review output with findings or explicit "no findings" | Tool sign-off |
| F-5 | Finding Metadata | All findings include Severity, Effort, Location, Rationale, Suggested Direction | Findings audit |
| F-6 | Architecture Gaps | "Architecture & Design Gaps" section ≥500 words, 3–8 systemic issues, severity/effort per issue | Synthesis |
| F-7 | Stale Backlog | "Stale Backlog Context" section lists pre-existing unchecked TASKS by feature, count, explicit out-of-scope statement | Backlog audit |
| F-8 | Final Report | REPORT.md complete with 7 sections, ≥10 pages, all 438 files attributed, ready for remediation pipeline | Report review |
| NF-1 | Granularity | 10–14 units, 31–44 files per unit on average, DESIGN.md documents any deviations | DESIGN validation |
| NF-2 | Coverage | Pre-analysis directory audit; 100% of libs/apps/ assigned to exactly one unit; halt if violations | Coverage audit |
| NF-3 | Checkpoints | Process pauses after each unit for human review/approval before next unit; checkpoint logs documented | Workflow enforcement |
| NF-4 | Prior Artifacts | Prior SPEC/DESIGN inform context; current code judged independently; doc-drift findings called out | Review discipline |
| NF-5 | Sequencing | Units analyzed 1 at a time (not in parallel); process logs show sequential execution and checkpoint adjustments | Process logs |
| NF-6 | Confidence | Only >80/100 confidence findings included; lower-confidence findings explicitly marked with score and caveats | Tool config |
| C-1 | Read-Only | No code modifications; git status shows only new .md docs at cycle end; no commits modifying C++/QML | Git audit |
| C-2 | No Backward-Compat | Findings suggest aggressive refactoring; no softening for API stability; backward-compat not mentioned as constraint | Finding review |
| C-3 | No Task Resumption | Stale TASKS listed in "Stale Backlog" section only; no old TASKS analyzed or executed; explicit out-of-scope statement | Scope enforcement |
| C-4 | Confidence Threshold | All findings >80/100 confidence; lower-confidence findings (if any) explicitly marked and justified | Tool validation |

---

## 8. Key Definitions

### Review Unit
A logical grouping of related C++ libraries, services, or QML component areas (30–50 source files each), defined to enable cohesive deep analysis without unbounded scope. Examples: "Topbar QML + Weather Services", "Session Management + Brightness + Idle", "Core Platform + Layer-Shell Protocol".

### Severity Levels

- **Critical**: Core functionality broken; data loss risk; security vulnerability; deadlock/crash that prevents usage
- **High**: Major feature incomplete or broken; significant performance degradation; architectural layering violation; design pattern misuse that affects multiple subsystems
- **Medium**: Code quality issue; maintainability debt; suboptimal design; minor feature gap; performance inefficiency in non-critical path
- **Low**: Style/convention deviation; documentation gap; minor inefficiency; cosmetic issue; edge-case handling gap

### Effort Estimates (Remediation Cost)

- **S (Small)**: ≤1–2 hours; single-file fix; straightforward refactoring; no dependencies
- **M (Medium)**: 4–8 hours; multi-file changes; few dependencies; requires design thought; 1 test cycle
- **L (Large)**: 1–3 days; cross-module rework; multiple dependencies; requires integration testing
- **XL (Extra-Large)**: ≥3 days; architectural rewrite; major subsystem dependencies; comprehensive testing; risk of cascading issues

### Doc-Drift Finding

A documented mismatch between a feature's original SPEC/DESIGN documentation (in docs/sdd/{feature}/) and the current implementation in code. Example: "original DESIGN.md specifies X caching pattern; implementation uses Y pattern instead without updating docs."

### Stale Backlog

Unchecked (incomplete) items in prior TASKS.md files, indicating unfinished work from the original SDD cycle for that feature. Noted for transparency and context; NOT resumed or addressed by this review.

---

## 9. Non-Goals (Explicit Exclusions)

This review cycle explicitly does NOT:

1. **Modify or commit code** — all outputs are documentation only (.md files); no C++/QML changes
2. **Resume or execute prior SDD TASKS** — unchecked TASKS.md items from docs/sdd/{feature}/ are noted in "Stale Backlog Context" section, not acted upon
3. **Achieve exhaustive per-function line-coverage analysis** — targets high-confidence structural findings per qt-cpp-review/qt-qml-review >80/100 threshold; does not require every function to be audited
4. **Preserve backward compatibility or API stability** — assumes freedom to refactor aggressively; breaking changes are welcome if architecturally sound
5. **Debug specific runtime issues, crashes, or user-reported bugs** — focuses on code quality, architecture, and maintainability audit
6. **Validate the correctness of prior SPEC/DESIGN documents** — uses them as contextual input; judges the current codebase independently
7. **Produce a remediation plan** (that's Stage 4's responsibility) — this spec defines the findings/synthesis process, not the fix execution

---

## 10. Assumptions & Dependencies

### Assumptions

1. **Qt6 & C++23 are the final language/framework targets** — no migration to Qt5 or C++20 is considered
2. **Wayland/wlr-layer-shell/ext-workspace are stable dependencies** — review assumes these protocols are correct and in-use as intended
3. **The codebase compiles and runs today** — no findings address build failures or runtime crashes in the current checkout
4. **Prior SDD cycles completed their feature design/delivery** — docs/sdd/{feature}/ directories represent a snapshot of past work, not live proposals
5. **Human reviewers are available for checkpoints between units** — process relies on human feedback; no auto-advance logic is implemented
6. **The target of remediation is the entire codebase** — no directories or features are declared out-of-scope pre-analysis (though pre-existing stale work is noted)

### Dependencies

- **qt-cpp-review skill**: Must be available and run at confidence threshold ≥80/100
- **qt-qml-review skill**: Must be available and run at confidence threshold ≥80/100
- **Git repository**: Codebase must be in a clean git checkout for documentation changes; no prior uncommitted code changes
- **Prior SDD artifacts**: docs/sdd/ directory structure is readable; SPEC.md and DESIGN.md files for each feature are available for context
- **Human review cadence**: Humans must be available to review and approve unit findings before next unit begins; assumes 1–2 days per unit

---

## 11. References & Artifacts

### Codebase Structure
- **Foundational Libraries**: `libs/holonight-core/`, `libs/holonight-platform/`, `libs/holonight-config/`, `libs/holonight-surfaces/`
- **Services**: `libs/holonight-services/src/` (14 subsystems)
- **Shell Application**: `apps/shell/app/` and `apps/shell/qml/` (11 QML areas)
- **Settings Application**: `apps/settings/`
- **Prior SDD Docs**: `docs/sdd/{feature}/` (~48 directories)

### Project Documentation
- `CLAUDE.md`: Project conventions, gotchas, build/test commands
- `Taskfile.yml`: Task-based build and test workflow
- `.clang-tidy`, `.clang-format`: Code style and linting rules
- `CMakeLists.txt`: C++ and QML module registration

### Review Tools
- `qt-cpp-review` skill: Linting + 6 parallel deep-analysis agents (model contracts, threading, API correctness, error handling, performance)
- `qt-qml-review` skill: Linting + 6 parallel deep-analysis agents (bindings, layout, loaders, delegates, states, performance)

---

## 12. Appendix: Pre-Existing Stale Work (Known Incomplete)

The following features have documented but incomplete TASKS.md items from prior SDD cycles. These are pre-existing gaps and are NOT part of this review's scope (listed for transparency and context only):

| Feature Directory | Unchecked TASKS Count | Notes |
|---|---|---|
| portal-service | 12 | Pre-existing implementation gaps in IPortalDBus adapter pattern |
| idle-management | 7 | Incomplete daemon detection and state machine refinement |
| launcher-desktop-cache | 4 | Cache invalidation strategy incomplete |
| holonight-settings | 2 | Verified via `grep -c '^- \[ \]' docs/sdd/holonight-settings/TASKS.md` |
| launcher-redesign | 1 | Verified via `grep -c '^- \[ \]' docs/sdd/launcher-redesign/TASKS.md` |
| right-sidebar | 1 | Verified via `grep -c '^- \[ \]' docs/sdd/right-sidebar/TASKS.md` |
| sidebar-upcoming-calendar | 2 | Verified via `grep -c '^- \[ \]' docs/sdd/sidebar-upcoming-calendar/TASKS.md` |
| system-appearance-portal | 1 | Verified via `grep -c '^- \[ \]' docs/sdd/system-appearance-portal/TASKS.md` |
| theme-variant-catalog | 2 | Verified via `grep -c '^- \[ \]' docs/sdd/theme-variant-catalog/TASKS.md` |
| workspace-indicator-redesign | 2 | Verified via `grep -c '^- \[ \]' docs/sdd/workspace-indicator-redesign/TASKS.md` |

**Explicit Scope Statement**: The above stale items are pre-existing incomplete work from prior SDD cycles. They will be documented in the final "Stale Backlog Context" section of REPORT.md with their exact counts, and are NOT addressed by this review cycle.

---

## Document Metadata

| Field | Value |
|---|---|
| Document ID | POC-Readiness-Review/SPEC.md |
| Version | 1.0 (SDD Stage 1 — Planning) |
| Date | 2026-07-03 |
| Status | Ready for Design Stage |
| Author | SDD Review Process (Specification) |
| Next Stage | Design — produce docs/sdd/poc-readiness-review/DESIGN.md with 10–14 unit groupings |

---

## Document Sign-Off

- [ ] Requirements reviewed and approved by project stakeholder
- [ ] Review process readiness confirmed (tools available, human reviewers assigned)
- [ ] Proceed to Stage 2 (Design)

---

*End of SPEC.md*
