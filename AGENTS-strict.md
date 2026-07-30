# AGENTS.md

Defines how AI agents work on this C++ industrial robot simulation project.
This is a working contract. Follow the user's direct instruction, but do not silently violate correctness, buildability, or ownership boundaries.

---

## 1. Priorities

Use this order when tradeoffs appear:
1. Correctness, buildability, and data safety.
2. Correct ownership layer and stable architecture.
3. Minimal effective change at the correct layer.
4. Clear module boundaries and reversible diffs.
5. Readable code and useful documentation.

Interpretation of "minimal": it does not mean the smallest local patch. It means the smallest change that fixes the real invariant at the layer that owns it. Do not add scattered GUI checks when the invariant belongs to document, runtime, project, IO, or command services. Prefer one central rule over many local compensations.

---

## 2. Project Context

Language: C++17 by default. Do not introduce C++20 unless the existing target already requires it or the user explicitly asks.
Build: CMake, target-based CMake only.
Platform: Windows + Visual Studio; pay attention to DLL paths, Debug/Release separation, and CRLF line endings.

Repository layout:
- `Common`: shared utilities, logging, runtime support.
- `SMRobotCore`: headless robot model, IO, runtime, kinematics, collision, SDK.
- `SMRobotPlatform`: assets, rendering, scene, camera, project/runtime platform, bridges.
- `SMRobotApps`: desktop/viewer apps, quick starts, application-level integration.

Main domains: industrial robot simulation, spraying/painting workflows, scene graph/rendering, asset/URDF/Simscape/robot loading, runtime state, kinematics, trajectory, collision, project/session management.

Known modules: `RobotCore`, `RobotIO`, `RobotRuntime`, `RobotInstance`, `Kinematics`, `Collision`, `RobotSDK`, `RobotTrajectoryCore`, `AssetCore`, `RenderCore`, `SceneCore`, `CameraCore`, `RobotRenderBridge`, `SimulationProject`, `SimulationRuntime`, `SensorCore`, `SensorSimulation`, `RobotViewerCore`, `RobotQtViewer`, `RobotGlfwViewer`.

Do not reorganize repositories, rename modules, move files, or change public APIs unless required by the task.

---

## 3. Rebuild Current Understanding

When code may have changed, inspect current source before coding. Do not rely only on old memory, previous plans, or previous conversations.

For large, unclear, or architectural tasks, produce a short Chinese project snapshot before modifying code. Prefer `docs/agent_project_snapshot.md`.

Snapshot contents: current branch and task goal; affected modules; important classes and ownership; build status if known; recent changes from `git status`, `git diff`, and file inspection; safe extension points; risky files; verification commands.

Document reading priority:
1. Root `AGENTS.md`.
2. `docs/agent_project_snapshot.md`.
3. `docs/agent_change_audit.md`.
4. Active design contracts under `docs/architecture/`, especially `docs/architecture/document_view_gui_contract.md` when touching GUI, binding, view model, document mutation, selection, or viewport display logic.
5. The plan explicitly named by the user.

Old documents are not current authority by default. Do not execute archived/outdated/reference plans or continue old execution status without confirming current code. If documents conflict with source code, source code and this file win; record the discrepancy before coding.

---

## 4. Root-Cause-First Rule

Before editing, decide whether the request describes a symptom or a real invariant. Internally answer:
- What symptom is reported?
- What invariant must always hold?
- Which module owns that invariant?
- Which paths can bypass the rule today?
- Should it work without Qt, in `RobotGlfwViewer`, SDK, tests, or headless runtime?
- Would the same check otherwise be copied into multiple callbacks/widgets?

A local patch is acceptable only when the behavior is truly local to one UI control, no persistent data/runtime semantics are affected, no save/load/undo/redo/project schema/SDK behavior is affected, and the fix will not be duplicated elsewhere.

A structural fix is required when the rule spans multiple views, concerns project data/file IO/runtime/selection/transforms/collision/robot semantics/save-load, can reappear through another UI path, or should be testable without launching Qt.

When structural change is needed, implement the smallest central API that owns the invariant, then adapt UI code to call it.

---

## 5. Planning Rule

Create a Chinese plan before coding when multiple files/modules are involved, public interfaces may change, architecture or module boundaries are affected, the task is unclear, the task is refactor/migration/cleanup, or the expected diff likely exceeds 8 files.

