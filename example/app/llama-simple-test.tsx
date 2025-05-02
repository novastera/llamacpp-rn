import * as React from 'react';
import { View, Text, StyleSheet, Button, ActivityIndicator, Platform } from 'react-native';
import * as FileSystem from 'expo-file-system';
// Using require instead of import to avoid TypeScript errors
const LlamaCppRn = require('@novastera-oss/llamacpp-rn');

export default function LlamaSimpleTest() {
  const [moduleInfo, setModuleInfo] = React.useState<any>(null);
  const [loading, setLoading] = React.useState(false);
  const [error, setError] = React.useState<string | null>(null);
  const [fileTestResult, setFileTestResult] = React.useState<any>(null);
  
  // On mount, check what functions are available on the module
  React.useEffect(() => {
    const availableFunctions = Object.keys(LlamaCppRn).filter(key => typeof LlamaCppRn[key] === 'function');
    setModuleInfo({
      functions: availableFunctions,
      hasInitLlama: typeof LlamaCppRn.initLlama === 'function',
      hasJsonSchemaToGbnf: typeof LlamaCppRn.jsonSchemaToGbnf === 'function',
      hasLoadLlamaModelInfo: typeof LlamaCppRn.loadLlamaModelInfo === 'function',
    });
  }, []);
  
  // Test a simple function that doesn't need the model
  const handleTestJsonSchema = async () => {
    setLoading(true);
    setError(null);
    
    try {
      const simpleSchema = {
        type: 'object',
        properties: {
          name: { type: 'string' },
          age: { type: 'number' }
        },
        required: ['name']
      };
      
      const result = await LlamaCppRn.jsonSchemaToGbnf({schema: JSON.stringify(simpleSchema)});
      console.log('Schema conversion result:', result);
      
      // Add this result to moduleInfo
      setModuleInfo((prev: any) => ({
        ...prev,
        schemaTestPassed: true,
        schemaResult: result
      }));
    } catch (e) {
      console.error('Error testing JSON schema:', e);
      setError(`Error: ${e instanceof Error ? e.message : String(e)}`);
      
      setModuleInfo((prev: any) => ({
        ...prev,
        schemaTestPassed: false,
        error: e instanceof Error ? e.message : String(e)
      }));
    } finally {
      setLoading(false);
    }
  };

  // Test file copy and model info loading
  const handleTestFileAccess = async () => {
    setLoading(true);
    setError(null);
    setFileTestResult(null);
    
    try {
      console.log('Starting file access test...');
      const docDir = FileSystem.documentDirectory;
      console.log('Documents directory:', docDir);
      
      // Create a simple test file in the documents directory
      const testFilePath = `${docDir}test_file.gguf`;
      console.log('Creating test file at:', testFilePath);
      
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
      ]);
      
      // Convert Uint8Array to base64 string
      let binary = '';
      fileContent.forEach(byte => {
        binary += String.fromCharCode(byte);
      });
      const base64Data = btoa(binary);
      
      // Write file to documents directory
      await FileSystem.writeAsStringAsync(testFilePath, base64Data, {
        encoding: FileSystem.EncodingType.Base64
      });
      
      console.log('Test file created successfully');
      
      // Check if file exists
      const fileInfo = await FileSystem.getInfoAsync(testFilePath);
      console.log('File info:', fileInfo);
      
      // Try to load model info (expect failure but it should handle it gracefully)
      let modelInfoResult = null;
      try {
        console.log('Attempting to load model info from test file...');
        modelInfoResult = await LlamaCppRn.loadLlamaModelInfo(testFilePath);
        console.log('Model info result:', modelInfoResult);
      } catch (modelError) {
        console.log('Expected error loading model info:', modelError);
        modelInfoResult = { error: modelError instanceof Error ? modelError.message : String(modelError) };
      }
      
      // Set the combined result
      setFileTestResult({
        fileCreated: true,
        fileInfo,
        modelInfoResult
      });
      
    } catch (e) {
      console.error('Error in file access test:', e);
      setError(`Error: ${e instanceof Error ? e.message : String(e)}`);
      setFileTestResult({
        fileCreated: false,
        error: e instanceof Error ? e.message : String(e)
      });
    } finally {
      setLoading(false);
    }
  };
  
  return (
    <View style={styles.container}>
      <Text style={styles.title}>Llama Native Module Test</Text>
      
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Module Registration Check</Text>
        
        {moduleInfo ? (
          <View>
            <Text style={styles.info}>Module is registered: {moduleInfo.functions.length > 0 ? '✅' : '❌'}</Text>
            <Text style={styles.info}>initLlama available: {moduleInfo.hasInitLlama ? '✅' : '❌'}</Text>
            <Text style={styles.info}>jsonSchemaToGbnf available: {moduleInfo.hasJsonSchemaToGbnf ? '✅' : '❌'}</Text>
            <Text style={styles.info}>loadLlamaModelInfo available: {moduleInfo.hasLoadLlamaModelInfo ? '✅' : '❌'}</Text>
            
            {moduleInfo.schemaTestPassed && (
              <Text style={[styles.info, styles.success]}>Schema test passed! ✅</Text>
            )}
          </View>
        ) : (
          <Text style={styles.info}>Checking module...</Text>
        )}
        
        <Button
          title="Test JSON Schema Function"
          onPress={handleTestJsonSchema}
          disabled={loading || !moduleInfo?.hasJsonSchemaToGbnf}
        />
        
        <View style={styles.buttonSpacing} />
        
        <Button
          title="Test File Access and Loading"
          onPress={handleTestFileAccess}
          disabled={loading || !moduleInfo?.hasLoadLlamaModelInfo}
        />
        
        {loading && <ActivityIndicator style={styles.loader} />}
        
        {error && (
          <Text style={styles.error}>{error}</Text>
        )}
        
        {fileTestResult && (
          <View style={styles.resultBox}>
            <Text style={styles.resultTitle}>File Test Results:</Text>
            <Text style={styles.resultText}>File created: {fileTestResult.fileCreated ? '✅' : '❌'}</Text>
            {fileTestResult.fileInfo && (
              <Text style={styles.resultText}>File size: {fileTestResult.fileInfo.size} bytes</Text>
            )}
            {fileTestResult.modelInfoResult && fileTestResult.modelInfoResult.error ? (
              <Text style={styles.resultText}>Model loading (expected failure): ✅</Text>
            ) : (
              <Text style={styles.resultText}>Model info loaded: {fileTestResult.modelInfoResult ? '✅' : '❌'}</Text>
            )}
          </View>
        )}
      </View>
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
  info: {
    fontSize: 14,
    marginBottom: 6,
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
  success: {
    color: 'green',
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