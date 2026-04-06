const fs = require('fs');
const path = require('path');

const modelsPath = process.argv[2] || path.join(__dirname, 'models');
const content = fs.readFileSync(modelsPath, 'utf8');
const jsonStr = content.split('\n')[1];
const data = JSON.parse(jsonStr);

const models = data.data.map(model => {
  return {
    id: model.id,
    name: model.id,
    type: model.supported_endpoint_types.includes('image-generation') ? 'image' : 'text',
    capabilities: model.supported_endpoint_types,
    provider: 'opencode'
  };
});

const config = {
  "$schema": "https://opencode.ai/config.json",
  "plugin": [
    "opencode plugin oh-my-opencode@latest",
    "node_modules/@tarquinen/opencode-dcp"
  ],
  "disabled_providers": [],
  "provider": {
    "coolyeah": {
      "base_url": "https://open.coolyeah.net/v1",
      "api_key": "${OPENAI_API_KEY}",
      "models": models
    }
  }
};

console.log(JSON.stringify(config, null, 2));
