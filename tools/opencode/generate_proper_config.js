const fs = require('fs');
const path = require('path');

const modelsPath = process.argv[2] || path.join(__dirname, 'models');
const content = fs.readFileSync(modelsPath, 'utf8');
const jsonStr = content.split('\n')[1];
const data = JSON.parse(jsonStr);

const modelsObj = {};
data.data.forEach(model => {
  modelsObj[model.id] = {
    name: model.id,
    attachment: true,
    reasoning: model.supported_endpoint_types.includes('anthropic') || model.id.includes('thinking') || model.id.includes('R1'),
    temperature: true,
    tool_call: true
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
      "npm": "@ai-sdk/openai-compatible",
      "options": {
        "baseURL": "https://open.coolyeah.net/v1",
        "apiKey": "${OPENAI_API_KEY}"
      },
      "models": modelsObj
    }
  }
};

console.log(JSON.stringify(config, null, 2));
