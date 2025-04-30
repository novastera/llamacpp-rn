import * as React from 'react';
import { View, Text, StyleSheet, Button, NativeModules } from 'react-native';
import { TurboModuleRegistry } from 'react-native';

// Using require instead of import to avoid TypeScript errors
const LlamaCppRn = require('@novastera-oss/llamacpp-rn');

export default function TestModule() {
  const [moduleInfo, setModuleInfo] = React.useState<any>(null);
  
  // Check module availability
  const checkModuleAvailability = () => {
    const modName = 'LlamaCppRn';
    const standardModule = NativeModules[modName];
    const turboModule = TurboModuleRegistry.get(modName);
    
    // Log more detailed information about the module
    console.log('Checking for LlamaCppRn module');
    console.log('All Native Modules:', Object.keys(NativeModules));
    console.log('Standard module available:', !!standardModule);
    console.log('Turbo module available:', !!turboModule);
    console.log('Direct require result type:', typeof LlamaCppRn);
    
    // Check if LlamaCppRn is loaded but with a different name
    for (const key of Object.keys(NativeModules)) {
      console.log(`Module "${key}" methods:`, Object.keys(NativeModules[key]));
    }
    
    // Check available methods
    const availableFunctions = Object.keys(LlamaCppRn || {}).filter(
      key => typeof LlamaCppRn[key] === 'function'
    );
    console.log('LlamaCppRn available functions:', availableFunctions);
    
    return {
      available: {
        standardModule: !!standardModule,
        turboModule: !!turboModule,
      },
      moduleInfo: {
        functions: availableFunctions,
        hasInitLlama: typeof LlamaCppRn?.initLlama === 'function',
        hasJsonSchemaToGbnf: typeof LlamaCppRn?.jsonSchemaToGbnf === 'function',
        hasLoadLlamaModelInfo: typeof LlamaCppRn?.loadLlamaModelInfo === 'function',
      },
    };
  };
  
  // Check module on mount
  React.useEffect(() => {
    setModuleInfo(checkModuleAvailability());
  }, []);
  
  // Manually refresh check
  const handleRefreshCheck = () => {
    setModuleInfo(checkModuleAvailability());
  };
  
  return (
    <View style={styles.container}>
      <Text style={styles.title}>LlamaCppRn Module Test</Text>
      
      <View style={styles.resultContainer}>
        <Text style={styles.sectionTitle}>Module Availability</Text>
        {moduleInfo && (
          <>
            <Text style={styles.item}>
              Standard Module: {moduleInfo.available.standardModule ? '✅' : '❌'}
            </Text>
            <Text style={styles.item}>
              Turbo Module: {moduleInfo.available.turboModule ? '✅' : '❌'}
            </Text>
            
            <Text style={styles.sectionTitle}>Available Functions</Text>
            {moduleInfo.moduleInfo.functions.map((func: string) => (
              <Text key={func} style={styles.item}>• {func}</Text>
            ))}
            
            <Text style={styles.sectionTitle}>Core Functions</Text>
            <Text style={styles.item}>
              initLlama: {moduleInfo.moduleInfo.hasInitLlama ? '✅' : '❌'}
            </Text>
            <Text style={styles.item}>
              jsonSchemaToGbnf: {moduleInfo.moduleInfo.hasJsonSchemaToGbnf ? '✅' : '❌'}
            </Text>
            <Text style={styles.item}>
              loadLlamaModelInfo: {moduleInfo.moduleInfo.hasLoadLlamaModelInfo ? '✅' : '❌'}
            </Text>
          </>
        )}
        
        <View style={styles.buttonSpacing} />
        <Button title="Refresh Check" onPress={handleRefreshCheck} />
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
  resultContainer: {
    backgroundColor: '#fff',
    padding: 16,
    borderRadius: 8,
  },
  sectionTitle: {
    fontSize: 16,
    fontWeight: 'bold',
    marginTop: 12,
    marginBottom: 8,
    color: '#333',
  },
  item: {
    fontSize: 14,
    marginBottom: 8,
    color: '#555',
  },
  buttonSpacing: {
    height: 16,
  },
}); 