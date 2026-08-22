# CoserRetrieval.UI

Windows WPF launcher for the active CoserRetrieval system. It exposes the
common `coser_cli ingest/query` arguments as a form and starts the selected
command as a child process. The list visualizes each supported image below the
selected input path:

- complete ingest rows are green;
- face/clothing partial route failures are marked on their corresponding stage;
- a process or ingest failure marks every stage red.

The form exposes command, input path, database, face/clothing indices, models
directory, person, role, query mode, and top-k. `Advanced CLI arguments` is
passed through after these fields and accepts quoted values, covering optional
arguments such as `--face-model adaface`, `--file-prefix`, `--report`,
`--human-parsing-model`, and `--clip-model`. The `Run Regression` button fills
the form with a disposable regression database and starts the fixed 7-image set.

The window is resizable and contains a two-direction scrollable task list. A
row shows a truncated file name with its full path in a tooltip, a source image
thumbnail, and the stages `Hash`, `Decode`, `Face`, `Segment`, `Embedding`, and
`Index`. Green is complete, blue with a marquee bar is running, gray is pending,
and red is failed.

## Task Views And Order

The task area has two live-count tabs. `Running / Waiting` contains only the
current CLI batch; each row is moved to `Finished` as soon as its CLI result is
received. The finished view retains completed, duplicate-skipped, warning, and
failed rows for inspection, using the same thumbnail and per-stage detail row
as the active view.

The order menu controls the active view:

- `Running first` moves active rows above waiting rows while preserving file
  order within each group.
- `UI order priority` retains the file order. Directory inputs are sorted with
  ordinal lexical order before display, matching the CLI's TBB enqueue order;
  the first seven rows are the maximum in-flight tasks.
- `Priority + follow` uses the same enqueue order and scrolls to the next row
  that becomes active.

The UI currently obtains per-file updates from CLI output. It handles stdout
and stderr arriving in either order, so a late partial-route warning is still
applied after a row has moved to `Finished`.

## Native Bridge Test

`coser_bridge.dll` is a small C ABI wrapper around `CoserRetrievalCore`. The
WPF project P/Invokes it at startup and computes the MD5 of a regression image
using the existing C++ `FileHasher`. The subtitle displays the native bridge
state and MD5 prefix. This validates real WPF-to-C++ linking without coupling
the UI to C++ class ABI.

Build native code first, then build/run the WPF project:

```powershell
cmake --build .\CoserRetrieval\build --config Debug --target coser_bridge
dotnet run --project .\CoserRetrieval.UI\CoserRetrieval.UI.csproj
```

The bridge is copied from `bin/coser_bridge.dll` into the WPF output directory
at build time. The current UI starts the CLI as a child process and maps its
stdout/stderr to list rows. Future work will expose task creation and
`TaskProgressHub` subscription through this bridge, replacing output parsing
with direct progress events.