Plan contents: goal and non-goals; files to inspect; files expected to modify; classes/functions likely to change; current data flow and desired data flow; owning module for the invariant; expected behavior; verification method; why this is the minimal correct design; stop conditions.

Do not create plan files unless requested. If creating plan/execution notes under `plans/`, write Chinese by default.

For autonomous long work, define scope, non-scope, completion criteria, verification commands, change budget, and stop conditions before modifying code.

Default stop conditions: more than 50 source/config files need modification; public API rename appears necessary; module dependency change appears necessary; new dependency appears necessary; two consecutive build attempts fail for different reasons; behavior preservation is uncertain; the task expands beyond the original goal.

When a stop condition is reached, stop and report what changed, why stopped, current build/test status, and recommended next options.

---

## 6. Document/View/Command Architecture

Use document-view thinking for persistent project behavior.

Core principle: document/session owns truth; runtime owns execution state; views show state and emit user intents; commands/services mutate persistent state; UI wires intent to command and displays results.

GUI display must be the result of document state, not direct widget-side repair. For any GUI behavior that affects project-visible state, persistent display policy, robot/object/mount/attachment semantics, selection, transforms, save/load, collision, runtime, or viewport rendering, the required flow is:

```text
User intent -> message/command/mutation -> document/session/runtime state change -> registered view notification -> view model rebuild -> widget/viewport repaint
```

Do not implement such behavior by directly pushing ad hoc state into several widgets, tree items, panels, or viewport flags. If a display rule matters, first make sure an owning data structure can express it. If the data structure cannot express it, add or extend the correct document/runtime/view-model state at the owning layer before changing the view.

Document update notification is an architectural mechanism, not an optional convenience. A document does not know which GUI objects exist by hardcoded names. Instead, GUI modules, dialogs, widgets, panels, or their controllers must register with the relevant document/context/event registry when created, declare the document/runtime events they depend on, and refresh from document snapshots or view models when notified. Prefer existing mechanisms such as `RobotQtViewerDocumentController`, `RobotQtViewerEventHub`, `RobotQtViewerDocumentViewRegistry`, module controllers, or their platform equivalents. If an equivalent registration mechanism is missing for a new GUI area, add the smallest typed registration/notification path instead of wiring widget-to-widget refresh calls.

Widgets and dialogs must not own persistent truth. They may keep temporary UI-only state such as focus, current editor text before Apply, hover state, splitter sizes, or transient preview handles. They must not own project semantics, binding relationships, display visibility policy, dirty-state rules, save/load invariants, or cross-view selection behavior. Those belong in document/session/runtime services or in explicit transient task/session state that is rebuilt or cleared through controller logic.

For display defects, first ask:

- What document/runtime state should express the desired display?
- Which command or mutation changes that state?
- Which registered views should be notified?
- Which view model builder converts that state into tree rows, panel controls, or viewport overlays?
- Is any widget directly compensating for missing document state?

`RobotQtViewer` is a UI adapter, not the owner of project semantics. Replacing `RobotQtViewer` with `RobotGlfwViewer` should require rebuilding UI controls, not rewriting project/tool/robot/runtime/save-load/collision semantics.

`MainWindow` may own menus, actions, docks, dialogs, widgets, status messages, top-level signal wiring, and temporary UI-only state. It must not own project mutation semantics, file dirty rules, save/load invariants, robot/tool/sensor/fixture semantics, collision filtering, runtime execution, or persistent transform propagation.

Widgets should expose user intents, accept view models, display command results, and avoid direct persistent document mutation. View model builders may shape state for display but must not perform persistent mutation.

Persistent project edits should go through `ProjectSession`, `ProjectDocumentService`, `SimulationProject`, `SimulationRuntime`, `RobotRuntime`, `RobotInstance`, `RobotRenderBridge`, `Collision`, or another appropriate non-UI module.

Do not add project-schema role packing or persistent `QTreeWidgetItem` construction to `MainWindow`. Prefer extending a view model/builder or document command API. Do not add manual cross-widget refresh chains when a document/event registration path should be used.

Before changing GUI, binding display, view models, task panels, document mutation, selection, or viewport visibility, read `docs/architecture/document_view_gui_contract.md` if it exists.

