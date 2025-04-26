package com.novastera.llamacpprn;

import androidx.annotation.NonNull;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.Promise;
import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.turbomodule.core.interfaces.TurboModule;
import com.facebook.react.turbomodule.core.CallInvokerHolderImpl;

/**
 * Java wrapper for the LlamaCppRn TurboModule.
 * Optimized for the new React Native architecture only.
 */
@ReactModule(name = LlamaCppRnModule.NAME)
public class LlamaCppRnModule extends NativeLlamaCppRnSpec {
    public static final String NAME = "LlamaCppRn";

    static {
        try {
            // Load native library
            System.loadLibrary("llamacpprn");
        } catch (Exception e) {
            throw new RuntimeException("Failed to load native library", e);
        }
    }

    public LlamaCppRnModule(ReactApplicationContext reactContext) {
        super(reactContext);
    }

    @Override
    @NonNull
    public String getName() {
        return NAME;
    }

    @Override
    public void initLlama(ReadableMap params, Promise promise) {
        // Implementation using JSI/Turbo Module system directly in C++
        // This method is only called in fallback cases
        promise.reject("ERR_DIRECT_JSI", "This method should be called through JSI interface");
    }

    @Override
    public void loadLlamaModelInfo(String modelPath, Promise promise) {
        // Implementation using JSI/Turbo Module system directly in C++
        // This method is only called in fallback cases
        promise.reject("ERR_DIRECT_JSI", "This method should be called through JSI interface");
    }

    @Override
    public void jsonSchemaToGbnf(ReadableMap schema, Promise promise) {
        // Implementation using JSI/Turbo Module system directly in C++
        // This method is only called in fallback cases
        promise.reject("ERR_DIRECT_JSI", "This method should be called through JSI interface");
    }
} 