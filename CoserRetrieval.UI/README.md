# CoserRetrieval.UI

Windows WPF prototype for the active CoserRetrieval system. It starts a real
`coser_cli ingest` regression run after the window loads and visualizes the
seven files in `CoserRetrieval/testdata/regression/`:

- complete ingest rows are green;
- face/clothing partial route failures are marked on their corresponding stage;
- a process or ingest failure marks every stage red.

The window is resizable and contains a two-direction scrollable task list. A
row shows a truncated file name with its full path in a tooltip, a source image
thumbnail, and the stages `Hash`, `Decode`, `Face`, `Segment`, `Embedding`, and
`Index`. Green is complete, blue with a marquee bar is running, gray is pending,
and red is failed.

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
