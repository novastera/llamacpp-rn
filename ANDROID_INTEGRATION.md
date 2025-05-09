# Android Integration for LlamaCppRn

This guide provides concise instructions for integrating `@novastera-oss/llamacpp-rn` in your React Native Android app.

## Prerequisites

- React Native 0.76.0 or higher
- New Architecture enabled
- Android minSdkVersion 26 or higher

## Basic Setup

1. **Enable New Architecture** in `android/gradle.properties`:

```properties
newArchEnabled=true
```

2. **Set minimum SDK version** in `android/app/build.gradle`:

```gradle
android {
    defaultConfig {
        minSdkVersion 26
        // other config
    }
}
```

3. **Install the package**:

```bash
npm install @novastera-oss/llamacpp-rn
```

4. **Run the codegen**:

Codegen should automatically run during the build. If you need to manually generate the bindings:

```bash
cd android
./gradlew generateCodegenArtifactsFromSchema
```

5. **Configure ABI filters** in your app's `android/app/build.gradle`:

```gradle
android {
    defaultConfig {
        ndk {
            abiFilters 'arm64-v8a', 'x86_64'
        }
    }
}
```

## Troubleshooting

### Common Issues

1. **Duplicate class errors**: Add the following to your app's `build.gradle`:

```gradle
configurations.all {
    exclude group: 'jakarta.activation', module: 'jakarta.activation-api'
    exclude group: 'com.sun.activation', module: 'javax.activation'
    exclude group: 'org.slf4j', module: 'slf4j-api'
}
```

2. **Clean build cache** if you run into strange build errors:

```bash
cd android
./gradlew clean
cd ..
rm -rf android/.gradle
```

3. **Manual module registration** (if auto-linking fails):

```java
// In MainApplication.java
import com.novastera.llamacpprn.LlamaCppRnPackage;

// Inside getPackages()
packages.add(new LlamaCppRnPackage());
``` 