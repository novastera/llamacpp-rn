import * as React from 'react';
import { View, Text, StyleSheet, Button, ActivityIndicator, ScrollView, Platform, Alert, TouchableOpacity } from 'react-native';
import * as FileSystem from 'expo-file-system';
import * as Asset from 'expo-asset';
// Using require instead of import to avoid TypeScript errors
const LlamaCppRn = require('@novastera-oss/llamacpp-rn');

// Define interface for file info
interface ExtendedFileInfo {
  exists: boolean;
  uri: string;
  isDirectory: boolean;
  modificationTime: number;
  size?: number;
}

export default function LlamaTest() {
  const [fileTest, setFileTest] = React.useState<any>(null);
  const [fileLoading, setFileLoading] = React.useState(false);
  const [fileError, setFileError] = React.useState<string | null>(null);
  
  const [modelInfo, setModelInfo] = React.useState<any>(null);
  const [modelLoading, setModelLoading] = React.useState(false);
  const [modelError, setModelError] = React.useState<string | null>(null);
  
  const [schemaResult, setSchemaResult] = React.useState<string | null>(null);
  const [schemaLoading, setSchemaLoading] = React.useState(false);
  const [schemaError, setSchemaError] = React.useState<string | null>(null);
  
  // Add API status state for simple file test
  const [apiStatus, setApiStatus] = React.useState<string | null>(null);
  const [simpleFileLoading, setSimpleFileLoading] = React.useState(false);

  const [minimalSettingsLoading, setMinimalSettingsLoading] = React.useState(false);

  // Test file system access
  const testFileAccess = async () => {
    setFileLoading(true);
    setFileError(null);
    setFileTest(null);
    
    try {
      console.log('Testing file system access...');
      
      // Get basic directory information
      const bundleDir = FileSystem.bundleDirectory || '';
      const documentsDir = FileSystem.documentDirectory || '';
      
      console.log('Bundle directory:', bundleDir);
      console.log('Documents directory:', documentsDir);
      
      // Create test file in documents directory
      const testFilePath = `${documentsDir}test_file.txt`;
      await FileSystem.writeAsStringAsync(testFilePath, 'This is a test file');
      
      // Verify file exists
      const fileInfo = await FileSystem.getInfoAsync(testFilePath) as ExtendedFileInfo;
      console.log('Test file info:', fileInfo);
      
      // Test file exists function if available
      let fileExistsResult = null;
      if (typeof LlamaCppRn.fileExists === 'function') {
        try {
          fileExistsResult = await LlamaCppRn.fileExists(testFilePath);
          console.log('File exists (via native module):', fileExistsResult);
        } catch (error) {
          console.error('Error checking if file exists:', error);
        }
      }
      
      // List files in bundle directory
      let bundleFiles: string[] = [];
      try {
        bundleFiles = await FileSystem.readDirectoryAsync(bundleDir);
        console.log('Bundle directory files:', bundleFiles);
      } catch (error) {
        console.error('Error listing bundle directory:', error);
      }
      
      // Look for model files
      const ggufFiles = bundleFiles.filter(file => file.endsWith('.gguf'));
      console.log('GGUF files in bundle:', ggufFiles);
      
      setFileTest({
        bundleDir,
        documentsDir,
        testFilePath,
        fileExists: fileInfo.exists,
        fileSize: fileInfo.size,
        fileExistsResult,
        bundleFiles,
        ggufFiles
      });
    } catch (error) {
      console.error('File access test failed:', error);
      setFileError(`Error: ${error instanceof Error ? error.message : String(error)}`);
    } finally {
      setFileLoading(false);
    }
  };

  // Test loading a model
  const testModelLoading = async (useFakeModel = false) => {
    setModelLoading(true);
    setModelError(null);
    setModelInfo(null);
    
    try {
      console.log(`Testing model loading with ${useFakeModel ? 'fake' : 'real'} model...`);
      
      let modelPath = '';
      
      if (useFakeModel) {
        // Create a fake model file for testing
        const documentsDir = FileSystem.documentDirectory || '';
        modelPath = `${documentsDir}fake_model.gguf`;
        
        // Create minimal GGUF header
        const headerData = new Uint8Array([
          // GGUF magic
          0x47, 0x47, 0x55, 0x46, // "GGUF"
          0x00, 0x00, 0x00, 0x01, // Version 1
          // Add some dummy data to get past minimal checks
          ...Array(256).fill(0).map((_, i) => i % 256)
        ]);
        
        // Convert to Base64 for writing
        let binary = '';
        headerData.forEach(byte => {
          binary += String.fromCharCode(byte);
        });
        const base64Data = btoa(binary);
        
        try {
          // Write the fake model file
          await FileSystem.writeAsStringAsync(modelPath, base64Data, {
            encoding: FileSystem.EncodingType.Base64
          });
          
          const fileInfo = await FileSystem.getInfoAsync(modelPath) as ExtendedFileInfo;
          console.log('Fake model file created:', fileInfo);
        } catch (writeError) {
          console.error('Error creating fake model:', writeError);
          throw new Error(`Failed to create fake model: ${writeError instanceof Error ? writeError.message : String(writeError)}`);
        }
      } else {
        // Look for real models in the bundle
        const bundleDir = FileSystem.bundleDirectory || '';
        const documentsDir = FileSystem.documentDirectory || '';
        
        console.log('Bundle directory (exact):', bundleDir);
        console.log('Documents directory (exact):', documentsDir);
        
        // First check if there are any GGUF files in bundle
        let bundleFiles: string[] = [];
        try {
          bundleFiles = await FileSystem.readDirectoryAsync(bundleDir);
          console.log('Bundle directory files:', bundleFiles);
          
          // Specifically look for model files by known names
          const modelNames = [
            'Llama-3.2-1B-Instruct-Q4_K_M.gguf',
            'Mistral-7B-Instruct-v0.3.Q4_K_M.gguf'
          ];
          
          for (const name of modelNames) {
            if (bundleFiles.includes(name)) {
              modelPath = `${bundleDir}${name}`;
              console.log('Found specific model in bundle root:', modelPath);
              break;
            }
          }
        } catch (error) {
          console.log('Could not read bundle directory:', error);
        }
        
        // If we found a model by name in the bundle, use it
        if (modelPath) {
          console.log('Using model found in bundle:', modelPath);
        } 
        // Otherwise check for any .gguf file
        else {
          const ggufFiles = bundleFiles.filter(file => file.endsWith('.gguf'));
          
          if (ggufFiles.length > 0) {
            // Use the first GGUF file found
            modelPath = `${bundleDir}${ggufFiles[0]}`;
            console.log('Found GGUF model in bundle:', modelPath);
          } else {
            // Check for models in the assets directory
            try {
              // First check if there are models in the example/assets directory
              try {
                const assetsDir = `${bundleDir}assets/`;
                console.log('Checking assets directory:', assetsDir);
                const assetFiles = await FileSystem.readDirectoryAsync(assetsDir);
                console.log('Asset directory files:', assetFiles);
                
                const assetGgufFiles = assetFiles.filter(file => file.endsWith('.gguf'));
                if (assetGgufFiles.length > 0) {
                  modelPath = `${assetsDir}${assetGgufFiles[0]}`;
                  console.log('Found model in assets directory:', modelPath);
                  return;
                }
              } catch (assetsDirError) {
                console.log('Could not read assets directory:', assetsDirError);
              }
              
              // Check the documents/assets directory
              const assetDir = FileSystem.documentDirectory + 'assets/';
              await FileSystem.makeDirectoryAsync(assetDir, { intermediates: true }).catch(() => {});
              
              // Copy models from bundle to documents directory if they don't exist
              const llamaModelDestPath = assetDir + 'Llama-3.2-1B-Instruct-Q4_K_M.gguf';
              const mistralModelDestPath = assetDir + 'Mistral-7B-Instruct-v0.3.Q4_K_M.gguf';
              
              // Check if the models are already in the assets directory
              const llamaExists = await FileSystem.getInfoAsync(llamaModelDestPath);
              const mistralExists = await FileSystem.getInfoAsync(mistralModelDestPath);
              
              if (llamaExists.exists) {
                console.log('Found Llama model in assets directory');
                modelPath = llamaModelDestPath;
                return;
              } else if (mistralExists.exists) {
                console.log('Found Mistral model in assets directory');
                modelPath = mistralModelDestPath;
                return;
              }

              // Try to create a minimal test model in the documents directory
              // This ensures we at least have something to test with
              console.log('No existing models found, creating a minimal test model');
              const testModelPath = `${documentsDir}test_model.gguf`;
              
              // Create minimal GGUF header with version 3
              const header = new Uint8Array([
                0x47, 0x47, 0x55, 0x46, // Magic: GGUF
                0x00, 0x00, 0x00, 0x03, // Version 3
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // KV count (0)
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Tensor count (0)
                // Add some extra bytes to make it look more realistic
                0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10
              ]);
              
              try {
                await FileSystem.writeAsStringAsync(testModelPath, String.fromCharCode(...header), {
                  encoding: FileSystem.EncodingType.UTF8
                });
                const fileInfo = await FileSystem.getInfoAsync(testModelPath);
                console.log('Created test model file:', fileInfo);
                
                if (fileInfo.exists) {
                  modelPath = testModelPath;
                  console.log('Using created test model:', modelPath);
                  return;
                }
              } catch (writeError) {
                console.error('Failed to create test model:', writeError);
              }
              
              console.log('No models found in assets directory');
              
              // If we get here, no models were found anywhere
              throw new Error('No GGUF models found in bundle, models directory, or assets');
            } catch (overallError) {
              console.error('Error finding models:', overallError);
              throw new Error('No GGUF models found. Please add a model to your app bundle or assets directory.');
            }
          }
        }
      }
      
      // Try loading with file:// path
      console.log('Attempting to load model from path:', modelPath);
      try {
        const info = await LlamaCppRn.loadLlamaModelInfo(modelPath);
        console.log('Model loaded successfully:', info);
        
        // Check if we should try with GPU
        const gpuTestResult = info.gpuSupported ? 
          'GPU support available - you can enable it for inference' : 
          'GPU support not available on this device/build';
        
        // On simulator, add a note about GPU not working
        const isSimulator = modelPath.includes('Simulator');
        const gpuSimulatorNote = isSimulator && info.gpuSupported ? 
          ' (Note: GPU acceleration typically fails on simulators, but should work on real devices)' : '';
        
        setModelInfo({
          path: modelPath,
          info: info,
          rawPathUsed: false,
          gpuSupport: gpuTestResult + gpuSimulatorNote,
          timestamp: new Date().toISOString()
        });
        return;
      } catch (error) {
        console.error('Failed to load with file:// path:', error);
        // Continue with raw path
      }
      
      // Try loading with raw path (no file:// prefix)
      const rawPath = modelPath.replace('file://', '');
      console.log('Attempting to load model with raw path:', rawPath);
      try {
        const info = await LlamaCppRn.loadLlamaModelInfo(rawPath);
        console.log('Model loaded successfully with raw path:', info);
        
        // Check if we should try with GPU
        const gpuTestResult = info.gpuSupported ? 
          'GPU support available - you can enable it for inference' : 
          'GPU support not available on this device/build';
          
        // On simulator, add a note about GPU not working
        const isSimulator = rawPath.includes('Simulator');
        const gpuSimulatorNote = isSimulator && info.gpuSupported ? 
          ' (Note: GPU acceleration typically fails on simulators, but should work on real devices)' : '';
        
        setModelInfo({
          path: rawPath,
          info: info,
          rawPathUsed: true,
          gpuSupport: gpuTestResult + gpuSimulatorNote,
          timestamp: new Date().toISOString()
        });
        return;
      } catch (loadError) {
        // Log detailed error information
        console.error('Failed to load with raw path:', loadError);
        console.error('Error type:', typeof loadError);
        console.error('Error message:', loadError instanceof Error ? loadError.message : String(loadError));
        console.error('Error stack:', loadError instanceof Error ? loadError.stack : 'No stack');
        
        // Check for specific error properties
        if (loadError instanceof Error) {
          const errorObj = loadError as any;
          if (errorObj.code) console.error('Error code:', errorObj.code);
          if (errorObj.userInfo) console.error('Error userInfo:', errorObj.userInfo);
        }
        
        throw loadError;
      }
    } catch (error) {
      console.error('Model loading test failed:', error);
      setModelError(`Error: ${error instanceof Error ? error.message : String(error)}`);
    } finally {
      setModelLoading(false);
    }
  };

  // Test schema conversion
  const testSchemaConversion = async () => {
    setSchemaLoading(true);
    setSchemaError(null);
    setSchemaResult(null);
    
    try {
      console.log('Testing JSON schema to GBNF conversion...');
      
      // Simple JSON schema
      const schema = {
        type: 'object',
        properties: {
          name: { type: 'string' },
          age: { type: 'number' },
          isActive: { type: 'boolean' },
          tags: {
            type: 'array',
            items: { type: 'string' }
          },
          address: {
            type: 'object',
            properties: {
              street: { type: 'string' },
              city: { type: 'string' }
            },
            required: ['street', 'city']
          }
        },
        required: ['name', 'age']
      };
      
      // Call the conversion function
      const result = await LlamaCppRn.jsonSchemaToGbnf({schema: JSON.stringify(schema)});
      console.log('Schema conversion result:', result);
      
      setSchemaResult(result);
    } catch (error) {
      console.error('Schema conversion test failed:', error);
      setSchemaError(`Error: ${error instanceof Error ? error.message : String(error)}`);
    } finally {
      setSchemaLoading(false);
    }
  };

  // Test with a simple file
  const testWithSimpleFile = async () => {
    setSimpleFileLoading(true);
    setApiStatus(null);
    
    try {
      // Get the documents directory path
      const documentsDir = FileSystem.documentDirectory || '';
      console.log('[testWithSimpleFile] Documents directory:', documentsDir);
      
      // Create the test file path
      const testFilePath = `${documentsDir}test_file.gguf`;
      console.log('[testWithSimpleFile] Test file path:', testFilePath);
      
      // Check if the native module is available
      console.log('[testWithSimpleFile] Native module available:', !!LlamaCppRn);
      console.log('[testWithSimpleFile] Available functions:', Object.keys(LlamaCppRn).join(', '));
      console.log('[testWithSimpleFile] loadLlamaModelInfo type:', typeof LlamaCppRn.loadLlamaModelInfo);
      
      // Create minimal GGUF header with version 3
      const header = new Uint8Array([
        0x47, 0x47, 0x55, 0x46, // Magic: GGUF
        0x00, 0x00, 0x00, 0x03, // Version 3
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // KV count (0)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Tensor count (0)
        // Add some extra bytes to make it look more realistic
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10
      ]);
      
      try {
        // Convert the binary data to a string that can be written
        const headerStr = Array.from(header).map(byte => String.fromCharCode(byte)).join('');
        
        // Write the file with UTF8 encoding
        console.log('[testWithSimpleFile] Attempting to write test file...');
        await FileSystem.writeAsStringAsync(testFilePath, headerStr, {
          encoding: FileSystem.EncodingType.UTF8
        });
        
        // Verify the file exists
        const fileInfo = await FileSystem.getInfoAsync(testFilePath);
        console.log('[testWithSimpleFile] File created successfully:', JSON.stringify(fileInfo));
        
        // Verify with native module if possible
        if (LlamaCppRn.fileExists) {
          const exists = await LlamaCppRn.fileExists(testFilePath);
          console.log('[testWithSimpleFile] Native module fileExists check:', exists);
        }
        
        // Log more details about the file
        if (fileInfo && fileInfo.exists) {
          console.log(`[testWithSimpleFile] File size: ${fileInfo.size} bytes`);
          console.log(`[testWithSimpleFile] File URI: ${fileInfo.uri}`);
          if (fileInfo.modificationTime) {
            console.log(`[testWithSimpleFile] Modified: ${new Date(fileInfo.modificationTime).toISOString()}`);
          }
        }
        
        setApiStatus('File created successfully. Attempting to load model info...');
      } catch (writeError: any) {
        console.error('[testWithSimpleFile] Error writing test file:',
          writeError instanceof Error ? {
            name: writeError.name,
            message: writeError.message,
            stack: writeError.stack,
            ...Object.fromEntries(
              Object.getOwnPropertyNames(writeError)
                .filter(prop => prop !== 'name' && prop !== 'message' && prop !== 'stack')
                .map(prop => [prop, (writeError as any)[prop]])
            )
          } : writeError
        );
        setApiStatus(`Failed to write test file: ${writeError instanceof Error ? writeError.message : String(writeError)}`);
        setSimpleFileLoading(false);
        return;
      }
      
      // First try with default options
      try {
        console.log('[testWithSimpleFile] Attempting to load model info with path:', testFilePath);
        console.log('[testWithSimpleFile] Using default options');
        
        const modelInfo = await LlamaCppRn.loadLlamaModelInfo(testFilePath);
        console.log('[testWithSimpleFile] SUCCESS! Model info loaded:', modelInfo);
        setApiStatus(`Successfully loaded model info: ${JSON.stringify(modelInfo)}`);
      } catch (loadError: any) {
        console.error('[testWithSimpleFile] Error loading model info (default options):',
          loadError instanceof Error ? {
            type: typeof loadError,
            constructor: loadError.constructor?.name,
            name: loadError.name,
            message: loadError.message,
            stack: loadError.stack,
            code: (loadError as any).code,
            userInfo: (loadError as any).userInfo,
            nativeStack: Platform.OS === 'ios' 
              ? (loadError as any).nativeStackIOS 
              : (loadError as any).nativeStackAndroid,
            ...Object.fromEntries(
              Object.getOwnPropertyNames(loadError)
                .filter(prop => !['name', 'message', 'stack', 'code', 'userInfo', 'nativeStackIOS', 'nativeStackAndroid'].includes(prop))
                .map(prop => [prop, (loadError as any)[prop]])
            )
          } : loadError
        );
        
        // Try again with explicit options (disable GPU)
        try {
          const options = {
            n_gpu_layers: 0,
            verbose: true,
            embedding: false,
            n_threads: 1,
            n_batch: 8
          };
          console.log('[testWithSimpleFile] Attempting with explicit options:', options);
          
          const modelInfo = await LlamaCppRn.loadLlamaModelInfo(testFilePath, options);
          console.log('[testWithSimpleFile] SUCCESS with options! Model info loaded:', modelInfo);
          setApiStatus(`Successfully loaded model info with options: ${JSON.stringify(modelInfo)}`);
        } catch (optionsError: any) {
          console.error('[testWithSimpleFile] Error loading model info (with options):',
            optionsError instanceof Error ? {
              type: typeof optionsError,
              constructor: optionsError.constructor?.name,
              name: optionsError.name,
              message: optionsError.message,
              stack: optionsError.stack,
              code: (optionsError as any).code,
              userInfo: (optionsError as any).userInfo,
              nativeStack: Platform.OS === 'ios' 
                ? (optionsError as any).nativeStackIOS 
                : (optionsError as any).nativeStackAndroid,
              ...Object.fromEntries(
                Object.getOwnPropertyNames(optionsError)
                  .filter(prop => !['name', 'message', 'stack', 'code', 'userInfo', 'nativeStackIOS', 'nativeStackAndroid'].includes(prop))
                  .map(prop => [prop, (optionsError as any)[prop]])
              )
            } : optionsError
          );
          
          // Try checking if module can see the file directly
          try {
            if (LlamaCppRn.fileExists) {
              const fileExists = await LlamaCppRn.fileExists(testFilePath);
              console.log(`[testWithSimpleFile] Final check - file exists (native): ${fileExists}`);
              
              setApiStatus(`Failed to load model info despite file existing (native check: ${fileExists}): ${loadError instanceof Error ? loadError.message : String(loadError)}`);
            } else {
              setApiStatus(`Failed to load model info: ${loadError instanceof Error ? loadError.message : String(loadError)}`);
            }
          } catch (finalError) {
            console.error('[testWithSimpleFile] Final file check error:', finalError);
            setApiStatus(`Failed to load model info: ${loadError instanceof Error ? loadError.message : String(loadError)}`);
          }
        }
      }
    } catch (error: any) {
      console.error('[testWithSimpleFile] Unexpected error:',
        error instanceof Error ? {
          type: typeof error,
          constructor: error.constructor?.name,
          name: error.name,
          message: error.message,
          stack: error.stack,
          ...Object.fromEntries(
            Object.getOwnPropertyNames(error)
              .filter(prop => prop !== 'name' && prop !== 'message' && prop !== 'stack')
              .map(prop => [prop, (error as any)[prop]])
          )
        } : error
      );
      setApiStatus(`Error: ${error instanceof Error ? error.message : String(error)}`);
    } finally {
      setSimpleFileLoading(false);
    }
  };

  // Basic test function for the native module
  const testModuleImport = async () => {
    setFileLoading(true);
    setFileError(null);
    setFileTest(null);
    
    try {
      console.log('Testing module import...');
      
      // Check if module is imported
      const moduleExists = typeof LlamaCppRn !== 'undefined';
      console.log('Module exists:', moduleExists);
      
      // List available functions
      const functions = moduleExists ? Object.keys(LlamaCppRn) : [];
      console.log('Available functions:', functions);
      
      setFileTest({
        moduleExists,
        functions
      });
    } catch (error) {
      console.error('Module import test failed:', error);
      setFileError(`Error: ${error instanceof Error ? error.message : String(error)}`);
    } finally {
      setFileLoading(false);
    }
  };

  // Test just the bundle directory contents without trying to load any models
  const testBundleDirectoryOnly = async () => {
    setFileLoading(true);
    setFileError(null);
    setFileTest(null);
    
    try {
      console.log('Testing bundle directory contents only...');
      
      const bundleDir = FileSystem.bundleDirectory || '';
      const documentsDir = FileSystem.documentDirectory || '';
      
      console.log('Bundle directory:', bundleDir);
      console.log('Documents directory:', documentsDir);
      
      // List files in bundle directory
      let bundleFiles: string[] = [];
      try {
        bundleFiles = await FileSystem.readDirectoryAsync(bundleDir);
        console.log('Bundle directory files:', bundleFiles);
      } catch (error) {
        console.error('Error listing bundle directory:', error);
        throw new Error(`Failed to read bundle directory: ${error instanceof Error ? error.message : String(error)}`);
      }
      
      // Get directory sizes and check for GGUF files
      const ggufFiles = bundleFiles.filter(file => file.endsWith('.gguf'));
      console.log('GGUF files in bundle:', ggufFiles);
      
      // If there are any GGUF files, get their info
      const ggufInfos = [];
      for (const file of ggufFiles) {
        try {
          const filePath = `${bundleDir}${file}`;
          const info = await FileSystem.getInfoAsync(filePath) as ExtendedFileInfo;
          ggufInfos.push({
            name: file,
            path: filePath,
            size: info.size || 0,
            exists: info.exists
          });
        } catch (error) {
          console.error(`Error getting info for file ${file}:`, error);
        }
      }
      
      setFileTest({
        bundleDir,
        documentsDir,
        bundleFiles,
        ggufFiles,
        ggufInfos,
        timestamp: new Date().toISOString()
      });
    } catch (error) {
      console.error('Bundle directory test failed:', error);
      setFileError(`Error: ${error instanceof Error ? error.message : String(error)}`);
    } finally {
      setFileLoading(false);
    }
  };

  // Test basic native function that doesn't depend on model loading
  const testBasicNativeFunction = async () => {
    setFileLoading(true);
    setFileError(null);
    setFileTest(null);
    
    try {
      console.log('Testing basic native functions...');
      
      // Check if the module exists
      if (typeof LlamaCppRn === 'undefined') {
        throw new Error('LlamaCppRn module is not available');
      }
      
      // Get all available functions
      const functions = Object.keys(LlamaCppRn);
      console.log('Available functions:', functions);
      
      // Try to call simpler functions
      const results: Record<string, any> = {};
      
      // Test fileExists if available
      if (typeof LlamaCppRn.fileExists === 'function') {
        const documentsDir = FileSystem.documentDirectory || '';
        const testPath = `${documentsDir}test_basic.txt`;
        
        // Create a test file
        await FileSystem.writeAsStringAsync(testPath, 'Test content');
        
        // Check if it exists
        const exists = await LlamaCppRn.fileExists(testPath);
        results.fileExists = { path: testPath, exists };
      }
      
      // Test llama.cpp version if available
      if (typeof LlamaCppRn.getLlamaCppVersion === 'function') {
        const version = await LlamaCppRn.getLlamaCppVersion();
        results.llamaCppVersion = version;
      }
      
      // Get library info if available
      if (typeof LlamaCppRn.getLibraryInfo === 'function') {
        const info = await LlamaCppRn.getLibraryInfo();
        results.libraryInfo = info;
      }
      
      setFileTest({
        timestamp: new Date().toISOString(),
        functions,
        results
      });
    } catch (error) {
      console.error('Basic native function test failed:', error);
      setFileError(`Error: ${error instanceof Error ? error.message : String(error)}`);
    } finally {
      setFileLoading(false);
    }
  };

  // Test model location and prepare for chat test
  const testPrepareModelForChat = async () => {
    setFileLoading(true);
    setFileError(null);
    setFileTest(null);
    
    try {
      console.log('Testing model preparation for chat...');
      
      const bundleDir = FileSystem.bundleDirectory || '';
      const documentsDir = FileSystem.documentDirectory || '';
      
      console.log('Bundle directory:', bundleDir);
      console.log('Documents directory:', documentsDir);
      
      // Look for the specific model we want to use for chat
      const llamaModelName = 'Llama-3.2-1B-Instruct-Q4_K_M.gguf';
      const documentsModelPath = `${documentsDir}${llamaModelName}`;
      
      // First check if the model exists in the documents directory
      const modelInfo = await FileSystem.getInfoAsync(documentsModelPath);
      console.log('Llama model in documents directory exists:', modelInfo.exists);
      
      // If it exists, try to load its info to verify it's valid
      if (modelInfo.exists) {
        try {
          console.log('Attempting to load model info for:', documentsModelPath);
          
          // Load model info with the same settings we'll use in chat
          const info = await LlamaCppRn.loadLlamaModelInfo(documentsModelPath);
          console.log('Successfully loaded model info:', info);
          
          setFileTest({
            modelLocation: documentsModelPath,
            modelExists: true,
            modelInfo: info,
            modelSize: modelInfo.size,
            timestamp: new Date().toISOString()
          });
          return;
        } catch (loadError) {
          console.error('Failed to load model info:', loadError);
          // If we can't load info, the model might be corrupted - proceed below
        }
      }
      
      // Check for a model in specific locations that need to be copied
      const sourceLocations = [
        `${bundleDir}${llamaModelName}`,
        `${bundleDir}assets/${llamaModelName}`,
        `${bundleDir}example/assets/${llamaModelName}`
      ];
      
      console.log('Checking possible source locations:', sourceLocations);
      
      let sourceModelPath = null;
      
      // Check each source location
      for (const path of sourceLocations) {
        try {
          const info = await FileSystem.getInfoAsync(path);
          if (info.exists) {
            console.log('Found source model at:', path);
            sourceModelPath = path;
            break;
          }
        } catch (pathError) {
          console.log(`Error checking path ${path}:`, pathError);
        }
      }
      
      // If we found a source model, copy it to the documents directory
      if (sourceModelPath) {
        try {
          console.log(`Copying model from ${sourceModelPath} to ${documentsModelPath}...`);
          await FileSystem.copyAsync({
            from: sourceModelPath,
            to: documentsModelPath
          });
          
          // Verify the copy
          const copiedInfo = await FileSystem.getInfoAsync(documentsModelPath);
          if (copiedInfo.exists) {
            console.log('Successfully copied model to documents directory');
            
            // Try to load model info
            try {
              const info = await LlamaCppRn.loadLlamaModelInfo(documentsModelPath);
              console.log('Successfully loaded model info after copy:', info);
              
              setFileTest({
                modelLocation: documentsModelPath,
                modelExists: true,
                modelInfo: info,
                modelSize: copiedInfo.size,
                sourceModelPath,
                copied: true,
                timestamp: new Date().toISOString()
              });
              return;
            } catch (loadError) {
              console.error('Failed to load model info after copy:', loadError);
              // Continue below
            }
          }
        } catch (copyError) {
          console.error('Error copying model:', copyError);
        }
      }
      
      // If we get here, we couldn't find or copy a valid model
      setFileTest({
        modelExists: false,
        documentsDir,
        bundleDir,
        checkedLocations: sourceLocations,
        timestamp: new Date().toISOString(),
        message: "No suitable model found. Please download a valid GGUF model and place it in the documents directory.",
      });
      
      // Set a specific helpful error message
      setFileError(`No valid GGUF model found for chat. You need to download the Llama-3.2-1B-Instruct-Q4_K_M.gguf model (or another compatible GGUF) and place it in ${documentsDir}. This is required for the chat test to work.`);
    } catch (error) {
      console.error('Model preparation test failed:', error);
      setFileError(`Error: ${error instanceof Error ? error.message : String(error)}`);
    } finally {
      setFileLoading(false);
    }
  };

  const testLlamaModel = async (modelPath: string) => {
    setApiStatus(null);
    setModelLoading(true);
    
    try {
      // Load model info first to check if it's available
      const modelInfo = await LlamaCppRn.loadLlamaModelInfo(modelPath);
      console.log('Successfully loaded model info:', modelInfo);
      
      // Try to load the model with explicit settings
      // First attempt: with recommended settings from model info
      try {
        console.log('Initializing model with optimal settings...');
        const context = await LlamaCppRn.initLlama({
          model: modelPath,
          n_ctx: 512, // Start with smaller context
          n_batch: 512,
          // Use GPU if supported based on model info
          n_gpu_layers: modelInfo.gpuSupported ? 32 : 0
        });
        
        console.log('Model initialized successfully:', context);
        setApiStatus('Model initialized successfully');
        // Use context for completions, embeddings, etc.
        return context;
      } catch (loadError: any) {
        console.error('Model initialization failed:', loadError);
        
        // Second attempt: try with CPU only
        try {
          console.log('Trying with CPU only mode...');
          const context = await LlamaCppRn.initLlama({
            model: modelPath,
            n_ctx: 512, // Small context size
            n_batch: 256, // Smaller batch size
            n_gpu_layers: 0, // Force CPU mode
            use_mlock: true, // Try to keep model in memory
          });
          
          console.log('Model initialized with CPU only:', context);
          setApiStatus('Model initialized with CPU only');
          return context;
        } catch (cpuError: any) {
          console.error('CPU-only initialization also failed:', cpuError);
          
          // Third attempt: minimal context size
          try {
            console.log('Trying with minimal context size...');
            const context = await LlamaCppRn.initLlama({
              model: modelPath,
              n_ctx: 128, // Minimal context
              n_batch: 64, // Minimal batch
              n_gpu_layers: 0,
              n_threads: 1, // Minimal threads
            });
            
            console.log('Model initialized with minimal settings:', context);
            setApiStatus('Model initialized with minimal settings');
            return context;
          } catch (minimalError: any) {
            console.error('All initialization attempts failed:', minimalError);
            setApiStatus(`Failed to initialize model after multiple attempts: ${minimalError.message}`);
            throw new Error(`Could not initialize model after all attempts: ${minimalError.message}`);
          }
        }
      }
    } catch (error: any) {
      console.error('Error loading model:', error);
      setApiStatus(`Error: ${error.message}`);
      throw error;
    } finally {
      setModelLoading(false);
    }
  };

  const testWithAdvancedFallbacks = async () => {
    setApiStatus(null);
    setModelLoading(true);
    
    try {
      if (!modelInfo || !modelInfo.path) {
        setApiStatus('No model loaded yet. Please load a model first.');
        return;
      }
      
      // Use the testLlamaModel function with fallbacks
      await testLlamaModel(modelInfo.path);
    } catch (error: any) {
      console.error('Advanced model loading failed:', error);
      setApiStatus(`Error: ${error.message}`);
    } finally {
      setModelLoading(false);
    }
  };

  const testMinimalSettings = async () => {
    setMinimalSettingsLoading(true);
    setApiStatus(null);
    
    try {
      console.log('[testMinimalSettings] Starting minimal settings test');
      const documentDirectory = FileSystem.documentDirectory;
      const testFilePath = `${documentDirectory}test.bin`;
      
      console.log(`[testMinimalSettings] Documents dir: ${documentDirectory}`);
      console.log(`[testMinimalSettings] Test file path: ${testFilePath}`);
      
      // Create a small test file (1KB of zeros)
      const testData = new Array(1024).fill('0').join('');
      try {
        await FileSystem.writeAsStringAsync(testFilePath, testData);
        console.log(`[testMinimalSettings] Created test file at ${testFilePath}`);
        
        // Check if file exists using native method
        if (LlamaCppRn.fileExists) {
          const exists = await LlamaCppRn.fileExists(testFilePath);
          console.log(`[testMinimalSettings] Native file check - exists: ${exists}`);
        }
        
        const fileInfo = await FileSystem.getInfoAsync(testFilePath);
        console.log(`[testMinimalSettings] File exists: ${fileInfo.exists}, URI: ${fileInfo.uri}`);
        
        // Log available functions in LlamaCppRn
        console.log('[testMinimalSettings] Available functions in LlamaCppRn:', 
          Object.keys(LlamaCppRn).join(', '));
        
        // Try to load with absolute minimal settings
        console.log('[testMinimalSettings] Attempting to load with minimal settings...');
        const options = {
          n_gpu_layers: 0,
          verbose: true,
          embedding: false,
          n_threads: 1,
          n_batch: 8
        };
        
        const modelInfo = await LlamaCppRn.loadLlamaModelInfo(testFilePath, options);
        console.log('[testMinimalSettings] SUCCESS! Model info loaded:', modelInfo);
        setApiStatus('Success with minimal settings! See console for details.');
      } catch (writeError) {
        console.error('[testMinimalSettings] Error writing test file:', 
          writeError instanceof Error ? writeError.message : writeError);
        if (writeError instanceof Error) {
          console.error('Error type:', writeError.constructor.name);
          console.error('Error stack:', writeError.stack);
          // Log additional properties
          Object.keys(writeError).forEach(key => {
            console.error(`${key}:`, (writeError as any)[key]);
          });
        }
        setApiStatus(`Error writing test file: ${writeError instanceof Error ? writeError.message : writeError}`);
      }
    } catch (error) {
      console.error('[testMinimalSettings] Outer error:', 
        error instanceof Error ? error.message : error);
      if (error instanceof Error) {
        console.error('Error type:', error.constructor.name);
        console.error('Error stack:', error.stack);
        // Log additional properties
        Object.keys(error).forEach(key => {
          console.error(`${key}:`, (error as any)[key]);
        });
      }
      setApiStatus(`Error during minimal settings test: ${error instanceof Error ? error.message : error}`);
    } finally {
      setMinimalSettingsLoading(false);
    }
  };

  return (
    <View style={styles.container}>
      <ScrollView style={styles.scrollView}>
        <View style={styles.section}>
          <Text style={styles.sectionTitle}>Module Linkage Tests</Text>
          <View>
            <Button title="Test LlamaCppRn Import" onPress={testModuleImport} />
            {fileLoading && <ActivityIndicator size="small" />}
            {fileError && <View style={styles.resultBox}><Text style={styles.error}>{fileError}</Text></View>}
            {fileTest && <Text style={styles.resultText}>{JSON.stringify(fileTest)}</Text>}
          </View>
          
          <View style={styles.buttonSpacing} />
          
          <View>
            <Button title="Test File Access" onPress={testFileAccess} />
            {fileLoading && <ActivityIndicator size="small" />}
            {fileError && <View style={styles.resultBox}><Text style={styles.error}>{fileError}</Text></View>}
            {fileTest && <Text style={styles.resultText}>{JSON.stringify(fileTest)}</Text>}
          </View>
          
          <View style={styles.buttonSpacing} />
          
          <View>
            <Button title="Test Direct Model Load" onPress={() => testModelLoading(false)} />
            {modelLoading && <ActivityIndicator size="small" />}
            {modelError && <View style={styles.resultBox}><Text style={styles.error}>{modelError}</Text></View>}
            {modelInfo && (
              <View style={styles.resultBox}>
                <Text style={styles.resultTitle}>Model Info:</Text>
                <Text style={styles.resultText}>Path: {modelInfo.path}</Text>
                <Text style={styles.resultText}>Raw Path Used: {modelInfo.rawPathUsed ? 'Yes' : 'No'}</Text>
                {modelInfo.gpuSupport && (
                  <Text style={styles.gpuInfo}>{modelInfo.gpuSupport}</Text>
                )}
                <Text style={styles.resultSubtitle}>Model Properties:</Text>
                <Text style={styles.resultText}>Type: {modelInfo.info.description || 'Unknown'}</Text>
                <Text style={styles.resultText}>Parameters: {modelInfo.info.n_params?.toLocaleString() || 'Unknown'}</Text>
                <Text style={styles.resultText}>Context: {modelInfo.info.n_context || 'Unknown'}</Text>
                <Text style={styles.resultText}>Vocab Size: {modelInfo.info.n_vocab || 'Unknown'}</Text>
                {modelInfo.timestamp && (
                  <Text style={styles.resultTimestamp}>Last tested: {new Date(modelInfo.timestamp).toLocaleTimeString()}</Text>
                )}
              </View>
            )}
          </View>
          
          <View style={styles.buttonSpacing} />
          
          <View>
            <Button title="Test Schema Conversion" onPress={testSchemaConversion} />
            {schemaLoading && <ActivityIndicator size="small" />}
            {schemaError && <View style={styles.resultBox}><Text style={styles.error}>{schemaError}</Text></View>}
            {schemaResult && <Text style={styles.resultText}>{schemaResult}</Text>}
          </View>
        </View>
        
        <View style={styles.section}>
          <Text style={styles.sectionTitle}>Advanced diagnostics</Text>
          <View>
            <Button title="Test Bundle Directory Only" onPress={testBundleDirectoryOnly} />
            {fileLoading && <ActivityIndicator size="small" />}
            {fileError && <View style={styles.resultBox}><Text style={styles.error}>{fileError}</Text></View>}
            {fileTest && <Text style={styles.resultText}>{JSON.stringify(fileTest)}</Text>}
          </View>
          
          <View style={styles.buttonSpacing} />
          
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Advanced Tests</Text>
            
            <View style={styles.buttonContainer}>
              <TouchableOpacity 
                style={styles.button} 
                onPress={testWithSimpleFile}
                disabled={simpleFileLoading}>
                <Text style={styles.buttonText}>Test with Simple File</Text>
                {simpleFileLoading ? <ActivityIndicator /> : null}
              </TouchableOpacity>
              <TouchableOpacity 
                style={styles.button} 
                onPress={testMinimalSettings}
                disabled={minimalSettingsLoading}>
                <Text style={styles.buttonText}>Test Minimal Settings</Text>
                {minimalSettingsLoading ? <ActivityIndicator /> : null}
              </TouchableOpacity>
            </View>
            
            {apiStatus && (
              <View style={styles.resultBox}>
                <Text style={styles.resultText}>{apiStatus}</Text>
              </View>
            )}
          </View>
          
          <View style={styles.buttonSpacing} />
          
          <View>
            <Button title="Test Basic Native Function" onPress={testBasicNativeFunction} />
            {fileLoading && <ActivityIndicator size="small" />}
            {fileError && <View style={styles.resultBox}><Text style={styles.error}>{fileError}</Text></View>}
            {fileTest && <Text style={styles.resultText}>{JSON.stringify(fileTest)}</Text>}
          </View>
          
          <View style={styles.buttonSpacing} />
          
          <View>
            <Button title="Prepare Model for Chat" onPress={testPrepareModelForChat} />
            {fileLoading && <ActivityIndicator size="small" />}
            {fileError && <View style={styles.resultBox}><Text style={styles.error}>{fileError}</Text></View>}
            {fileTest && <Text style={styles.resultText}>{JSON.stringify(fileTest)}</Text>}
          </View>
          
          <View style={styles.buttonSpacing} />
          
          <View>
            <Button title="Test With Advanced Fallbacks" onPress={testWithAdvancedFallbacks} />
            {modelLoading && <ActivityIndicator size="small" />}
            {modelError && <View style={styles.resultBox}><Text style={styles.error}>{modelError}</Text></View>}
            {apiStatus && <Text style={styles.resultText}>{apiStatus}</Text>}
          </View>
        </View>
      </ScrollView>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 16,
  },
  pre: {
    fontFamily: Platform.OS === 'ios' ? 'Menlo' : 'monospace',
  },
  logContainer: {
    backgroundColor: '#f0f0f0',
    padding: 10,
    borderRadius: 4,
    marginVertical: 8,
  },
  button: {
    padding: 12,
    backgroundColor: '#007bff',
    borderRadius: 6,
    marginHorizontal: 4,
    alignItems: 'center',
    flexDirection: 'row',
    justifyContent: 'center',
  },
  buttonText: {
    fontSize: 16,
    fontWeight: 'bold',
    color: '#fff',
    marginRight: 8,
  },
  buttonContainer: {
    flexDirection: 'row',
    marginVertical: 8,
    justifyContent: 'space-between',
    alignItems: 'center',
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
    marginVertical: 12,
    padding: 12,
    backgroundColor: '#f5f5f5',
    borderRadius: 8,
  },
  sectionTitle: {
    fontSize: 16,
    fontWeight: 'bold',
    marginBottom: 12,
    color: '#333',
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
    marginTop: 12,
    padding: 10,
    backgroundColor: '#e6f7ff',
    borderRadius: 6,
    borderWidth: 1,
    borderColor: '#91d5ff',
  },
  resultTitle: {
    fontWeight: 'bold',
    marginBottom: 8,
  },
  resultText: {
    marginBottom: 4,
  },
  gpuInfo: {
    color: 'green',
    marginTop: 4,
  },
  resultSubtitle: {
    fontWeight: 'bold',
    marginTop: 12,
    marginBottom: 8,
  },
  resultTimestamp: {
    color: 'gray',
    marginTop: 4,
  },
}); 