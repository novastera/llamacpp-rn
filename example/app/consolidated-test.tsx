import * as React from 'react';
import { 
  View, 
  Text, 
  StyleSheet, 
  Button, 
  ActivityIndicator, 
  ScrollView, 
  Platform, 
  TouchableOpacity,
  TextInput
} from 'react-native';
import * as FileSystem from 'expo-file-system';
// Using require instead of import to avoid TypeScript errors
const LlamaCppRn = require('@novastera-oss/llamacpp-rn');
// Import utility functions
import { 
  testModuleImport, 
  testLoadModelInfo, 
  createTestGgufFile,
  testBundleDirectory,
  testInitializeModel,
  findUsableModel
} from './utils/model-test-utils';

/**
 * Consolidated test component that combines multiple test functions
 * into a single, organized interface.
 */
export default function ConsolidatedTest() {
  // Module registration state
  const [moduleInfo, setModuleInfo] = React.useState<any>(null);
  
  // Model state
  const [modelInfo, setModelInfo] = React.useState<any>(null);
  const [modelInstance, setModelInstance] = React.useState<any>(null);
  
  // Test results
  const [testResult, setTestResult] = React.useState<string | null>(null);
  const [gbnfResult, setGbnfResult] = React.useState<string | null>(null);
  const [completionResult, setCompletionResult] = React.useState<string | null>(null);
  
  // UI state
  const [loading, setLoading] = React.useState(false);
  const [error, setError] = React.useState<string | null>(null);
  const [prompt, setPrompt] = React.useState("Complete this sentence: The sky is");
  
  // Tab state
  const [activeTab, setActiveTab] = React.useState('files');

  // Check what functions are available on the module when component mounts
  React.useEffect(() => {
    checkModuleRegistration();
  }, []);

  // Check module registration
  const checkModuleRegistration = async () => {
    try {
      const result = await testModuleImport();
      setModuleInfo({
        functions: result.functions,
        hasInitLlama: result.functions?.includes('initLlama') || false,
        hasJsonSchemaToGbnf: result.functions?.includes('jsonSchemaToGbnf') || false,
        hasLoadLlamaModelInfo: result.functions?.includes('loadLlamaModelInfo') || false,
        hasCompletion: result.functions?.includes('completion') || false
      });
    } catch (err) {
      console.error('Error checking module:', err);
      setError(`Module check failed: ${err instanceof Error ? err.message : String(err)}`);
    }
  };

  // Test file system functionality
  const testFileSystem = async () => {
    setLoading(true);
    setError(null);
    setTestResult(null);
    
    try {
      // Check bundle directory
      const bundleResult = await testBundleDirectory();
      
      // Create test file
      const docDir = FileSystem.documentDirectory;
      if (!docDir) {
        throw new Error('Documents directory not available');
      }
      
      const testFilePath = `${docDir}test_file.txt`;
      await FileSystem.writeAsStringAsync(testFilePath, 'This is a test file');
      
      // Verify file exists
      const fileInfo = await FileSystem.getInfoAsync(testFilePath);
      
      setTestResult(`File system test passed. Bundle directory has ${bundleResult.bundleFiles?.length || 0} files.` + 
                    `${bundleResult.ggufFiles?.length ? ` Found ${bundleResult.ggufFiles.length} GGUF model(s).` : ''}`);
    } catch (error) {
      console.error('File system test failed:', error);
      setError(`Error: ${error instanceof Error ? error.message : String(error)}`);
    } finally {
      setLoading(false);
    }
  };

  // Create test model
  const createTestModel = async () => {
    setLoading(true);
    setError(null);
    setTestResult(null);
    
    try {
      const docDir = FileSystem.documentDirectory;
      if (!docDir) {
        throw new Error('Documents directory not available');
      }
      
      const testModelPath = `${docDir}test_model.gguf`;
      
      // Create a test GGUF file
      await createTestGgufFile(testModelPath);
      
      setTestResult(`Created test model at ${testModelPath}`);
    } catch (error) {
      console.error('Test model creation failed:', error);
      setError(`Error: ${error instanceof Error ? error.message : String(error)}`);
    } finally {
      setLoading(false);
    }
  };

  // Test JSON Schema to GBNF conversion
  const testSchemaConversion = async () => {
    setLoading(true);
    setError(null);
    setGbnfResult(null);
    
    try {
      console.log('Converting JSON Schema to GBNF...');
      
      // Simple JSON schema
      const schema = {
        type: 'object',
        properties: {
          name: { type: 'string' },
          age: { type: 'number' },
          email: { type: 'string' },
          address: {
            type: 'object',
            properties: {
              street: { type: 'string' },
              city: { type: 'string' },
              zip: { type: 'string' }
            },
            required: ['street', 'city']
          },
          tags: {
            type: 'array',
            items: { type: 'string' }
          }
        },
        required: ['name', 'email']
      };
      
      // Call the conversion function
      const result = await LlamaCppRn.jsonSchemaToGbnf({schema: JSON.stringify(schema)});
      console.log('Schema conversion result:', result);
      
      setGbnfResult(result);
      setTestResult('GBNF conversion successful!');
    } catch (error) {
      console.error('Schema conversion test failed:', error);
      setError(`Schema conversion error: ${error instanceof Error ? error.message : String(error)}`);
    } finally {
      setLoading(false);
    }
  };

  // Find and load a model
  const findAndLoadModel = async () => {
    setLoading(true);
    setError(null);
    setTestResult(null);
    setModelInfo(null);
    
    try {
      // Find a model to use
      const modelPath = await findUsableModel();
      
      if (!modelPath) {
        throw new Error('No GGUF models found. Please add a model to your app bundle or assets directory.');
      }
      
      // Try to load model info
      const result = await testLoadModelInfo(modelPath);
      setModelInfo(result);
      
      if (!result.success) {
        throw new Error(result.error);
      }
      
      setTestResult(`Successfully loaded model from ${modelPath}`);
    } catch (error) {
      console.error('Model loading test failed:', error);
      setError(`Error: ${error instanceof Error ? error.message : String(error)}`);
    } finally {
      setLoading(false);
    }
  };

  // Initialize model for inference
  const initializeModel = async () => {
    setLoading(true);
    setError(null);
    setTestResult(null);
    
    try {
      if (!modelInfo || !modelInfo.path) {
        throw new Error('No model loaded. Please load a model first.');
      }
      
      const context = await testInitializeModel(modelInfo.path);
      
      if (context) {
        setModelInstance(context);
        setTestResult('Model initialized successfully for inference!');
      } else {
        throw new Error('Failed to initialize model after multiple attempts');
      }
    } catch (error) {
      console.error('Model initialization failed:', error);
      setError(`Error: ${error instanceof Error ? error.message : String(error)}`);
    } finally {
      setLoading(false);
    }
  };

  // Test basic completion
  const testCompletion = async () => {
    setLoading(true);
    setError(null);
    setCompletionResult(null);
    
    try {
      if (!modelInstance) {
        throw new Error('Model not initialized. Please initialize the model first.');
      }
      
      console.log('Testing completion with prompt:', prompt);
      
      // Use a simple prompt with minimal parameters
      const response = await modelInstance.completion({
        prompt: prompt,
        temperature: 0.1,
        max_tokens: 32,
        n_gpu_layers: 0,
      });
      
      console.log('Completion result:', response);
      
      // Check for error in response
      if (response.finish_reason === 'error') {
        throw new Error(response.text || 'Unknown error during completion');
      }
      
      setCompletionResult(response.text);
      setTestResult('Completion successful!');
    } catch (error) {
      console.error('Completion error:', error);
      setError(`Completion error: ${error instanceof Error ? error.message : String(error)}`);
    } finally {
      setLoading(false);
    }
  };

  // Release model resources
  const unloadModel = async () => {
    setLoading(true);
    setError(null);
    
    try {
      if (!modelInstance) {
        throw new Error('No model to unload');
      }
      
      await modelInstance.release();
      setModelInstance(null);
      setTestResult('Model released successfully');
    } catch (error) {
      console.error('Model unload error:', error);
      setError(`Error: ${error instanceof Error ? error.message : String(error)}`);
    } finally {
      setLoading(false);
    }
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>LlamaCpp React Native Tests</Text>
      
      {/* Module Registration Section */}
      <View style={styles.moduleSection}>
        <Text style={styles.sectionTitle}>Module Status</Text>
        
        {moduleInfo ? (
          <View style={styles.moduleInfo}>
            <Text style={styles.infoItem}>Module registered: {moduleInfo.functions?.length > 0 ? '✅' : '❌'}</Text>
            <Text style={styles.infoItem}>Load model info: {moduleInfo.hasLoadLlamaModelInfo ? '✅' : '❌'}</Text>
            <Text style={styles.infoItem}>GBNF conversion: {moduleInfo.hasJsonSchemaToGbnf ? '✅' : '❌'}</Text>
            <Text style={styles.infoItem}>Inference: {moduleInfo.hasInitLlama ? '✅' : '❌'}</Text>
          </View>
        ) : (
          <Text>Checking module registration...</Text>
        )}
      </View>
      
      {/* Tabs Navigation */}
      <View style={styles.tabContainer}>
        <TouchableOpacity 
          style={[styles.tab, activeTab === 'files' && styles.activeTab]} 
          onPress={() => setActiveTab('files')}>
          <Text style={[styles.tabText, activeTab === 'files' && styles.activeTabText]}>Files & GBNF</Text>
        </TouchableOpacity>
        <TouchableOpacity 
          style={[styles.tab, activeTab === 'model' && styles.activeTab]} 
          onPress={() => setActiveTab('model')}>
          <Text style={[styles.tabText, activeTab === 'model' && styles.activeTabText]}>Model & Inference</Text>
        </TouchableOpacity>
      </View>
      
      <ScrollView style={styles.scrollView}>
        {/* File System & GBNF Tests */}
        {activeTab === 'files' && (
          <View style={styles.section}>
            <Text style={styles.sectionSubtitle}>File System & GBNF Tests</Text>
            
            <View style={styles.buttonGroup}>
              <Button
                title="Test File System"
                onPress={testFileSystem}
                disabled={loading}
              />
              <View style={styles.buttonSpacer} />
              <Button
                title="Create Test Model"
                onPress={createTestModel}
                disabled={loading}
              />
              <View style={styles.buttonSpacer} />
              <Button
                title="Test Schema to GBNF"
                onPress={testSchemaConversion}
                disabled={loading || !moduleInfo?.hasJsonSchemaToGbnf}
              />
            </View>
            
            {loading && <ActivityIndicator style={styles.loader} />}
            
            {error && (
              <View style={styles.errorContainer}>
                <Text style={styles.errorText}>{error}</Text>
              </View>
            )}
            
            {testResult && (
              <View style={styles.resultContainer}>
                <Text style={styles.resultText}>{testResult}</Text>
              </View>
            )}
            
            {gbnfResult && (
              <View style={styles.gbnfContainer}>
                <Text style={styles.resultTitle}>GBNF Grammar:</Text>
                <ScrollView style={styles.codeScroll}>
                  <Text style={styles.codeText}>{gbnfResult}</Text>
                </ScrollView>
              </View>
            )}
          </View>
        )}
        
        {/* Model & Inference Tests */}
        {activeTab === 'model' && (
          <View style={styles.section}>
            <Text style={styles.sectionSubtitle}>Model & Inference Tests</Text>
            
            <View style={styles.buttonGroup}>
              <Button
                title="Find & Load Model"
                onPress={findAndLoadModel}
                disabled={loading}
              />
              <View style={styles.buttonSpacer} />
              <Button
                title="Initialize Model"
                onPress={initializeModel}
                disabled={loading || !modelInfo?.success}
              />
              <View style={styles.buttonSpacer} />
              <Button
                title="Unload Model"
                onPress={unloadModel}
                disabled={loading || !modelInstance}
                color="#d32f2f"
              />
            </View>
            
            {loading && <ActivityIndicator style={styles.loader} />}
            
            {error && (
              <View style={styles.errorContainer}>
                <Text style={styles.errorText}>{error}</Text>
              </View>
            )}
            
            {testResult && (
              <View style={styles.resultContainer}>
                <Text style={styles.resultText}>{testResult}</Text>
              </View>
            )}
            
            {modelInfo?.success && (
              <View style={styles.modelInfoContainer}>
                <Text style={styles.resultTitle}>Model Information:</Text>
                <Text style={styles.infoItem}>Type: {modelInfo.info?.description || 'Unknown'}</Text>
                <Text style={styles.infoItem}>Parameters: {modelInfo.info?.n_params?.toLocaleString() || 'Unknown'}</Text>
                <Text style={styles.infoItem}>Context Size: {modelInfo.info?.n_context || 'Unknown'}</Text>
                <Text style={styles.infoItem}>Vocab Size: {modelInfo.info?.n_vocab || 'Unknown'}</Text>
                <Text style={styles.infoItem}>Optimal GPU Layers: {modelInfo.optimalGpuLayers || 'Unknown'}</Text>
                <Text style={styles.infoItem}>Quant Type: {modelInfo.quant_type || 'Unknown'}</Text>
                <Text style={styles.infoItem}>Architecture: {modelInfo.architecture || 'Unknown'}</Text>
                
                {modelInfo.gpuSupport && (
                  <Text style={styles.gpuInfo}>{modelInfo.gpuSupport}</Text>
                )}
              </View>
            )}
            
            {modelInstance && (
              <View style={styles.completionContainer}>
                <Text style={styles.resultTitle}>Test Completion:</Text>
                <TextInput
                  style={styles.promptInput}
                  value={prompt}
                  onChangeText={setPrompt}
                  placeholder="Enter prompt..."
                  multiline
                />
                <Button
                  title="Run Completion"
                  onPress={testCompletion}
                  disabled={loading || !modelInstance}
                />
                
                {completionResult && (
                  <View style={styles.completionResult}>
                    <Text style={styles.completionTitle}>Result:</Text>
                    <Text style={styles.completionText}>{completionResult}</Text>
                  </View>
                )}
              </View>
            )}
          </View>
        )}
      </ScrollView>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 16,
    backgroundColor: '#f8f9fa',
  },
  title: {
    fontSize: 22,
    fontWeight: 'bold',
    marginBottom: 16,
    textAlign: 'center',
  },
  moduleSection: {
    backgroundColor: '#ffffff',
    borderRadius: 8,
    padding: 12,
    marginBottom: 12,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.2,
    shadowRadius: 1.5,
    elevation: 2,
  },
  sectionTitle: {
    fontSize: 16,
    fontWeight: 'bold',
    marginBottom: 8,
    color: '#333',
  },
  sectionSubtitle: {
    fontSize: 16,
    fontWeight: 'bold',
    marginBottom: 12,
    color: '#333',
  },
  moduleInfo: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    justifyContent: 'space-between',
  },
  infoItem: {
    fontSize: 14,
    marginBottom: 4,
    width: '48%',
  },
  tabContainer: {
    flexDirection: 'row',
    marginBottom: 12,
  },
  tab: {
    flex: 1,
    padding: 12,
    backgroundColor: '#e0e0e0',
    alignItems: 'center',
    borderRadius: 4,
    marginHorizontal: 4,
  },
  activeTab: {
    backgroundColor: '#2196f3',
  },
  tabText: {
    fontWeight: 'bold',
    color: '#616161',
  },
  activeTabText: {
    color: '#ffffff',
  },
  scrollView: {
    flex: 1,
  },
  section: {
    backgroundColor: '#ffffff',
    borderRadius: 8,
    padding: 16,
    marginBottom: 16,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.2,
    shadowRadius: 1.5,
    elevation: 2,
  },
  buttonGroup: {
    marginBottom: 12,
  },
  buttonSpacer: {
    height: 8,
  },
  loader: {
    marginVertical: 12,
  },
  errorContainer: {
    backgroundColor: '#ffebee',
    borderRadius: 6,
    padding: 12,
    marginVertical: 8,
  },
  errorText: {
    color: '#d32f2f',
  },
  resultContainer: {
    backgroundColor: '#e0f2f1',
    borderRadius: 6,
    padding: 12,
    marginVertical: 8,
  },
  resultText: {
    color: '#00695c',
  },
  gbnfContainer: {
    marginTop: 12,
    padding: 12,
    backgroundColor: '#f0f7ff',
    borderRadius: 6,
    borderLeftWidth: 4,
    borderLeftColor: '#4a8eff',
    height: 200,
  },
  modelInfoContainer: {
    backgroundColor: '#e8f5e9',
    borderRadius: 6,
    padding: 12,
    marginVertical: 8,
  },
  resultTitle: {
    fontWeight: 'bold',
    marginBottom: 8,
  },
  codeScroll: {
    flex: 1,
  },
  codeText: {
    fontFamily: Platform.OS === 'ios' ? 'Menlo' : 'monospace',
    fontSize: 12,
  },
  gpuInfo: {
    color: '#1565c0',
    marginTop: 8,
  },
  completionContainer: {
    marginTop: 16,
    padding: 12,
    backgroundColor: '#f5f5f5',
    borderRadius: 6,
  },
  promptInput: {
    borderWidth: 1,
    borderColor: '#ddd',
    borderRadius: 4,
    padding: 8,
    marginBottom: 12,
    backgroundColor: '#fff',
    minHeight: 80,
  },
  completionResult: {
    marginTop: 12,
    padding: 12,
    backgroundColor: '#fff',
    borderRadius: 6,
    borderLeftWidth: 4,
    borderLeftColor: '#4caf50',
  },
  completionTitle: {
    fontWeight: 'bold',
    marginBottom: 8,
  },
  completionText: {
    fontSize: 14,
    lineHeight: 20,
  },
}); 