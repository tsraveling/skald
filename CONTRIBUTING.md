## Dev flow when making an update

Turn off `SKALD_BUILD_LSP` (the C part of the LSP), `_SKALDER` (the TUI tester), and `_SHARED` (the C binding). Once the core parser is compiling again, turn them on one by one. Recommend starting with Skalder so you can test your changes before committing changes to the bindings and LSP code (which are more boilerplate anyway).
