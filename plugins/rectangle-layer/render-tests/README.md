# Rectangle render tests

This directory owns the rectangle plugin's manifest, fixtures, and expected images. It is discovered by the repository-level runner documented in [`render-tests/README.md`](../../../render-tests/README.md).

The fixtures cover source-driven values, fractional-zoom composite expressions,
feature-state updates, and runtime paint-property replacement. They are ordinary
MapLibre render tests; the shared runner contains no rectangle-specific cases.

Run only this plugin's manifest from the repository root with:

```sh
build-Metal/plugin-render-tests \
  --manifestPath plugins/rectangle-layer/render-tests/manifest.json
```
