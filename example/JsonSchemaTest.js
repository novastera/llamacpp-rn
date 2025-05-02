/**
 * Test utility for jsonSchemaToGbnf function
 * 
 * This shows the proper way to use the function and includes error handling
 */

import { jsonSchemaToGbnf } from 'llama.rn';

/**
 * Test converting a simple JSON Schema to GBNF grammar
 */
async function testJsonSchemaToGbnf() {
  console.log('Testing JSON Schema to GBNF conversion...');
  
  // Example schema object
  const schemaObj = {
    type: "object",
    properties: {
      name: { type: "string" },
      age: { type: "number" },
      isActive: { type: "boolean" }
    },
    required: ["name"]
  };
  
  try {
    // IMPORTANT: The schema must be passed as a stringified JSON in a schema property
    const grammar = await jsonSchemaToGbnf({
      schema: JSON.stringify(schemaObj)
    });
    
    console.log('Successfully converted schema to grammar:');
    console.log(grammar);
    return grammar;
  } catch (error) {
    console.error('Error converting schema to grammar:', error);
    throw error;
  }
}

/**
 * Test error handling for invalid input
 */
async function testInvalidInput() {
  console.log('Testing invalid inputs...');
  
  const testCases = [
    { 
      name: 'Missing schema property',
      input: {},
      expectedError: 'Missing required parameter: schema'
    },
    { 
      name: 'Non-string schema',
      input: { schema: { foo: 'bar' } },
      expectedError: 'Schema must be a string'
    },
    { 
      name: 'Empty schema',
      input: { schema: '' },
      expectedError: 'Schema cannot be empty'
    },
    { 
      name: 'Invalid JSON',
      input: { schema: '{not valid json' },
      expectedError: 'Invalid JSON in schema'
    }
  ];
  
  for (const testCase of testCases) {
    console.log(`Testing case: ${testCase.name}`);
    try {
      await jsonSchemaToGbnf(testCase.input);
      console.error('❌ Test failed: Should have thrown an error');
    } catch (error) {
      if (error.message.includes(testCase.expectedError)) {
        console.log(`✅ Test passed: Error properly thrown (${error.message})`);
      } else {
        console.error(`❌ Test failed: Wrong error: ${error.message}`);
      }
    }
  }
}

/**
 * A more complex schema for testing GBNF conversion
 */
async function testComplexSchema() {
  console.log('Testing complex schema...');
  
  const complexSchema = {
    type: "object",
    properties: {
      query: {
        type: "string",
        description: "Search query"
      },
      filters: {
        type: "object",
        properties: {
          category: {
            type: "string",
            enum: ["books", "electronics", "clothing"]
          },
          minPrice: { type: "number" },
          maxPrice: { type: "number" }
        }
      },
      pagination: {
        type: "object",
        properties: {
          page: { type: "integer" },
          limit: { type: "integer" }
        },
        required: ["page"]
      }
    },
    required: ["query"]
  };
  
  try {
    // IMPORTANT: The schema must be passed as a stringified JSON in a schema property
    const grammar = await jsonSchemaToGbnf({
      schema: JSON.stringify(complexSchema)
    });
    
    console.log('Successfully converted complex schema:');
    console.log(grammar);
    return grammar;
  } catch (error) {
    console.error('Error converting complex schema:', error);
    throw error;
  }
}

// Export test functions
export {
  testJsonSchemaToGbnf,
  testInvalidInput,
  testComplexSchema
};

// Main execution if run directly
if (typeof require !== 'undefined' && require.main === module) {
  (async () => {
    try {
      await testJsonSchemaToGbnf();
      await testInvalidInput();
      await testComplexSchema();
      console.log('All tests complete!');
    } catch (error) {
      console.error('Test suite failed:', error);
    }
  })();
} 