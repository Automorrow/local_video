const fs = require('fs');
const path = require('path');

const modelsPath = process.argv[2] || path.join(__dirname, 'models');
const content = fs.readFileSync(modelsPath, 'utf8');
const jsonStr = content.split('\n')[1];
const data = JSON.parse(jsonStr);

const modelNames = data.data.map(model => model.id);

const config = {
  "$schema": "https://opencode.ai/config.json",
  "plugin": [
    "opencode plugin oh-my-opencode@latest",
    "node_modules/@tarquinen/opencode-dcp"
  ],
  "disabled_providers": [],
  "providers": {
    "coolyeah": {
      "baseURL": "https://open.coolyeah.net/v1",
      "apiKey": "${OPENAI_API_KEY}",
      "models": modelNames
    }
  }
};

console.log(JSON.stringify(config, null, 2));
