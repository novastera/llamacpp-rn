import * as React from 'react';
import { View, Text, StyleSheet, ScrollView, Platform } from 'react-native';
import TestModule from './test-module';
import AndroidTest from './android-test';
import ModelChatTest from './model-chat-test';
import ConsolidatedTest from './consolidated-test';

export default function App() {
  return (
    <ScrollView style={styles.container}>
      <Text style={styles.heading}>LlamaCppRn Example</Text>
      
      {/* Consolidated test that combines multiple tests */}
      <ConsolidatedTest />

      {/* Chat interface with model loading */}
      <ModelChatTest />
      
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
