# MAX31865 implementation prompts

Prepared 2026-10-25 from current main
`9937c886d5617bc8deca1d7db148a3fcf286a377` and retained IDF tip
`5263d9032b0185604696f777ef136558a8e3eb6e`. These are executable implementation
instructions for follow-up work. **Status: NOT IMPLEMENTED.** Writing these
prompts did not change the driver, adapter, package contract or CI.

Use each numbered document as the implementation request, in order, on the
same existing branch. Re-read the current source if main has moved; the SHAs
are evidence anchors, not instructions to reset the working tree. Execute
the selected scope and tests through completion rather than returning a plan.

| Audit item | Prompt | Current status / scope |
| --- | --- | --- |
| R1, callback safety, native ownership and SDK limitations | [01 Native adapter](01_NATIVE_ADAPTER.md) | NOT IMPLEMENTED. Deliver an example-owned adapter conforming to the current transport, with behavioral and actual SDK build evidence. |
| R1, maintained application, packaging, clean native consumers and CI | [02 Native application and integration](02_NATIVE_APPLICATION_AND_CI.md) | NOT IMPLEMENTED. Depends on 01's supported terminal-transfer path. Ship a native example and S2/S3 build gates. |
| R2, physical qualification | No software prompt | Separate hardware acceptance using `../HARDWARE_BRINGUP.md` and `../TESTING.md`; no hardware result is inferred from this prompt set. |
| Historical neutral-core refactor, component, wrapper | No implementation prompt | Already implemented/equivalent, as documented in [the retained review](../RETAINED_BRANCH_AUDIT.md). Do not restore old API aliases or merge the retained branch wholesale. |

R1 was an intentional difference in product scope, not a current core defect.
Executing prompt 01 explicitly elects to add a maintained native **example
adapter** and authorizes the necessary narrow updates to AGENTS and boundary
rules. Keep the installed core component framework-neutral and callback-only.
Do not stop at asking whether to expand scope, but do not claim completion if
the chosen SDK cannot implement the terminal, bounded callback contract.

The default architecture is a small adapter under `examples/esp_idf/` borrowing
application handles. Root `CMakeLists.txt` remains the canonical two-source
neutral component. No new public backend, bus manager, retry engine, queue
service, or Arduino compatibility layer is required.

SDK evidence checked while preparing these prompts: installed ESP32-S3
ESP-IDF 5.5.5 headers and the versioned official SPI implementation linked in
01. That is API/source evidence only, not a native target build. Prompt 01
must settle actual transfer completion and cleanup before 02 advertises a
working adapter. Merely putting a deadline around a non-cancellable SDK call
does not meet the contract.

For each implementation, inspect existing workflows first; preserve and run
the native/static/sanitizer/Arduino/package/documentation gates appropriate
to the change. Verify CI for the exact final implementation commit, commit
only owned work, push normally and confirm upstream synchronization. Update
this index and the retained review with software closure evidence; keep
hardware acceptance separate. No branch deletion or release is included.