Before changing `RobotQtViewer` workbench modes, GUI document/session state, mode switching, tree/panel/viewport linkage, mode-specific context menus, or workbench view-model projection, also read `docs/architecture/simulation_platform_document_workbench_contract.md` if it exists.

---

## 7. Structural Repair Rule

Some requested changes are local bug fixes; some reveal that the current design puts responsibility in the wrong layer. Do not automatically choose the smallest visible patch. Choose the smallest change that makes the design correct.

Before a non-trivial modification, classify the task:

- **Local fix**: one component owns the behavior; no other path should share it; no persistent state, runtime state, file IO, save/load, undo/redo, collision, selection, or SDK behavior is involved.
- **Structural fix**: the behavior is an invariant that should hold across multiple views, commands, import/export paths, tests, headless runtime, or future UI implementations.

When the task is structural, first identify the owning abstraction and repair that abstraction. Then adapt existing callers to use it. Do not scatter equivalent checks across GUI callbacks, dialogs, widgets, render code, or file-specific branches.

Typical structural owners:

- Persistent project data: project document, project session, project service, or project command layer.
- Runtime behavior: robot/runtime/trajectory/collision/service layer.
- Visualization mapping: bridge/view-model layer, not core robot logic and not widget code.
- File IO and modified-state policy: project/session/IO layer, not individual menu actions.
- Selection and transform propagation: scene/project/runtime ownership layer, not isolated tree-widget code.

For structural tasks, the plan must state:

- the symptom;
- the invariant that should always hold;
- the layer that should own the invariant;
- current bypass paths;
- the smallest central API or ownership change needed;
- which UI/view callers will be adapted;
- how to verify the rule without relying only on manual GUI testing.

A GUI-only patch is acceptable only after code inspection proves the behavior is truly UI-only.

---

## 8. Module Boundaries

`AssetCore`: CPU-side asset loading/descriptions only; no OpenGL, SceneCore dependency, or robot runtime semantics.

`RenderCore`: GPU resources such as Shader, Texture, UBO, Model, Mesh buffers; no robot semantics, project document semantics, or Qt dependency.

`SceneCore`: SceneGraph, nodes, transforms, traversal, render flow; no robot-specific logic or persistent project schema ownership.

`RobotCore`: robot model and structural robot data; no rendering, Qt, or OpenGL.

`RobotInstance`: runtime robot instances and transforms; no low-level rendering ownership.

`RobotRuntime`: runtime state, trajectory, execution state; no UI ownership.

`RobotIO`: load URDF/Simscape/other robot formats into model data; no OpenGL, SceneGraph creation, or Qt UI behavior.

`RobotRenderBridge`: converts robot/runtime data to scene visualization; may know both robot-side and scene-side concepts; should not own core robot semantics.

`Collision`: collision geometry, objects, distance, contact, self-collision, filtering; independent from rendering and UI; visualization goes through SceneCore or bridge layers.

`CameraCore` and `Kinematics`: independent modules; avoid UI, rendering, or project document dependency unless explicitly designed.

If a boundary violation is found, stop, explain it, and propose the smallest correct fix.

---

## 9. Application and Platform Boundary

Viewer applications, including `RobotQtViewer` and `RobotGlfwViewer`, are UI adapters.

Qt/GLFW may own widgets, input events, dialogs, menus, and temporary UI state. They must not own robot, tool, sensor, fixture, project, collision, or runtime semantics.

Long-lived project data and operations belong in `SMRobotCore` or `SMRobotPlatform`. If a feature should work in GLFW, SDK, tests, or headless runtime, it does not belong primarily in `RobotQtViewer`.

`RobotQtViewer` should call stable Core/Platform interfaces and display results. Small UI-only calculations are acceptable. Persistent model mutation must be delegated to the appropriate non-UI module.

Mounted tools, sensors, fixtures, and attachments should be runtime/project entities, not viewer-only visual objects. They should participate in selection, transform propagation, rendering, collision, save/load, and non-Qt viewers through platform/core APIs.

If a Qt control needs new domain behavior, add or identify the Core/Platform API first, then wire Qt to it.

Before changing RobotQtViewer workbench, task panel, document/view, or RobotQtModules structure, read relevant design-contract documents in `docs/`, if present.

---

## 10. Rendering, Robot, and Collision Rules

