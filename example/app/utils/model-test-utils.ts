import * as FileSystem from 'expo-file-system';
const LlamaCppRn = require('@novastera-oss/llamacpp-rn');

// Interface for extended file info
export interface ExtendedFileInfo {
  exists: boolean;
  uri: string;
  isDirectory: boolean;
  modificationTime: number;
  size?: number;
}

// Interface for model loading results
export interface ModelLoadResult {
  success: boolean;
  path?: string;
  info?: any;
  error?: string;
  rawPathUsed?: boolean;
  gpuSupport?: string;
  optimalGpuLayers?: number;
  quant_type?: string;
  architecture?: string;
  timestamp: string;
}

// Interface for file test results
export interface FileTestResult {
  bundleDir?: string;
  documentsDir?: string;
  testFilePath?: string;
  fileExists?: boolean;
  fileSize?: number;
  fileExistsResult?: boolean;
  bundleFiles?: string[];
  ggufFiles?: string[];
  ggufInfos?: any[];
  moduleExists?: boolean;
  functions?: string[];
  timestamp: string;
  message?: string;
  error?: string;
  results?: Record<string, any>;
  modelExists?: boolean;
  modelPath?: string;
  modelInfo?: any;
}

/**
 * Create a minimal GGUF file for testing
 * @param filePath Path where to create the file
 * @returns Info about the created file
 */
export async function createTestGgufFile(filePath: string): Promise<ExtendedFileInfo> {
  console.log('Creating test GGUF file at:', filePath);
  
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
  
  // Convert the binary data to a string that can be written
  const headerStr = Array.from(header).map(byte => String.fromCharCode(byte)).join('');
  
  // Write the file with UTF8 encoding
  await FileSystem.writeAsStringAsync(filePath, headerStr, {
    encoding: FileSystem.EncodingType.UTF8
  });
  
  // Verify the file exists and return file info
  const fileInfo = await FileSystem.getInfoAsync(filePath) as ExtendedFileInfo;
  console.log('Test GGUF file created:', fileInfo);
  
  return fileInfo;
}

/**
 * Test loading a model's metadata
 * @param modelPath Path to the model file
 * @param useFallbacks Whether to try alternative loading methods if the first fails
 * @returns Model loading results
 */
export async function testLoadModelInfo(modelPath: string, useFallbacks = true): Promise<ModelLoadResult> {
  console.log('Attempting to load model info from:', modelPath);
  
  try {
    // Try loading with original path
    const info = await LlamaCppRn.loadLlamaModelInfo(modelPath);
    console.log('Model loaded successfully:', info);
    
    // Check GPU support
    const gpuTestResult = info.gpuSupported ? 
      'GPU support available - you can enable it for inference' : 
      'GPU support not available on this device/build';
    
    // On simulator, add a note about GPU not working
    const isSimulator = modelPath.includes('Simulator');
    const gpuSimulatorNote = isSimulator && info.gpuSupported ? 
      ' (Note: GPU acceleration typically fails on simulators, but should work on real devices)' : '';
    
    return {
      success: true,
      path: modelPath,
      info,
      rawPathUsed: false,
      gpuSupport: gpuTestResult + gpuSimulatorNote,
      optimalGpuLayers: info.optimalGpuLayers,
      quant_type: info.quant_type,
      architecture: info.architecture,
      timestamp: new Date().toISOString()
    };
  } catch (error) {
    console.error('Failed to load with original path:', error);
    
    // Try with raw path if fallbacks are enabled
    if (useFallbacks) {
      // Try loading with raw path (no file:// prefix)
      const rawPath = modelPath.replace('file://', '');
      if (rawPath !== modelPath) { // Only try if it's different
        console.log('Attempting to load model with raw path:', rawPath);
        try {
          const info = await LlamaCppRn.loadLlamaModelInfo(rawPath);
          console.log('Model loaded successfully with raw path:', info);
          
          // Check GPU support
          const gpuTestResult = info.gpuSupported ? 
            'GPU support available - you can enable it for inference' : 
            'GPU support not available on this device/build';
            
          // On simulator, add a note about GPU not working
          const isSimulator = rawPath.includes('Simulator');
          const gpuSimulatorNote = isSimulator && info.gpuSupported ? 
            ' (Note: GPU acceleration typically fails on simulators, but should work on real devices)' : '';
          
          return {
            success: true,
            path: rawPath,
            info,
            rawPathUsed: true,
            gpuSupport: gpuTestResult + gpuSimulatorNote,
            optimalGpuLayers: info.optimalGpuLayers,
            quant_type: info.quant_type,
            architecture: info.architecture,
            timestamp: new Date().toISOString()
          };
        } catch (rawPathError) {
          console.error('Failed to load with raw path:', rawPathError);
          
          // Try with minimal settings
          try {
            console.log('Attempting to load with minimal settings');
            const options = {
              n_gpu_layers: 0,
              verbose: true,
              embedding: false,
              n_threads: 1,
              n_batch: 8
            };
            
            const info = await LlamaCppRn.loadLlamaModelInfo(rawPath, options);
            console.log('Model loaded successfully with minimal settings:', info);
            
            return {
              success: true,
              path: rawPath,
              info,
              rawPathUsed: true,
              optimalGpuLayers: info.optimalGpuLayers,
              quant_type: info.quant_type,
              architecture: info.architecture,
              timestamp: new Date().toISOString()
            };
          } catch (minimalError) {
            console.error('Failed to load with minimal settings:', minimalError);
            return {
              success: false,
              error: `Failed to load model: ${minimalError instanceof Error ? minimalError.message : String(minimalError)}`,
              timestamp: new Date().toISOString()
            };
          }
        }
      }
    }
    
    // Return error if all attempts failed
    return {
      success: false,
      error: `Failed to load model: ${error instanceof Error ? error.message : String(error)}`,
      timestamp: new Date().toISOString()
    };
  }
}

