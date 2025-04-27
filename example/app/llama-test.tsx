import * as React from 'react';
import { View, Text, StyleSheet, Button, ActivityIndicator, ScrollView, Platform, Alert } from 'react-native';
import * as FileSystem from 'expo-file-system';
import * as Asset from 'expo-asset';
// Using require instead of import to avoid TypeScript errors
const LlamaCppRn = require('@novastera-oss/llamacpp-rn');

export default function LlamaTest() {
  const [modelInfo, setModelInfo] = React.useState<any>(null);
  const [modelInfoLoading, setModelInfoLoading] = React.useState(false);
  const [modelInfoError, setModelInfoError] = React.useState<string | null>(null);
  
  const [context, setContext] = React.useState<any>(null);
  const [contextLoading, setContextLoading] = React.useState(false);
  const [contextError, setContextError] = React.useState<string | null>(null);
  
  const [completion, setCompletion] = React.useState<string | null>(null);
  const [completionLoading, setCompletionLoading] = React.useState(false);
  const [completionError, setCompletionError] = React.useState<string | null>(null);
  
  const [modelPath, setModelPath] = React.useState<string | null>(null);
  
  const [apiStatus, setApiStatus] = React.useState<string | null>(null);
  const [apiStatusLoading, setApiStatusLoading] = React.useState(false);
  const [apiStatusError, setApiStatusError] = React.useState<string | null>(null);
  
  const [bundleFileTest, setBundleFileTest] = React.useState<any>(null);
  const [bundleFileLoading, setBundleFileLoading] = React.useState(false);
  const [bundleFileError, setBundleFileError] = React.useState<string | null>(null);

  // Try to load the model using various methods
  const loadModel = async () => {
    setModelInfoLoading(true);
    setModelInfoError(null);
    setModelInfo(null);
    
    const modelName = 'Llama-3.2-1B-Instruct-Q4_K_M.gguf';
    const possiblePaths = [
      // 1. Try the direct project path (for development)
      `/Users/hassan/github/llamacpp-rn/example/ios/example/models/${modelName}`,
      
      // 2. Try app bundle paths (for iOS)
      `${FileSystem.bundleDirectory}models/${modelName}`,
      `models/${modelName}`,
      
      // 3. Try Android asset paths (for Android)
      `asset:/models/${modelName}`,
      `file:///android_asset/models/${modelName}`
    ];
    
    console.log('Trying to load model from possible paths...');
    
    for (const path of possiblePaths) {
      try {
        console.log(`Attempting path: ${path}`);
        const info = await LlamaCppRn.loadLlamaModelInfo(path);
        console.log('✅ Success! Model loaded from:', path);
        console.log('Model info:', info);
        
        setModelPath(path);
        setModelInfo(info);
        return true;
      } catch (error) {
        console.log(`❌ Failed to load from ${path}:`, error);
      }
    }
    
    // If all paths fail, try using Asset.loadAsync as a last resort
    try {
      console.log('Trying to load model as an asset...');
      
      // If we had the model in the assets folder, we could use:
      // const asset = Asset.fromModule(require('../assets/Mistral-7B-Instruct-v0.3.Q4_K_M.gguf'));
      // await asset.downloadAsync();
      // console.log('Asset downloaded to:', asset.localUri);
      
      // For now, we'll just show a message with instructions
      setModelInfoError('Could not find model in any expected location. Please ensure the model file is added to the Xcode project manually.');
      Alert.alert(
        'Model Not Found',
        'The model file could not be loaded. Please follow these steps:\n\n' +
        '1. Open the Xcode project\n' +
        '2. Right-click on the example project in the Navigator\n' +
        '3. Select "Add Files to example..."\n' +
        '4. Navigate to the models directory and select the model file\n' +
        '5. Make sure "Create folder references" is selected\n' +
        '6. Rebuild the project'
      );
    } catch (error) {
      console.error('Asset loading failed:', error);
      setModelInfoError(`Failed to load model: ${error instanceof Error ? error.message : String(error)}`);
    } finally {
      setModelInfoLoading(false);
    }
    
    return false;
  };
  // Simple test for bundle directory file access
  const testBundleDirectoryAccess = async () => {
    setBundleFileLoading(true);
    setBundleFileError(null);
    setBundleFileTest(null);
    
    try {
      // Read bundle directory
      console.log('Checking bundle directory...');
      const bundleDir = FileSystem.bundleDirectory || '';
      console.log('Bundle directory:', bundleDir);
      
      // List files in the bundle directory to see if any exist
      console.log('Listing files in bundle directory...');
      let bundleFiles: string[] = [];
      try {
        bundleFiles = await FileSystem.readDirectoryAsync(bundleDir || '');
        console.log('Bundle directory files:', bundleFiles);
      } catch (dirError) {
        console.log('Error reading bundle directory:', dirError);
      }
      
      // Create small test GGUF file in documents directory
      console.log('Creating test file in documents directory...');
      const docDir = FileSystem.documentDirectory;
      const testFilePath = `${docDir}test_model.gguf`;
      
      // Create a small binary file with GGUF magic
      const fileContent = new Uint8Array([
        // GGUF magic header
        0x47, 0x47, 0x55, 0x46, // "GGUF"
        // Add some dummy content to make it look like a GGUF file
        0x00, 0x00, 0x00, 0x01, // Version 1
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Some dummy data
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // Add more bytes to make it larger than the minimum size check
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        // Repeat this a few times to pass size checks
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
      ]);
      
      // Convert to base64 for file system write
      let binary = '';
      fileContent.forEach(byte => {
        binary += String.fromCharCode(byte);
      });
      const base64Data = btoa(binary);
      
      // Write file
      await FileSystem.writeAsStringAsync(testFilePath, base64Data, { 
        encoding: FileSystem.EncodingType.Base64 
      });
      console.log('Test file created at:', testFilePath);
      
      // Verify file exists
      const fileInfo = await FileSystem.getInfoAsync(testFilePath);
      console.log('File info:', fileInfo);
      
      // Try to get native path info
      let nativePathInfo = null;
      if (typeof LlamaCppRn.getAbsolutePath === 'function') {
        nativePathInfo = await LlamaCppRn.getAbsolutePath(testFilePath);
        console.log('Native path info:', nativePathInfo);
      }
      
      // Try to load model info (this will likely fail but should be handled gracefully)
      let modelInfoResult = null;
      try {
        console.log('Attempting to load model info from test file...' + testFilePath);
        modelInfoResult = await LlamaCppRn.loadLlamaModelInfo(testFilePath.replace('file://', ''));
        console.log('Test model info result:', modelInfoResult);
      } catch (modelError) {
        console.log('Error loading test model info (expected):', modelError);
        modelInfoResult = { error: modelError instanceof Error ? modelError.message : String(modelError) };
      }
      
      // Set results
      setBundleFileTest({
        bundleDir,
        bundleFiles,
        testFilePath,
        fileInfo,
        nativePathInfo,
        modelInfoResult,
      });
      
    } catch (error) {
      console.error('Error in bundle file test:', error);
      setBundleFileError(`Error: ${error instanceof Error ? error.message : String(error)}`);
    } finally {
      setBundleFileLoading(false);
    }
  };

  const handleLoadModelInfo = async () => {
    if (!modelPath) {
      setModelInfoError('Model file not found. Please make sure it was copied correctly.');
      return;
    }
    console.log('**Loading model info from:', modelPath);
    setModelInfoLoading(true);
    setModelInfoError(null);
    setModelInfo(null);
    
    try {
      console.log('Loading model info from:', modelPath);
      
      const info = await LlamaCppRn.loadLlamaModelInfo(modelPath);
      console.log('Model info loaded successfully:', info);
      setModelInfo(info);
    } catch (error) {
      console.error('Failed to load model info:', error);
      setModelInfoError(error instanceof Error ? error.message : String(error));
    } finally {
      setModelInfoLoading(false);
    }
  };

  const handleInitContext = async () => {
    if (!modelPath) {
      setContextError('Model file not found. Please make sure it was copied correctly.');
      return;
    }
    
    setContextLoading(true);
    setContextError(null);
    setContext(null);
    
    try {
      console.log('Initializing context with model:', modelPath);
      
      const ctx = await LlamaCppRn.initLlama({
        modelPath,
        contextSize: 2048,
        seed: 42,
        batchSize: 512,
        gpuLayers: 0, // Use CPU for compatibility
      });
      
      setContext(ctx);
      console.log('Context initialized:', ctx);
    } catch (error) {
      console.error('Failed to initialize context:', error);
      setContextError(error instanceof Error ? error.message : String(error));
    } finally {
      setContextLoading(false);
    }
  };

  const handleCompletion = async () => {
    if (!context) {
      setCompletionError('Context not initialized');
      return;
    }
    
    setCompletionLoading(true);
    setCompletionError(null);
    setCompletion(null);
    
    try {
      console.log('Starting completion with context:', context);
      
      const result = await context.completion({
        prompt: "What is artificial intelligence?",
        maxTokens: 256,
        temperature: 0.7,
        topP: 0.9
      });
      
      setCompletion(result.text);
      console.log('Completion result:', result);
    } catch (error) {
      console.error('Failed to generate completion:', error);
      setCompletionError(error instanceof Error ? error.message : String(error));
    } finally {
      setCompletionLoading(false);
    }
  };

  // Test if we can call the native module directly
  const testDirectAPICall = async () => {
    setApiStatusLoading(true);
    setApiStatusError(null);
    setApiStatus(null);
    
    try {
      console.log('Testing direct API call to native module...');
      
      // Try a simple call to the module
      if (typeof LlamaCppRn.jsonSchemaToGbnf === 'function') {
        const schema = { 
          type: 'object',
          properties: { 
            name: { type: 'string' },
            age: { type: 'number' }
          }
        };
        
        const gbnf = await LlamaCppRn.jsonSchemaToGbnf(schema);
        console.log('GBNF result:', gbnf);
        setApiStatus('API call successful: jsonSchemaToGbnf');
      } else {
        setApiStatus('jsonSchemaToGbnf function not available');
      }
    } catch (error) {
      console.error('Failed to make direct API call:', error);
      setApiStatusError(error instanceof Error ? error.message : String(error));
    } finally {
      setApiStatusLoading(false);
    }
  };

  // Add a new function to load a model file from assets
  const copyModelFromAssetsIfNeeded = async () => {
    setModelInfoLoading(true);
    setModelInfoError(null);
    setModelInfo(null);

    try {
      console.log('Checking for model file in app bundle and documents...');

      // Check bundle directory first
      const bundleDir = FileSystem.bundleDirectory;
      console.log('Bundle directory:', bundleDir);
      
      // Try to list files in the 'models' subdirectory of the bundle
      let bundleModelsDir = `${bundleDir}models/`;
      let bundleModelFiles: string[] = [];
      try {
        bundleModelFiles = await FileSystem.readDirectoryAsync(bundleModelsDir);
        console.log('Models in bundle:', bundleModelFiles);
      } catch (error) {
        console.log('Could not read models directory in bundle:', error);
      }

      // Check if we have any GGUF files
      const ggufFiles = bundleModelFiles.filter(file => file.endsWith('.gguf'));
      if (ggufFiles.length > 0) {
        console.log('Found GGUF files in bundle:', ggufFiles);
        
        // Use the first GGUF file
        const modelName = ggufFiles[0];
        const bundleModelPath = `${bundleModelsDir}${modelName}`;
        console.log('Using model:', bundleModelPath);
        
        // Try to load directly from bundle
        try {
          console.log('Trying to load model directly from bundle...');
          const info = await LlamaCppRn.loadLlamaModelInfo(bundleModelPath);
          console.log('Successfully loaded model from bundle:', info);
          
          setModelPath(bundleModelPath);
          setModelInfo(info);
          setModelInfoLoading(false);
          return true;
        } catch (error) {
          console.log('Failed to load from bundle directly:', error);
          // Continue to try copying and loading from documents
        }
        
        // Copy to documents directory
        const documentsDir = FileSystem.documentDirectory;
        const docsModelPath = `${documentsDir}${modelName}`;
        
        // Check if the file already exists in documents
        const fileInfo = await FileSystem.getInfoAsync(docsModelPath);
        if (fileInfo.exists) {
          console.log('Model already exists in documents directory:', docsModelPath);
          
          try {
            console.log('Loading model from documents directory...');
            const info = await LlamaCppRn.loadLlamaModelInfo(docsModelPath);
            console.log('Successfully loaded model from documents:', info);
            
            setModelPath(docsModelPath);
            setModelInfo(info);
            setModelInfoLoading(false);
            return true;
          } catch (error) {
            console.log('Failed to load from documents, will try copying again:', error);
            // Will re-copy file below
          }
        }
        
        // Copy the file
        console.log(`Copying model from bundle to documents: ${bundleModelPath} -> ${docsModelPath}`);
        await FileSystem.copyAsync({
          from: bundleModelPath,
          to: docsModelPath
        });
        
        // Verify the copy
        const copiedFileInfo = await FileSystem.getInfoAsync(docsModelPath);
        if (copiedFileInfo.exists) {
          console.log('File copied successfully, size:', copiedFileInfo.size);
          
          // Try to load from documents
          try {
            console.log('Loading model from newly copied file...');
            const info = await LlamaCppRn.loadLlamaModelInfo(docsModelPath);
            console.log('Successfully loaded model:', info);
            
            setModelPath(docsModelPath);
            setModelInfo(info);
            setModelInfoLoading(false);
            return true;
          } catch (error) {
            console.log('Failed to load copied model:', error);
            setModelInfoError(`Failed to load copied model: ${error instanceof Error ? error.message : String(error)}`);
          }
        } else {
          setModelInfoError('Failed to copy model file to documents directory');
        }
      } else {
        console.log('No GGUF files found in bundle models directory');
        
        // Create a small test GGUF file
        console.log('Creating a test GGUF file in documents directory...');
        const docDir = FileSystem.documentDirectory;
        const testModelPath = `${docDir}test_model.gguf`;
        
        // Create a larger binary file with more realistic GGUF format
        // This still won't be a real model but should pass more basic checks
        const createLargerTestFile = () => {
          // Start with the magic header
          const header = new Uint8Array([
            // GGUF magic header
            0x47, 0x47, 0x55, 0x46, // "GGUF"
            0x00, 0x00, 0x00, 0x01, // Version 1
          ]);
          
          // Add metadata section
          const metaLength = new Uint8Array(8); // 8 bytes for uint64 length
          const view = new DataView(metaLength.buffer);
          view.setBigUint64(0, BigInt(10), true); // 10 metadata entries
          
          // Generate a large block of data to make the file sizeable
          // ~1MB of data should be enough to pass basic file size checks
          const blockSize = 1024 * 1024; // 1MB
          const dataBlock = new Uint8Array(blockSize);
          
          // Fill with pseudorandom data
          for (let i = 0; i < blockSize; i++) {
            dataBlock[i] = i % 256;
          }
          
          // Add some GGUF-like structures
          // n_vocab, n_embd, n_mult, n_head values
          const modelParams = new Uint8Array([
            // Set some parameter values that might be read by llama.cpp
            0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // n_vocab (dummy value)
            0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // n_embd (dummy value)
            0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // n_mult (dummy value)
            0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // n_head (dummy value)
          ]);
          
          // Combine all parts
          const combinedLength = header.length + metaLength.length + modelParams.length + dataBlock.length;
          const combined = new Uint8Array(combinedLength);
          
          let offset = 0;
          combined.set(header, offset);
          offset += header.length;
          combined.set(metaLength, offset);
          offset += metaLength.length;
          combined.set(modelParams, offset);
          offset += modelParams.length;
          combined.set(dataBlock, offset);
          
          return combined;
        };
        
        const fileContent = createLargerTestFile();
        console.log(`Created test file with size: ${fileContent.length} bytes`);
        
        // Convert to base64 for file system write
        let binary = '';
        for (let i = 0; i < Math.min(fileContent.length, 1024 * 1024 * 10); i++) { // Limit to 10MB max for safety
          binary += String.fromCharCode(fileContent[i]);
        }
        const base64Data = btoa(binary);
        
        // Write file
        try {
          await FileSystem.writeAsStringAsync(testModelPath, base64Data, {
            encoding: FileSystem.EncodingType.Base64,
          });
          
          // Verify the file exists and check its size
          const fileInfo = await FileSystem.getInfoAsync(testModelPath);
          console.log('Test file created:', JSON.stringify(fileInfo));
          
          // Try to load the test model
          console.log('Attempting to load test model from:', testModelPath);
          
          // First try to get the native path if that function exists
          let nativePath = testModelPath;
          if (typeof LlamaCppRn.getAbsolutePath === 'function') {
            try {
              const pathInfo = await LlamaCppRn.getAbsolutePath(testModelPath);
              console.log('Native path info:', pathInfo);
              if (pathInfo && pathInfo.path) {
                nativePath = pathInfo.path;
                console.log('Using native path instead:', nativePath);
              }
            } catch (pathError) {
              console.error('Error getting native path:', pathError);
              // Continue with original path
            }
          }
          
          try {
            console.log('Loading model from path:', nativePath);
            const modelInfo = await LlamaCppRn.loadLlamaModelInfo(nativePath);
            console.log('Successfully loaded test model info:', JSON.stringify(modelInfo));
            return { success: true, path: nativePath, info: modelInfo };
          } catch (loadError: unknown) {
            console.error('Error loading test model:', loadError);
            // Log detailed error properties
            console.error('Error type:', typeof loadError);
            console.error('Error name:', (loadError as Error)?.name);
            console.error('Error message:', (loadError as Error)?.message);
            console.error('Error stack:', (loadError as Error)?.stack);
            if ((loadError as any)?.code) console.error('Error code:', (loadError as any).code);
            if ((loadError as any)?.userInfo) console.error('Error userInfo:', JSON.stringify((loadError as any).userInfo));
            
            // Try loading with explicit options
            console.log('Trying to load with explicit options...');
            try {
              // Note: this assumes loadLlamaModelInfo accepts a second parameter with options
              // If it doesn't, we'll catch another error
              const modelInfoWithOptions = await LlamaCppRn.loadLlamaModelInfo(nativePath, {
                useGPU: false, // Try without GPU
                verbose: true, // Enable verbose logging
              });
              console.log('Successfully loaded test model with options:', JSON.stringify(modelInfoWithOptions));
              return { success: true, path: nativePath, info: modelInfoWithOptions };
            } catch (optionsError: unknown) {
              console.error('Failed to load with options:', optionsError);
              return { success: false, path: nativePath, error: (optionsError as Error)?.message || String(optionsError) };
            }
          }
        } catch (writeError: unknown) {
          console.error('Error creating test file:', writeError);
          return { success: false, error: `Failed to create test file: ${(writeError as Error)?.message || String(writeError)}` };
        }
      }
    } catch (error) {
      console.error('Error in copy model operation:', error);
      setModelInfoError(`Error in copy model operation: ${error instanceof Error ? error.message : String(error)}`);
    }
    
    setModelInfoLoading(false);
    return false;
  };

  // Test with raw file path
  const testWithRawFilePath = async () => {
    setModelInfoLoading(true);
    setModelInfoError(null);
    setModelInfo(null);
    
    try {
      console.log('Testing with raw file path...');
      
      // Get the path to the model in the app bundle
      const bundleDir = FileSystem.bundleDirectory || '';
      const modelName = 'Llama-3.2-1B-Instruct-Q4_K_M.gguf';
      const bundlePath = `${bundleDir}${modelName}`;
      
      // Check if the file exists
      const fileInfo = await FileSystem.getInfoAsync(bundlePath);
      console.log('Bundle file info:', JSON.stringify(fileInfo));
      
      if (!fileInfo.exists) {
        console.log('Model not found in bundle, checking bundle directory contents...');
        try {
          const files = await FileSystem.readDirectoryAsync(bundleDir || '');
          console.log('Bundle directory contains:', files);
        } catch (e) {
          console.error('Error reading bundle directory:', e);
        }
        throw new Error('Model file not found in bundle');
      }
      
      // Extract raw file path from the URI
      // For iOS, the file URI can be converted to a raw path
      let rawPath = bundlePath;
      if (Platform.OS === 'ios' && bundlePath.startsWith('file://')) {
        rawPath = bundlePath.replace('file://', '');
      }
      console.log('Using raw path:', rawPath);
      
      // Try direct load with raw path
      try {
        console.log('Loading model with raw path:', rawPath);
        const modelInfo = await LlamaCppRn.loadLlamaModelInfo(rawPath);
        console.log('Successfully loaded model with raw path:', modelInfo);
        setModelInfo(modelInfo);
        setModelPath(rawPath);
        return true;
      } catch (error) {
        console.error('Error loading with raw path:', error);
        throw error;
      }
    } catch (error) {
      console.error('Raw path test failed:', error);
      setModelInfoError(`Raw path test failed: ${error instanceof Error ? error.message : String(error)}`);
    } finally {
      setModelInfoLoading(false);
    }
    
    return false;
  };

  // Test with a file at a known iOS path
  const testWithIOSPath = async () => {
    if (Platform.OS !== 'ios') {
      Alert.alert('iOS Only', 'This test is only for iOS devices');
      return;
    }
    
    setModelInfoLoading(true);
    setModelInfoError(null);
    setModelInfo(null);
    
    try {
      console.log('Testing with iOS-specific path...');
      
      // Get documents directory through FileSystem
      const docDir = FileSystem.documentDirectory;
      console.log('Documents directory:', docDir);
      
      // Create a test GGUF file in documents directory
      const testFileName = 'ios_test_model.gguf';
      const testFilePath = `${docDir}${testFileName}`;
      
      // Create a basic file with GGUF magic
      const fileData = new Uint8Array([
        // GGUF magic
        0x47, 0x47, 0x55, 0x46, // "GGUF"
        0x00, 0x00, 0x00, 0x01, // Version 1
        // Add 100+ bytes of data to pass size checks
        ...Array(200).fill(0).map((_, i) => i % 256)
      ]);
      
      // Convert to base64
      let binary = '';
      fileData.forEach(byte => {
        binary += String.fromCharCode(byte);
      });
      const base64 = btoa(binary);
      
      // Write the file
      console.log('Writing test file to:', testFilePath);
      await FileSystem.writeAsStringAsync(testFilePath, base64, {
        encoding: FileSystem.EncodingType.Base64
      });
      
      // Verify file exists
      const fileInfo = await FileSystem.getInfoAsync(testFilePath);
      console.log('Test file info:', JSON.stringify(fileInfo));
      
      if (!fileInfo.exists) {
        throw new Error('Failed to create test file');
      }
      
      // Convert to raw path
      let rawPath = testFilePath;
      if (testFilePath.startsWith('file://')) {
        rawPath = testFilePath.replace('file://', '');
      }
      console.log('Raw iOS path:', rawPath);
      
      // Try to get native path using the module
      if (typeof LlamaCppRn.getAbsolutePath === 'function') {
        try {
          const nativePath = await LlamaCppRn.getAbsolutePath(testFilePath);
          console.log('Native path from module:', nativePath);
          if (nativePath && nativePath.path) {
            console.log('Will use native path:', nativePath.path);
            rawPath = nativePath.path;
          }
        } catch (e) {
          console.error('Error getting native path:', e);
        }
      }
      
      // Try to load the model using the raw path
      try {
        console.log('Loading model from iOS path:', rawPath);
        // Log path components to check for any encoding issues
        console.log('Path components:', rawPath.split('/'));
        const modelInfo = await LlamaCppRn.loadLlamaModelInfo(rawPath);
        console.log('Successfully loaded model from iOS path:', modelInfo);
        setModelInfo(modelInfo);
        setModelPath(rawPath);
        return true;
      } catch (error) {
        console.error('Error loading model from iOS path:', error);
        
        // Try with explicit options as a last resort
        try {
          console.log('Trying with options object...');
          const modelInfo = await LlamaCppRn.loadLlamaModelInfo(rawPath, {
            useGPU: false,
            verbose: true
          });
          console.log('Successfully loaded with options:', modelInfo);
          setModelInfo(modelInfo);
          setModelPath(rawPath);
          return true;
        } catch (optError) {
          console.error('Failed with options too:', optError);
          throw error;
        }
      }
    } catch (error) {
      console.error('iOS path test failed:', error);
      setModelInfoError(`iOS path test failed: ${error instanceof Error ? error.message : String(error)}`);
    } finally {
      setModelInfoLoading(false);
    }
    
    return false;
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Llama.cpp Test</Text>
      
      <ScrollView style={styles.scrollView}>
        <View style={styles.section}>
          <Text style={styles.sectionTitle}>0. Test API</Text>
          <Button 
            title="Test Direct API Call" 
            onPress={testDirectAPICall} 
            disabled={apiStatusLoading}
          />
          
          <View style={styles.buttonSpacing} />
          
          <Button 
            title="Test Bundle File Access" 
            onPress={testBundleDirectoryAccess} 
            disabled={bundleFileLoading}
          />
          
          <View style={styles.buttonSpacing} />
          
          <Button 
            title="Test Simple File Access" 
            onPress={async () => {
              try {
                setApiStatusLoading(true);
                setApiStatus('Testing basic file access...');
                
                // Create a simple text file in the documents directory
                const docDir = FileSystem.documentDirectory || '';
                const testFilePath = `${docDir}test.txt`;
                
                // Write a simple text file
                console.log('Writing simple text file to:', testFilePath);
                await FileSystem.writeAsStringAsync(testFilePath, 'This is a test file.');
                
                // Verify file exists
                const fileInfo = await FileSystem.getInfoAsync(testFilePath);
                console.log('Text file info:', JSON.stringify(fileInfo));
                
                if (!fileInfo.exists) {
                  throw new Error('Failed to create text file');
                }
                
                // Try to read the file content using FileSystem
                const content = await FileSystem.readAsStringAsync(testFilePath);
                console.log('File content read via FileSystem:', content);
                
                // Try to read the same file using the native module if there's a readFile function
                const hasReadFunction = typeof LlamaCppRn.readFile === 'function';
                console.log('Native module has readFile function:', hasReadFunction);
                
                if (hasReadFunction) {
                  try {
                    const nativeContent = await LlamaCppRn.readFile(testFilePath);
                    console.log('File content read via native module:', nativeContent);
                    setApiStatus('Native module can read files! Check logs.');
                  } catch (e) {
                    console.error('Native module could not read the file:', e);
                    setApiStatus('Native module cannot read files. Check logs.');
                  }
                } else if (typeof LlamaCppRn.fileExists === 'function') {
                  // Try the fileExists function instead
                  try {
                    const exists = await LlamaCppRn.fileExists(testFilePath);
                    console.log('File exists according to native module:', exists);
                    setApiStatus(`File exists check: ${exists ? 'Success' : 'Failed'}`);
                  } catch (e) {
                    console.error('Error checking file existence:', e);
                    setApiStatus('Error checking file existence. Check logs.');
                  }
                } else {
                  setApiStatus('Native module has no file reading functions.');
                }
                
                // Try with path without file:// prefix
                if (testFilePath.startsWith('file://')) {
                  const rawPath = testFilePath.replace('file://', '');
                  console.log('Testing with raw path:', rawPath);
                  
                  if (hasReadFunction) {
                    try {
                      const nativeContent = await LlamaCppRn.readFile(rawPath);
                      console.log('File content read via native module (raw path):', nativeContent);
                      setApiStatus('Native module can read files with raw path! Check logs.');
                    } catch (e) {
                      console.error('Native module could not read the file with raw path:', e);
                    }
                  } else if (typeof LlamaCppRn.fileExists === 'function') {
                    try {
                      const exists = await LlamaCppRn.fileExists(rawPath);
                      console.log('File exists (raw path) according to native module:', exists);
                    } catch (e) {
                      console.error('Error checking file existence (raw path):', e);
                    }
                  }
                }
              } catch (error) {
                console.error('File access test error:', error);
                setApiStatusError(`File access test error: ${error instanceof Error ? error.message : String(error)}`);
              } finally {
                setApiStatusLoading(false);
              }
            }}
            disabled={apiStatusLoading}
          />
          
          <View style={styles.buttonSpacing} />
          
          <Button 
            title="Find and Load 1B Model" 
            onPress={loadModel} 
            disabled={modelInfoLoading}
          />
          
          <View style={styles.buttonSpacing} />
          
          <Button 
            title="Check and Copy Model File" 
            onPress={copyModelFromAssetsIfNeeded} 
            disabled={modelInfoLoading}
          />

          <View style={styles.buttonSpacing} />
          
          <Button 
            title="Test with Raw File Path" 
            onPress={testWithRawFilePath} 
            disabled={modelInfoLoading}
          />
          
          <View style={styles.buttonSpacing} />
          
          <Button 
            title="Test with iOS Path" 
            onPress={testWithIOSPath} 
            disabled={modelInfoLoading}
          />
          
          <View style={styles.buttonSpacing} />
          
          <Button 
            title="Test With Real Model" 
            onPress={async () => {
              try {
                setApiStatusLoading(true);
                console.log('Testing with real model file in bundle...');
                
                // We know from previous logs that this file exists in the bundle
                const modelName = 'Llama-3.2-1B-Instruct-Q4_K_M.gguf';
                const bundleDir = FileSystem.bundleDirectory || '';
                const bundlePath = `${bundleDir}${modelName}`;
                
                // First, verify the file exists
                const fileInfo = await FileSystem.getInfoAsync(bundlePath);
                console.log('Real model file info:', JSON.stringify(fileInfo));
                
                if (!fileInfo.exists) {
                  throw new Error('Real model file not found in bundle');
                }
                
                // Now try different path formats with the real model
                const rawBundlePath = bundlePath.replace('file://', '');
                console.log('Raw bundle path:', rawBundlePath);
                
                console.log('Trying to load with file:// path...');
                try {
                  const modelInfo = await LlamaCppRn.loadLlamaModelInfo(bundlePath);
                  console.log('SUCCESS with file:// path:', modelInfo);
                  setModelInfo(modelInfo);
                  setModelPath(bundlePath);
                  setApiStatus('Successfully loaded model with file:// path');
                  return;
                } catch (e) {
                  console.error('Failed with file:// path:', e);
                }
                
                console.log('Trying to load with raw path...');
                try {
                  const modelInfo = await LlamaCppRn.loadLlamaModelInfo(rawBundlePath);
                  console.log('SUCCESS with raw path:', modelInfo);
                  setModelInfo(modelInfo);
                  setModelPath(rawBundlePath);
                  setApiStatus('Successfully loaded model with raw path');
                  return;
                } catch (e) {
                  console.error('Failed with raw path:', e);
                }
                
                // If we couldn't load directly, try to copy to documents directory
                console.log('Copying model to documents directory...');
                const docsDir = FileSystem.documentDirectory || '';
                const docsPath = `${docsDir}${modelName}`;
                
                await FileSystem.copyAsync({
                  from: bundlePath,
                  to: docsPath
                });
                
                const docsInfo = await FileSystem.getInfoAsync(docsPath);
                console.log('Copied file info:', JSON.stringify(docsInfo));
                
                const rawDocsPath = docsPath.replace('file://', '');
                
                // Try to load from documents directory
                console.log('Trying to load from documents with file:// path...');
                try {
                  const modelInfo = await LlamaCppRn.loadLlamaModelInfo(docsPath);
                  console.log('SUCCESS from documents with file:// path:', modelInfo);
                  setModelInfo(modelInfo);
                  setModelPath(docsPath);
                  setApiStatus('Successfully loaded model from documents with file:// path');
                  return;
                } catch (e) {
                  console.error('Failed from documents with file:// path:', e);
                }
                
                console.log('Trying to load from documents with raw path...');
                try {
                  const modelInfo = await LlamaCppRn.loadLlamaModelInfo(rawDocsPath);
                  console.log('SUCCESS from documents with raw path:', modelInfo);
                  setModelInfo(modelInfo);
                  setModelPath(rawDocsPath);
                  setApiStatus('Successfully loaded model from documents with raw path');
                  return;
                } catch (e) {
                  console.error('Failed from documents with raw path:', e);
                  
                  // Log detailed error for raw docs path
                  console.log('Document path raw loading error details:');
                  if (e instanceof Error) {
                    console.log('  Error name:', e.name);
                    console.log('  Error message:', e.message);
                    console.log('  Error stack:', e.stack);
                    if ((e as any).code) console.log('  Error code:', (e as any).code);
                    if ((e as any).userInfo) console.log('  Error userInfo:', JSON.stringify((e as any).userInfo));
                  } else {
                    console.log('  Non-error object:', e);
                  }
                }
                
                setApiStatus('All loading attempts failed - check logs');
              } catch (error) {
                console.error('Real model test error:', error);
                setApiStatusError(`Real model test error: ${error instanceof Error ? error.message : String(error)}`);
              } finally {
                setApiStatusLoading(false);
              }
            }}
            disabled={apiStatusLoading}
          />
          
          <View style={styles.buttonSpacing} />
          
          <Button 
            title="Extract & Test Header" 
            onPress={async () => {
              try {
                setApiStatusLoading(true);
                console.log('Extracting header from real model file...');
                
                // We know from previous logs that this file exists in the bundle
                const modelName = 'Llama-3.2-1B-Instruct-Q4_K_M.gguf';
                const bundleDir = FileSystem.bundleDirectory || '';
                const bundlePath = `${bundleDir}${modelName}`;
                
                // First, verify the file exists
                const fileInfo = await FileSystem.getInfoAsync(bundlePath);
                console.log('Real model file info:', JSON.stringify(fileInfo));
                
                if (!fileInfo.exists) {
                  throw new Error('Real model file not found in bundle');
                }
                
                // Get documents directory
                const docsDir = FileSystem.documentDirectory || '';
                
                // Read a small chunk of the real model file (just the header)
                const modelChunk = await FileSystem.readAsStringAsync(bundlePath, {
                  encoding: FileSystem.EncodingType.Base64,
                  length: 32768, // Read first 32KB
                  position: 0
                });
                
                console.log(`Read ${modelChunk.length} bytes (base64) from model file`);
                
                // Write this chunk to a new file with .gguf extension
                const headerFilePath = `${docsDir}model_header.gguf`;
                await FileSystem.writeAsStringAsync(headerFilePath, modelChunk, {
                  encoding: FileSystem.EncodingType.Base64
                });
                
                const headerFileInfo = await FileSystem.getInfoAsync(headerFilePath);
                console.log('Header file info:', JSON.stringify(headerFileInfo));
                
                // Now try to load the header file
                const rawHeaderPath = headerFilePath.replace('file://', '');
                
                console.log('Trying to load header file with raw path...');
                try {
                  const headerInfo = await LlamaCppRn.loadLlamaModelInfo(rawHeaderPath);
                  console.log('SUCCESS loading header file!', headerInfo);
                  setModelInfo(headerInfo);
                  setModelPath(rawHeaderPath);
                  setApiStatus('Successfully loaded model header file');
                  return;
                } catch (e) {
                  console.error('Failed to load header file:', e);
                  
                  // Check if there's more specific error info
                  if (e instanceof Error) {
                    if (e.message.includes("file doesn't exist")) {
                      console.log('Header file path issue - trying different path formats');
                      
                      // Try a few different path formats for the header file
                      const pathsToTry = [
                        { name: 'Raw with double slash', path: rawHeaderPath.replace('/', '//') },
                        { name: 'Absolute only', path: '/model_header.gguf' },
                        { name: 'Documents folder only', path: `${docsDir}model_header.gguf` }
                      ];
                      
                      for (const pathInfo of pathsToTry) {
                        console.log(`Trying path format: ${pathInfo.name} - ${pathInfo.path}`);
                        try {
                          const result = await LlamaCppRn.loadLlamaModelInfo(pathInfo.path);
                          console.log(`SUCCESS with ${pathInfo.name}!`, result);
                          setModelInfo(result);
                          setModelPath(pathInfo.path);
                          setApiStatus(`Successfully loaded with ${pathInfo.name}`);
                          return;
                        } catch (pathError) {
                          console.error(`Failed with ${pathInfo.name}:`, pathError);
                        }
                      }
                    } else if (e.message.includes("Failed to load model")) {
                      console.log('Header file format issue - trying to create a more complete model header');
                      
                      // Let's try a larger chunk
                      const largerChunk = await FileSystem.readAsStringAsync(bundlePath, {
                        encoding: FileSystem.EncodingType.Base64,
                        length: 262144, // 256KB
                        position: 0
                      });
                      
                      console.log(`Read ${largerChunk.length} bytes (base64) from model file for larger chunk`);
                      
                      // Create a file with a larger chunk
                      const largerPath = `${docsDir}model_larger.gguf`;
                      await FileSystem.writeAsStringAsync(largerPath, largerChunk, {
                        encoding: FileSystem.EncodingType.Base64
                      });
                      
                      const largerFileInfo = await FileSystem.getInfoAsync(largerPath);
                      console.log('Larger file info:', JSON.stringify(largerFileInfo));
                      
                      const rawLargerPath = largerPath.replace('file://', '');
                      
                      console.log('Trying to load larger chunk...');
                      try {
                        const largerInfo = await LlamaCppRn.loadLlamaModelInfo(rawLargerPath);
                        console.log('SUCCESS loading larger chunk!', largerInfo);
                        setModelInfo(largerInfo);
                        setModelPath(rawLargerPath);
                        setApiStatus('Successfully loaded larger model chunk');
                        return;
                      } catch (largerError) {
                        console.error('Failed to load larger chunk:', largerError);
                      }
                    }
                  }
                }
                
                setApiStatus('All loading attempts failed - check logs');
              } catch (error) {
                console.error('Header extraction error:', error);
                setApiStatusError(`Header extraction error: ${error instanceof Error ? error.message : String(error)}`);
              } finally {
                setApiStatusLoading(false);
              }
            }}
            disabled={apiStatusLoading}
          />
          
          {apiStatusLoading && <ActivityIndicator style={styles.loader} />}
          {bundleFileLoading && <ActivityIndicator style={styles.loader} />}
          
          {apiStatusError && (
            <Text style={styles.error}>API Error: {apiStatusError}</Text>
          )}
          
          {bundleFileError && (
            <Text style={styles.error}>Bundle File Error: {bundleFileError}</Text>
          )}
          
          {apiStatus && (
            <View style={styles.resultBox}>
              <Text style={styles.resultTitle}>API Status:</Text>
              <Text style={styles.resultText}>{apiStatus}</Text>
            </View>
          )}
          
          {bundleFileTest && (
            <View style={styles.resultBox}>
              <Text style={styles.resultTitle}>Bundle File Test:</Text>
              <Text style={styles.resultText}>Bundle directory: {bundleFileTest.bundleDir}</Text>
              <Text style={styles.resultText}>Bundle files count: {bundleFileTest.bundleFiles?.length || 0}</Text>
              <Text style={styles.resultText}>Test file created: {bundleFileTest.fileInfo?.exists ? '✅' : '❌'}</Text>
              <Text style={styles.resultText}>Test file size: {bundleFileTest.fileInfo?.size || 0} bytes</Text>
              {bundleFileTest.nativePathInfo && (
                <Text style={styles.resultText}>Native path valid: {bundleFileTest.nativePathInfo.exists ? '✅' : '❌'}</Text>
              )}
              {bundleFileTest.modelInfoResult?.error ? (
                <Text style={styles.resultText}>Model info error (expected): ✓</Text>
              ) : (
                <Text style={styles.resultText}>Received model info: {bundleFileTest.modelInfoResult ? '✅' : '❌'}</Text>
              )}
            </View>
          )}
        </View>
        
        <View style={styles.section}>
          <Text style={styles.sectionTitle}>1. Load Model Info</Text>
          <Button 
            title="Load Model Info" 
            onPress={handleLoadModelInfo} 
            disabled={modelInfoLoading}
          />
          
          {modelInfoLoading && <ActivityIndicator style={styles.loader} />}
          
          {modelInfoError && (
            <Text style={styles.error}>Error: {modelInfoError}</Text>
          )}
          
          {modelInfo && (
            <View style={styles.resultBox}>
              <Text style={styles.resultTitle}>Model Info:</Text>
              <Text style={styles.resultText}>Parameters: {modelInfo.n_params}</Text>
              <Text style={styles.resultText}>Vocabulary: {modelInfo.n_vocab}</Text>
              <Text style={styles.resultText}>Context Size: {modelInfo.n_context}</Text>
              <Text style={styles.resultText}>Embedding: {modelInfo.n_embd}</Text>
              <Text style={styles.resultText}>Description: {modelInfo.description}</Text>
            </View>
          )}
        </View>
        
        <View style={styles.section}>
          <Text style={styles.sectionTitle}>2. Initialize Context</Text>
          <Button 
            title="Initialize Context" 
            onPress={handleInitContext} 
            disabled={contextLoading || !modelInfo}
          />
          
          {contextLoading && <ActivityIndicator style={styles.loader} />}
          
          {contextError && (
            <Text style={styles.error}>Error: {contextError}</Text>
          )}
          
          {context && (
            <View style={styles.resultBox}>
              <Text style={styles.resultTitle}>Context Initialized</Text>
              <Text style={styles.resultText}>Context is ready for use</Text>
            </View>
          )}
        </View>
        
        <View style={styles.section}>
          <Text style={styles.sectionTitle}>3. Generate Completion</Text>
          <Button 
            title="Generate Response" 
            onPress={handleCompletion} 
            disabled={completionLoading || !context}
          />
          
          {completionLoading && <ActivityIndicator style={styles.loader} />}
          
          {completionError && (
            <Text style={styles.error}>Error: {completionError}</Text>
          )}
          
          {completion && (
            <View style={styles.resultBox}>
              <Text style={styles.resultTitle}>AI Response:</Text>
              <Text style={styles.resultText}>{completion}</Text>
            </View>
          )}
        </View>
      </ScrollView>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    padding: 16,
    backgroundColor: '#f5f5f5',
    borderRadius: 8,
    marginTop: 20,
  },
  title: {
    fontSize: 18,
    fontWeight: 'bold',
    marginBottom: 16,
  },
  scrollView: {
    maxHeight: 500,
  },
  section: {
    backgroundColor: '#fff',
    padding: 16,
    borderRadius: 8,
    marginBottom: 16,
  },
  sectionTitle: {
    fontSize: 16,
    fontWeight: 'bold',
    marginBottom: 12,
  },
  loader: {
    marginTop: 12,
  },
  error: {
    color: 'red',
    marginTop: 12,
  },
  buttonSpacing: {
    height: 10,
  },
  resultBox: {
    marginTop: 16,
    padding: 12,
    backgroundColor: '#f0f7ff',
    borderRadius: 6,
    borderLeftWidth: 4,
    borderLeftColor: '#4a8eff',
  },
  resultTitle: {
    fontWeight: 'bold',
    marginBottom: 8,
  },
  resultText: {
    marginBottom: 4,
  },
}); 