Rendering: `RenderCore` owns GPU resources and must not know robot semantics or project document semantics. `SceneCore` owns scene graph/render flow and must not contain robot-specific logic. Robot modules must not own GPU resources or include OpenGL headers. Visualization of robot, collision, sensor, or project data should go through bridge/platform layers.

Robot/runtime: `RobotIO` loads only; `RobotCore` owns robot model only; `RobotRuntime` owns runtime state only; `RobotInstance` owns instance transforms and runtime robot data; `RobotRenderBridge` handles robot-to-scene visualization. Do not mix visual mesh loading, kinematics, collision, runtime, and persistence in one new class.

Collision: collision code remains independent from rendering and UI; visualization goes through SceneCore or bridge layers; filtering must not be hidden in UI logic; distance/contact APIs should be usable from tests or headless tools; no Qt/OpenGL/viewer dependency.

---

## 11. File and Code Organization

Public headers: `include/<Module>/`. Private headers and sources follow existing module convention.

Use `.h` + `.cpp` by default. Avoid header-only implementation unless trivial. Prefer forward declarations in headers. Do not put `.cpp` files under `include/`.

Do not move existing files unless requested. Do not create duplicate classes with similar responsibility. Before adding a file, check whether an existing file is the correct extension point.

---

## 12. Coding Style

Classes: `PascalCase`. Functions: `camelCase`. Data members: `m_xxx`.

Prefer RAII and smart pointers. Avoid global mutable state, unnecessary templates, unnecessary inheritance, and speculative abstractions. Prefer simple data structures when the concept is simple.

Keep identifiers, class/function names, file paths, commands, schema fields, and external API names in original spelling.

Source code, CMake files, and identifiers should remain ASCII unless the existing file already uses non-ASCII text or the user asks otherwise. Do not use emoji in code, comments, plans, audits, commit-style notes, or documentation.

Chinese is preferred for agent-created plans, audits, and explanatory documents under `plans/` and `docs/`.

---

## 13. CMake and Dependencies

Use target-based CMake only. Do not modify unrelated targets. Do not add global include/link directories unless no target-based option exists.

Keep Debug/Release behavior clear. Do not hardcode local machine paths. Use CMake variables, imported targets, or documented cache variables.

Do not introduce new dependencies unless explicitly requested. If a dependency is unavoidable, stop and explain why.

---

## 14. Change Protocol

Before coding, state: files to inspect; files expected to change; which invariant owns the behavior; why the change belongs at that layer; how behavior will be verified.

During coding: modify only relevant files; do not perform unrelated refactors; do not reformat unrelated code; preserve naming/style; preserve line endings in untouched files.

After coding, report: files modified; what changed; build/test result; known limitations; public APIs changed or confirmed unchanged; module dependencies changed or confirmed unchanged.

Do not claim success without build/test evidence.

---

## 15. Large Change Safety

If the working tree already contains large AI-generated modifications, do not continue directly. First audit with `git status` and `git diff --stat`; group modified files by module; separate safe refactors, behavior changes, build-system changes, and risky changes; identify files to revert/split; verify configure/build status.

After a large change, create or update `docs/agent_change_audit.md` in Chinese. Include modified file groups, purpose, possible behavior changes, known compile/test result, remaining risks, and recommended next step.

Do not add new abstractions until the current diff is understood.

---

## 16. Verification

Prefer verification in order: CMake configure; build affected target; build main application target; run unit test or existing example; run viewer/example manually if required.

If verification is not possible, explain which command should be run, why it was not run, and what result is expected.

Useful inspection commands:

```bat
git status
git diff --stat
git diff --name-only
git diff -- <file>
```

Useful build commands:

```bat
cmake --preset <preset-name>
cmake --build --preset <build-preset-name>
cmake --build <build-dir> --target <target-name> --config Debug
```

Adapt commands to actual presets and targets. Do not invent successful command results.

---

## 17. Code Retirement and Deletion

AI agents must control code growth. Compatibility is useful only when it protects real users, project files, public APIs, or migration paths. Do not keep old functions, duplicate paths, wrappers, flags, or adapters merely to avoid deleting code. Dead code confuses future agents and weakens architectural stability.

Before adding a new implementation beside an old one, decide whether the old path should be retired. Prefer replacement over parallel implementation when behavior is meant to become the new standard.

Classify old code into one of these states:

