const fs = require('fs');
const path = require('path');

const modelsPath = process.argv[2] || path.join(__dirname, 'models');
const content = fs.readFileSync(modelsPath, 'utf8');
const jsonStr = content.split('\n')[1];
const data = JSON.parse(jsonStr);

console.log('Total models:', data.data.length);
console.log('\nAll models:');
data.data.forEach((model, index) => {
  console.log(`${index + 1}. ${model.id}`);
  console.log(`   owned_by: ${model.owned_by}`);
  console.log(`   supported_endpoint_types: ${model.supported_endpoint_types.join(', ')}`);
  console.log();
});
