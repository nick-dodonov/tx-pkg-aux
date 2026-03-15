# Patches Directory

This directory contains patches for external Bazel dependencies that need modifications for local development.

## Structure

```
patches/
├── generate_patch.sh          # Universal patch generation script
├── BUILD.bazel                # Exports patch files
├── boost.asio.patch           # Generated patch file
├── boost.asio/                # boost.asio patch workspace
│   ├── source/BUILD.bazel     # Original BCR BUILD file
│   └── target/BUILD.bazel     # Modified BUILD file
```

## Current Patches

### boost.asio.patch
- Adds `-std=c++23` flag for C++20 coroutine support  
- Adds `-pthread` flag specifically for WASM platform (`@platforms//cpu:wasm32`)
- Patches both BUILD.bazel and adds android-specific config

## Limitations

Bazel's `single_version_override` has strict patch validation that makes MODULE.bazel patching difficult.
**Recommended solution**: Create module version *-tx.N in tx-kit-registry with proper MODULE.bazel including required dependencies.

## Usage

### Generating Patches

Use the universal `generate_patch.sh` script to create patches:

```bash
cd patches/
./generate_patch.sh <module-name>
```

Examples:
```bash
./generate_patch.sh boost.asio
```

### How It Works

1. **source/BUILD.bazel** - Contains original BCR overlay BUILD file
2. **target/BUILD.bazel** - Contains your modifications
3. **generate_patch.sh** - Creates unified diff patch from source → target
4. **<module>.patch** - Generated patch file referenced in MODULE.bazel

### Applying Patches

Patches are applied automatically via `single_version_override` in MODULE.bazel:

```python
bazel_dep(name = "boost.asio", version = "...")

single_version_override(
    module_name = "boost.asio",
    patches = ["//patches:boost.asio.patch"],
    patch_strip = 0,
)
```

### Adding New Patches

1. Create module directory: `mkdir -p <module-name>/{source,target}`
2. Copy original BUILD.bazel to `<module-name>/source/BUILD.bazel`
3. Copy and modify to `<module-name>/target/BUILD.bazel`
4. Generate patch: `./generate_patch.sh <module-name>`
5. Add patch to `BUILD.bazel` exports
6. Add `single_version_override` to MODULE.bazel

## Development Workflow

These patches are **temporary** for local development. When ready:

1. Test patches work locally
2. Create new version in tx-kit-registry with changes
3. Update integrity hashes
4. Remove `single_version_override` from MODULE.bazel
5. Update version reference to new registry version
