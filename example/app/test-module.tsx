import * as React from 'react';
import { View, Text, StyleSheet, NativeModules, Platform } from 'react-native';
import { TurboModuleRegistry } from 'react-native';

// Import approach 1: Direct reference to source
let directImport: any = null;
let directImportError = '';
try {
  // eslint-disable-next-line @typescript-eslint/no-var-requires
  directImport = require('../../src/index');
  console.log('Direct import succeeded:', directImport);
} catch (error) {
  directImportError = error instanceof Error ? error.message : String(error);
  console.log('Direct import failed:', directImportError);
}

// Import approach 2: Package name (as users would use it)
let packageImport: any = null;
let packageImportError = '';
try {
  // eslint-disable-next-line @typescript-eslint/no-var-requires
  packageImport = require('@novastera-oss/llamacpp-rn');
  console.log('Package import succeeded:', packageImport);
} catch (error) {
  packageImportError = error instanceof Error ? error.message : String(error);
  console.log('Package import failed:', packageImportError);
}

// Function to check if TurboModule is properly registered and available
function checkModuleAvailability() {
  // Check for the module in both TurboModuleRegistry and NativeModules
  const modName = 'LlamaCppRn';
  const standardModule = NativeModules[modName];
  const turboModule = TurboModuleRegistry.get(modName);
  
  // List all available modules
  const allNativeModules = Object.keys(NativeModules);
  console.log('All Native Modules:', allNativeModules);
  
  // For demo, try calling a method if the module is available
  let methodCallResult = null;
  let methodCallError = '';
  
  if (standardModule || turboModule) {
    try {
      const module = turboModule || standardModule;
      if (module && typeof module.loadLlamaModelInfo === 'function') {
        // Don't actually call it, just check if it exists
        methodCallResult = 'Method exists but not called';
      } else {
        methodCallResult = 'Method does not exist';
      }
    } catch (error) {
      methodCallError = error instanceof Error ? error.message : String(error);
    }
  }
  
  return {
    available: {
      standardModule: !!standardModule,
      turboModule: !!turboModule,
    },
    moduleInfo: {
      standardModule: standardModule ? Object.keys(standardModule).join(', ') : 'N/A',
      turboModule: turboModule ? Object.keys(turboModule).join(', ') : 'N/A',
    },
    imports: {
      direct: directImport ? 'Success' : 'Failed',
      package: packageImport ? 'Success' : 'Failed',
      directError: directImportError,
      packageError: packageImportError,
    },
    methodCall: {
      result: methodCallResult,
      error: methodCallError,
    },
    allNativeModules,
  };
}

export default function TestModule() {
  const [result, setResult] = React.useState(() => checkModuleAvailability());
  
  // Check again after a delay to ensure modules have registered
  React.useEffect(() => {
    const timer = setTimeout(() => {
      setResult(checkModuleAvailability());
    }, 2000);
    
    return () => clearTimeout(timer);
  }, []);
  
  return (
    <View style={styles.container}>
      <Text style={styles.title}>LlamaCppRn Module Test</Text>
      
      <View style={styles.resultContainer}>
        <Text style={styles.sectionTitle}>Module Availability</Text>
        <Text style={styles.item}>
          Standard Module: {result.available.standardModule ? '✅' : '❌'}
        </Text>
        <Text style={styles.item}>
          Turbo Module: {result.available.turboModule ? '✅' : '❌'}
        </Text>
        
        <Text style={styles.sectionTitle}>Import Status</Text>
        <Text style={styles.item}>
          Direct Import: {result.imports.direct}
          {result.imports.directError && (
            <Text style={styles.error}>{'\n'}Error: {result.imports.directError}</Text>
          )}
        </Text>
        <Text style={styles.item}>
          Package Import: {result.imports.package}
          {result.imports.packageError && (
            <Text style={styles.error}>{'\n'}Error: {result.imports.packageError}</Text>
          )}
        </Text>
        
        <Text style={styles.sectionTitle}>Methods</Text>
        <Text style={styles.item}>
          Standard Module: {result.moduleInfo.standardModule}
        </Text>
        <Text style={styles.item}>
          Turbo Module: {result.moduleInfo.turboModule}
        </Text>
        
        {(result.methodCall.result || result.methodCall.error) && (
          <>
            <Text style={styles.sectionTitle}>Method Call</Text>
            {result.methodCall.result && (
              <Text style={styles.item}>Result: {result.methodCall.result}</Text>
            )}
            {result.methodCall.error && (
              <Text style={styles.error}>Error: {result.methodCall.error}</Text>
            )}
          </>
        )}
        
        <Text style={styles.sectionTitle}>All Native Modules</Text>
        <Text style={styles.smallItem}>
          {result.allNativeModules.join(', ')}
        </Text>
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
  smallItem: {
    fontSize: 12,
    color: '#777',
    lineHeight: 18,
  },
  error: {
    fontSize: 12,
    color: '#d00',
  },
}); 