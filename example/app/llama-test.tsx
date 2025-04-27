import * as React from 'react';
import { View, Text, StyleSheet, Button, ActivityIndicator, ScrollView, Platform, Alert } from 'react-native';
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
      
      // Test native module path conversion if available
      let nativePath = null;
      if (typeof LlamaCppRn.getAbsolutePath === 'function') {
        try {
          nativePath = await LlamaCppRn.getAbsolutePath(testFilePath);
          console.log('Native path:', nativePath);
        } catch (error) {
          console.error('Error getting native path:', error);
        }
      }
      
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
        nativePath,
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
        setModelInfo({
          path: modelPath,
          info: info,
          rawPathUsed: false
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
        setModelInfo({
          path: rawPath,
          info: info,
          rawPathUsed: true
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
      const result = await LlamaCppRn.jsonSchemaToGbnf(schema);
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
      console.log('Documents directory:', documentsDir);
      
      // Create the test file path
      const testFilePath = `${documentsDir}test_file.gguf`;
      console.log('Test file path:', testFilePath);
      
      // Check if the native module is available
      console.log('Native module available:', !!LlamaCppRn);
      console.log('Available functions:', Object.keys(LlamaCppRn).join(', '));
      console.log('loadLlamaModelInfo type:', typeof LlamaCppRn.loadLlamaModelInfo);
      
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
        await FileSystem.writeAsStringAsync(testFilePath, headerStr, {
          encoding: FileSystem.EncodingType.UTF8
        });
        
        // Verify the file exists
        const fileInfo = await FileSystem.getInfoAsync(testFilePath);
        console.log('File created successfully:', fileInfo);
        
        // Verify with native module if possible
        if (LlamaCppRn.fileExists) {
          const exists = await LlamaCppRn.fileExists(testFilePath);
          console.log('Native module fileExists check:', exists);
        }
      } catch (writeError: any) {
        console.error('Error writing test file:',
          writeError.name,
          writeError.message,
          writeError.stack,
          writeError.code,
          writeError.userInfo
        );
        return;
      }
      
      try {
        // Try the first attempt with default options
        console.log('Attempting to load model info with path:', testFilePath);
        const modelInfo = await LlamaCppRn.loadLlamaModelInfo(testFilePath);
        console.log('Successfully loaded model info:', modelInfo);
        setApiStatus(`Successfully loaded model info: ${JSON.stringify(modelInfo)}`);
      } catch (loadError: any) {
        console.error('Error loading model info (default options):',
          typeof loadError,
          loadError?.constructor?.name,
          loadError?.name,
          loadError?.message,
          loadError?.stack,
          loadError?.code,
          loadError?.userInfo,
          Platform.OS === 'ios' ? loadError?.nativeStackIOS : loadError?.nativeStackAndroid
        );
        
        // Try again with explicit options (disable GPU)
        try {
          const options = {
            n_gpu_layers: 0,
            verbose: true
          };
          console.log('Attempting with explicit options:', options);
          const modelInfo = await LlamaCppRn.loadLlamaModelInfo(testFilePath, options);
          console.log('Successfully loaded model info with options:', modelInfo);
          setApiStatus(`Successfully loaded model info with options: ${JSON.stringify(modelInfo)}`);
        } catch (optionsError: any) {
          console.error('Error loading model info (with options):',
            typeof optionsError,
            optionsError?.constructor?.name,
            optionsError?.name,
            optionsError?.message,
            optionsError?.stack,
            optionsError?.code,
            optionsError?.userInfo,
            Platform.OS === 'ios' ? optionsError?.nativeStackIOS : optionsError?.nativeStackAndroid
          );
          setApiStatus(`Failed to load model info: ${loadError?.message || 'Unknown error'}`);
        }
      }
    } catch (error: any) {
      console.error('Unexpected error in testWithSimpleFile:',
        typeof error,
        error?.constructor?.name,
        error?.name,
        error?.message,
        error?.stack
      );
      setApiStatus(`Error: ${error?.message || 'Unknown error'}`);
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

  // Test GPU support on iOS and Android
  const testGpuCapabilities = async () => {
    setFileLoading(true);
    setFileError(null);
    setFileTest(null);
    
    try {
      console.log('Testing GPU capabilities...');
      
      // Check if getGPUInfo function exists
      if (typeof LlamaCppRn.getGPUInfo !== 'function') {
        throw new Error('getGPUInfo function is not available in the native module');
      }
      
      // Call getGPUInfo
      const gpuInfo = await LlamaCppRn.getGPUInfo();
      console.log('GPU capabilities:', gpuInfo);
      
      const platform = Platform.OS;
      console.log(`Platform: ${platform}`);
      
      setFileTest({
        platform,
        gpuInfo,
        timestamp: new Date().toISOString()
      });
    } catch (error) {
      console.error('GPU capabilities test failed:', error);
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

  // Test getAbsolutePath specifically
  const testGetAbsolutePath = async () => {
    setFileLoading(true);
    setFileError(null);
    setFileTest(null);
    
    try {
      console.log('Testing getAbsolutePath function...');
      
      // Check if the module exists
      if (typeof LlamaCppRn === 'undefined') {
        throw new Error('LlamaCppRn module is not available');
      }
      
      // Check if getAbsolutePath is available
      console.log('getAbsolutePath type:', typeof LlamaCppRn.getAbsolutePath);
      console.log('All available methods:', Object.keys(LlamaCppRn).filter(key => typeof LlamaCppRn[key] === 'function'));
      
      if (typeof LlamaCppRn.getAbsolutePath !== 'function') {
        throw new Error('getAbsolutePath function is not available');
      }
      
      // Try to call getAbsolutePath with a simple path
      const documentsDir = FileSystem.documentDirectory || '';
      const testPath = `${documentsDir}test_path_check.txt`;
      
      // Create a test file
      await FileSystem.writeAsStringAsync(testPath, 'Test content');
      
      // Call getAbsolutePath
      const pathInfo = await LlamaCppRn.getAbsolutePath(testPath);
      console.log('getAbsolutePath result:', pathInfo);
      
      setFileTest({
        functionExists: true,
        pathInfo,
        testPath
      });
    } catch (error) {
      console.error('getAbsolutePath test failed:', error);
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
            {modelInfo && <Text style={styles.resultText}>{JSON.stringify(modelInfo)}</Text>}
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
          
          <View>
            <Button title="Test GPU Capabilities" onPress={testGpuCapabilities} />
            {fileLoading && <ActivityIndicator size="small" />}
            {fileError && <View style={styles.resultBox}><Text style={styles.error}>{fileError}</Text></View>}
            {fileTest && <Text style={styles.resultText}>{JSON.stringify(fileTest)}</Text>}
          </View>
          
          <View style={styles.buttonSpacing} />
          
          <View>
            <Button title="Test With Simple File" onPress={testWithSimpleFile} />
            {simpleFileLoading && <ActivityIndicator size="small" />}
            {apiStatus && <Text style={styles.resultText}>{apiStatus}</Text>}
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
            <Button title="Test getAbsolutePath" onPress={testGetAbsolutePath} />
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