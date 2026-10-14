const { defineConfig } = require('@playwright/test');
module.exports = defineConfig({
  testDir: '.', timeout: 20000, workers: 1,
  use: { browserName: 'chromium', channel: 'chrome', headless: true,
    launchOptions: { args: ['--enable-unsafe-webgpu'] }, screenshot: 'off', video: 'off', trace: 'off' },
  webServer: { command: 'node server.js', url: 'http://127.0.0.1:8197', reuseExistingServer: false },
});