/**
 * Test module import and available functions
 * @returns Results of module import test
 */
export async function testModuleImport(): Promise<FileTestResult> {
  console.log('Testing module import...');
  
  // Check if module is imported
  const moduleExists = typeof LlamaCppRn !== 'undefined';
  console.log('Module exists:', moduleExists);
  
  // List available functions
  const functions = moduleExists ? Object.keys(LlamaCppRn) : [];
  console.log('Available functions:', functions);
  
  return {
    moduleExists,
    functions,
    timestamp: new Date().toISOString()
  };
}

/**
 * List all files in bundle directory and find GGUF files
 * @returns Results of bundle directory test
 */
export async function testBundleDirectory(): Promise<FileTestResult> {
  console.log('Testing bundle directory contents...');
  
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
    return {
      bundleDir,
      documentsDir,
      error: `Failed to read bundle directory: ${error instanceof Error ? error.message : String(error)}`,
      timestamp: new Date().toISOString()
    };
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
  
  return {
    bundleDir,
    documentsDir,
    bundleFiles,
    ggufFiles,
    ggufInfos,
    timestamp: new Date().toISOString()
  };
}

/**
 * Test initializing a model for inference
 * @param modelPath Path to the model file
 * @returns Initialized model context or null
 */
export async function testInitializeModel(modelPath: string) {
  console.log('Testing model initialization:', modelPath);
  
  try {
    // Load model info first to check if it's available
    const modelInfo = await LlamaCppRn.loadLlamaModelInfo(modelPath);
    console.log('Successfully loaded model info:', modelInfo);
    
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
      
      console.log('Model initialized successfully');
      return context;
    } catch (loadError) {
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
        
        console.log('Model initialized with CPU only');
        return context;
      } catch (cpuError) {
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
          
          console.log('Model initialized with minimal settings');
          return context;
        } catch (minimalError) {
          console.error('All initialization attempts failed:', minimalError);
          return null;
        }
      }
    }
  } catch (error) {
    console.error('Error loading model info:', error);
    return null;
  }
}

/**
 * Find a usable model in the app bundle or assets
 * @returns Path to a usable model or null if none found
 */
export async function findUsableModel(): Promise<string | null> {
  const bundleDir = FileSystem.bundleDirectory || '';
  const documentsDir = FileSystem.documentDirectory || '';
  
  // Common model names
  const modelNames = [
    'Llama-3.2-1B-Instruct-Q4_K_M.gguf',
    'Mistral-7B-Instruct-v0.3.Q4_K_M.gguf'
  ];
  
  // Check if any models exist in the documents directory
  for (const name of modelNames) {
    const documentsModelPath = `${documentsDir}${name}`;
    try {
      const modelInfo = await FileSystem.getInfoAsync(documentsModelPath);
      if (modelInfo.exists) {
        console.log('Found model in documents directory:', documentsModelPath);
        return documentsModelPath;
      }
    } catch (error) {
      console.log(`Error checking for model ${name} in documents:`, error);
    }
  }
  
  // Check if there are any GGUF files in bundle
  try {
    const bundleFiles = await FileSystem.readDirectoryAsync(bundleDir);
    
    // First look for known model names
    for (const name of modelNames) {
      if (bundleFiles.includes(name)) {
        const modelPath = `${bundleDir}${name}`;
        console.log('Found specific model in bundle root:', modelPath);
        return modelPath;
      }
    }
    
    // Then check for any GGUF file
    const ggufFiles = bundleFiles.filter(file => file.endsWith('.gguf'));
    if (ggufFiles.length > 0) {
      const modelPath = `${bundleDir}${ggufFiles[0]}`;
      console.log('Found GGUF model in bundle:', modelPath);
      return modelPath;
    }
  } catch (error) {
    console.log('Could not read bundle directory:', error);
  }
  
  // Check the assets directory
  try {
    const assetsDir = `${bundleDir}assets/`;
    const assetFiles = await FileSystem.readDirectoryAsync(assetsDir);
    const assetGgufFiles = assetFiles.filter(file => file.endsWith('.gguf'));
    if (assetGgufFiles.length > 0) {
      const modelPath = `${assetsDir}${assetGgufFiles[0]}`;
      console.log('Found model in assets directory:', modelPath);
      return modelPath;
    }
  } catch (error) {
    console.log('Could not read assets directory:', error);
  }
  
  // Create a test GGUF file if no real models are found
  try {
    const testModelPath = `${documentsDir}test_model.gguf`;
    await createTestGgufFile(testModelPath);
    console.log('Created test GGUF file:', testModelPath);
    return testModelPath;
  } catch (error) {
    console.error('Failed to create test model:', error);
  }
  
  return null;
} 