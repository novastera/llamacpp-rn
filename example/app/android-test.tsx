import * as React from 'react';
import { View, Text, StyleSheet, NativeModules, Platform } from 'react-native';
import { TurboModuleRegistry } from 'react-native';

export default function AndroidTest() {
  const [moduleInfo, setModuleInfo] = React.useState<string>('Loading...');
  
  React.useEffect(() => {
    // Only run this on Android
    if (Platform.OS !== 'android') {
      setModuleInfo('This test is only for Android');
      return;
    }
    
    try {
      // Try different ways to access the module
      const turboModule = TurboModuleRegistry.get('LlamaCppRn');
      const stdModule = NativeModules.LlamaCppRn;
      
      let info = '';
      
      if (turboModule) {
        info += `TurboModule found: ${Object.keys(turboModule).join(', ')}\n`;
      } else {
        info += 'TurboModule not found\n';
      }
      
      if (stdModule) {
        info += `Standard module found: ${Object.keys(stdModule).join(', ')}\n`;
      } else {
        info += 'Standard module not found\n';
      }
      
      // List all available modules
      info += `\nAll modules: ${Object.keys(NativeModules).join(', ')}`;
      
      setModuleInfo(info);
    } catch (error) {
      setModuleInfo(`Error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }, []);
  
  return (
    <View style={styles.container}>
      <Text style={styles.title}>Android Module Test</Text>
      <View style={styles.resultContainer}>
        <Text style={styles.content}>{moduleInfo}</Text>
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
  content: {
    fontSize: 14,
    color: '#555',
  },
}); 