import * as React from 'react';
import { View, Text, StyleSheet, Button, ActivityIndicator, ScrollView, Platform, Alert } from 'react-native';
import * as FileSystem from 'expo-file-system';
// Using require instead of import to avoid TypeScript errors
const LlamaCppRn = require('@novastera-oss/llamacpp-rn');

export default function LlamaSchemaTest() {
  const [gbnfResult, setGbnfResult] = React.useState<string | null>(null);
  const [loading, setLoading] = React.useState(false);
  const [error, setError] = React.useState<string | null>(null);
  
  // Check if the module is available
  React.useEffect(() => {
    console.log('LlamaCppRn module:', LlamaCppRn);
    console.log('jsonSchemaToGbnf available:', typeof LlamaCppRn.jsonSchemaToGbnf === 'function');
  }, []);

  const handleConvertSchema = async () => {
    setLoading(true);
    setError(null);
    setGbnfResult(null);
    
    try {
      console.log('Converting JSON Schema to GBNF...');
      
      const schema = {
        type: 'object',
        properties: {
          name: { type: 'string' },
          age: { type: 'number' },
          email: { type: 'string', format: 'email' },
          address: {
            type: 'object',
            properties: {
              street: { type: 'string' },
              city: { type: 'string' },
              zip: { type: 'string' }
            },
            required: ['street', 'city']
          },
          hobbies: {
            type: 'array',
            items: { type: 'string' }
          }
        },
        required: ['name', 'email']
      };
      
      const gbnf = await LlamaCppRn.jsonSchemaToGbnf(schema);
      console.log('GBNF Result:', gbnf);
      setGbnfResult(gbnf);
    } catch (error: unknown) {
      console.error('Failed to convert schema:', error);
      setError(error instanceof Error ? error.message : String(error));
    } finally {
      setLoading(false);
    }
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Schema Conversion Test</Text>
      
      <ScrollView style={styles.scrollView}>
        <View style={styles.section}>
          <Text style={styles.sectionTitle}>JSON Schema to GBNF Conversion</Text>
          <Text style={styles.description}>
            This test converts a JSON Schema to GBNF grammar format. 
            It doesn't require a model file, so it's a good way to verify the Turbo Module is working.
          </Text>
          
          <Button 
            title="Convert Schema to GBNF" 
            onPress={handleConvertSchema} 
            disabled={loading}
          />
          
          {loading && <ActivityIndicator style={styles.loader} />}
          
          {error && (
            <Text style={styles.error}>Error: {error}</Text>
          )}
          
          {gbnfResult && (
            <View style={styles.resultBox}>
              <Text style={styles.resultTitle}>GBNF Grammar:</Text>
              <ScrollView style={styles.codeScroll}>
                <Text style={styles.codeText}>{gbnfResult}</Text>
              </ScrollView>
            </View>
          )}
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
    marginBottom: 8,
  },
  description: {
    fontSize: 14,
    color: '#666',
    marginBottom: 16,
    lineHeight: 20,
  },
  loader: {
    marginTop: 12,
  },
  error: {
    color: 'red',
    marginTop: 12,
  },
  resultBox: {
    marginTop: 16,
    padding: 12,
    backgroundColor: '#f0f7ff',
    borderRadius: 6,
    borderLeftWidth: 4,
    borderLeftColor: '#4a8eff',
    height: 300,
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
  resultText: {
    marginTop: 12,
    marginBottom: 4,
  },
  success: {
    color: 'green',
  },
}); 