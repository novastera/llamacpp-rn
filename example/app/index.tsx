import * as React from 'react';
import { View, Text, StyleSheet, ScrollView, Platform } from 'react-native';
import TestModule from './test-module';
import AndroidTest from './android-test';
import LlamaTest from './llama-test';
import LlamaSchemaTest from './llama-schema-test';
import LlamaSimpleTest from './llama-simple-test';

export default function App() {
  return (
    <ScrollView style={styles.container}>
      <Text style={styles.heading}>LlamaCppRn Example</Text>
      
      {/* Simple Module Test - Most Basic Test */}
      <LlamaSimpleTest />
      
      {/* Schema Test Component - No model needed */}
      <LlamaSchemaTest />
      
      {/* Full Llama Test Component */}
      <LlamaTest />
      
      {/* Module test that works on both platforms */}
      <TestModule />
      
      {/* Android-specific test */}
      {Platform.OS === 'android' && <AndroidTest />}
      
      <View style={styles.spacer} />
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 16,
    backgroundColor: '#fff',
  },
  heading: {
    fontSize: 24,
    fontWeight: 'bold',
    marginTop: 40,
    marginBottom: 20,
  },
  spacer: {
    height: 100,
  }
});