- **Active**: still used by normal runtime, build targets, tests, examples, public APIs, or supported project files. Keep it.
- **Compatibility**: needed only for old project files, public API stability, or staged migration. Keep it temporarily, mark the reason, and define a removal condition.
- **Deprecated**: replaced by a newer path, not preferred for new callers, but still referenced somewhere. Stop adding new callers and plan migration.
- **Dead**: not referenced by build targets, tests, examples, public headers, project IO, or documented workflows. Delete it after verification.

Deletion should be considered when any of the following is true:

- A new central API fully replaces scattered old helper functions.
- Two implementations perform the same domain operation.
- Old code is kept only because the agent was afraid to remove it.
- A wrapper exists only to call the new API and has no public compatibility value.
- A UI-specific path duplicates a Core/Platform command.
- An old branch is unreachable after the new invariant is centralized.
- Tests/examples/build targets no longer reference the old code.

Do not delete code blindly. Before deleting, inspect references using search/build evidence, not guesswork. Check at least:

```bat
rg "OldClassName|oldFunctionName|oldFileName"
git grep "OldClassName"
```

Also check CMake targets, public headers, examples, tests, project IO, serialization, plugin registration, factory maps, reflection-like string names, Qt signal/slot names, and file-format migration code.

Safe deletion protocol:

1. State what code is being retired and what replaces it.
2. Confirm whether it is public API, project-file compatibility, or only internal implementation.
3. Remove or migrate callers first.
4. Delete unused functions/files only after references are gone.
5. Update CMake, headers, docs, examples, and tests.
6. Build affected targets.
7. Report deleted files/functions and verification result.

When compatibility must remain, make it explicit and narrow:

- Add a short comment explaining why the old path remains.
- Route old APIs to the new central implementation when possible.
- Do not let deprecated APIs contain separate business logic.
- Do not add new callers to deprecated APIs.
- Record the removal condition, for example: after project schema v2 migration, after old example target is removed, or after public SDK deprecation window.

For large cleanup, use a separate cleanup plan instead of mixing broad deletion into an unrelated feature. The plan must group candidates as: safe delete now, migrate then delete, keep for compatibility, and unknown/risky.

Healthy diff principle: for refactors and replacements, a mature change should often remove code as well as add code. A diff that only adds new parallel paths must justify why the old paths cannot be retired in the same task.

---

## 18. Legacy and Generated Code

Do not modify `archive`, deprecated, generated, or legacy directories unless required. Do not refactor legacy code just because it looks messy. Mention legacy issues, but do not fix them blindly.

Legacy code is not automatically protected. If legacy code is still required for compatibility, keep it and document why. If it is not referenced, not built, and not needed for supported file/API compatibility, prefer deleting it through the Code Retirement protocol.

Preserve old project IO compatibility unless migration is explicitly requested and tested.

---

## 19. Forbidden by Default

Unless explicitly requested, do not perform large unrelated refactors, rename public APIs, change module dependencies, add third-party dependencies, move files, reorganize directories, create parallel duplicate implementations, replace working code only because a new design looks cleaner, rewrite unrelated files for formatting, mix LF/CRLF, or convert unrelated line endings.

If one appears necessary for the correct architectural fix, stop and report why it is necessary, what smaller alternatives were considered, which API/dependency would change, and how to verify behavior preservation.

---

## 20. Documentation and Response Style

Responses should be precise, concise, engineering-focused, and Chinese by default for plans/summaries. Show tradeoffs when relevant and avoid unnecessary theory. For code tasks: understand first, plan in Chinese, then implement.

Agent-created architecture notes, inventories, audits, execution documents, and plans under `docs/` and `plans/` must be written in Chinese by default. If updating an existing English agent-authored plan/status/inventory, convert touched content to Chinese unless the user asks for English.

Keep identifiers, file paths, commands, schema fields, and external API names in original spelling.

---

## 21. Line Ending Policy

This repository is developed mainly on Windows with Visual Studio. When editing existing files, preserve the file's original line ending style.

For C/C++/CMake/Qt project files, use Windows CRLF line endings: `.h`, `.hpp`, `.cpp`, `.c`, `.cxx`, `.cmake`, `CMakeLists.txt`, `.ui`, `.qrc`.

Do not mix LF and CRLF in the same file. Before applying changes, ensure each modified file uses a consistent line ending. Do not rewrite unrelated parts only to change line endings.

---

## END
