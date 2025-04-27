# Using and Packaging Prebuilt Binaries

This package supports using prebuilt binaries to make installation and usage easier, especially for users who may not have the necessary build tools installed.

## How Prebuilt Binaries Work

The package follows this sequence when setting up:

1. First, it checks for **pre-packaged binaries** included in the npm package
2. If not found, it tries to **download prebuilt binaries** from GitHub releases
3. As a last resort, it will **build from source** (requires appropriate build tools)

## For Users

As a user of this package, you generally don't need to worry about the binaries. When you install the package:

```bash
npm install @novastera-oss/llamacpp-rn
# or
yarn add @novastera-oss/llamacpp-rn
```

The postinstall script will automatically setup the libraries using the above sequence.

## For Contributors and Publishers

If you're contributing to this package or publishing a fork, you should include prebuilt binaries to make installation easier for your users.

### Generating Prebuilt Binaries

1. Make sure the iOS prebuilt binaries are downloaded or built:
   ```bash
   yarn setup-llama-cpp
   ```

2. For Android, you'll need to build the libraries yourself, which requires the Android NDK.

3. Once you have all binaries ready, package them for distribution:
   ```bash
   yarn package-prebuilt
   ```

   This will copy the binaries to the `prebuilt/` directory.

4. Commit these files to your repository:
   ```bash
   git add prebuilt/
   git commit -m "Add prebuilt binaries for version X.Y.Z"
   ```

5. When you publish your npm package, the prebuilt binaries will be included.

### File Size Considerations

iOS XCFrameworks and Android libraries can be large. Consider:

- Hosting large binaries on a separate server if they exceed npm's size limits
- Using Git LFS for large binary files
- Providing clear documentation on how to build from source for users who want to customize

## Customizing the Build

Users can always choose to build from source by setting the `LLAMACPPRN_BUILD_FROM_SOURCE` environment variable:

```bash
LLAMACPPRN_BUILD_FROM_SOURCE=true npm install @novastera-oss/llamacpp-rn
# or 
LLAMACPPRN_BUILD_FROM_SOURCE=true yarn add @novastera-oss/llamacpp-rn
```

This will skip the prebuilt binaries and build directly from the llama.cpp source code. 