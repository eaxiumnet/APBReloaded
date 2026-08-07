/**
 * APB Content Studio — static-mode Playwright gate.
 *
 * Proves the studio works with NO backend at all: a plain `python -m http.server`
 * serves the baked `studio-static/` bundle (produced by build_static.ps1) and
 * the app must render weapons/vehicles/clothing from the pre-converted files.
 *
 * Run:
 *   npm run build:static && npm run bake   # or: ..\build_static.ps1
 *   npm run e2e:static
 */
import { defineConfig, devices } from "@playwright/test";
import { fileURLToPath } from "node:url";
import path from "node:path";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const serverDir = path.resolve(__dirname, "..", "server");
const venvPython = path.join(serverDir, ".venv", "Scripts", "python.exe");
const staticDir = path.resolve(__dirname, "..", "studio-static");

const STATIC_URL = "http://127.0.0.1:4174/";

export default defineConfig({
  testDir: "./e2e",
  testMatch: /static\.spec\.ts/,
  fullyParallel: false,
  workers: 1,
  retries: 0,
  reporter: [["list"]],
  outputDir: "test-results-static",
  expect: { timeout: 30_000 },
  use: {
    baseURL: STATIC_URL,
    trace: "retain-on-failure",
    screenshot: "only-on-failure",
  },
  webServer: [
    {
      // A plain static file server: no FastAPI, no Vite, no node dev server.
      command: `"${venvPython}" -m http.server 4174 --bind 127.0.0.1 --directory "${staticDir}"`,
      url: STATIC_URL,
      reuseExistingServer: true,
      timeout: 30_000,
    },
  ],
  projects: [
    {
      name: "chromium",
      use: { ...devices["Desktop Chrome"] },
    },
  ],
});
