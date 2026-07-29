/**
 * APB Content Studio — Playwright config (Track B Slice 1 QA gate).
 *
 * Drives the REAL stack end-to-end: Playwright auto-starts the FastAPI backend
 * (the venv python + uvicorn, which serves the weapon catalog and converts
 * ActorX .psk -> GLB on demand) AND the Vite dev server (React + three.js
 * viewer), then runs the e2e specs against http://localhost:5173.
 *
 * Both servers use `reuseExistingServer: true`, so if a `run.ps1` session (or
 * another agent) already has them up, Playwright reuses them and only tears down
 * servers it started itself.
 *
 * Run:
 *   npm run e2e:install   # one-time: download the chromium binary
 *   npm run e2e            # run the suite (auto-starts both servers)
 */
import { defineConfig, devices } from "@playwright/test";
import { fileURLToPath } from "node:url";
import path from "node:path";

// ESM-safe __dirname (package.json has "type": "module").
const __dirname = path.dirname(fileURLToPath(import.meta.url));
const serverDir = path.resolve(__dirname, "..", "server");
const venvPython = path.join(serverDir, ".venv", "Scripts", "python.exe");

const BACKEND_URL = "http://127.0.0.1:8777/api/health";
const FRONTEND_URL = "http://localhost:5173/";

export default defineConfig({
  testDir: "./e2e",
  // One worker, no parallelism: we run two shared dev servers per run.
  fullyParallel: false,
  workers: 1,
  retries: process.env.CI ? 2 : 0,
  forbidOnly: !!process.env.CI,
  reporter: [["list"], ["html", { open: "never", outputFolder: "playwright-report" }]],
  outputDir: "test-results",
  expect: { timeout: 30_000 },
  use: {
    baseURL: FRONTEND_URL,
    trace: "retain-on-failure",
    screenshot: "only-on-failure",
    video: "retain-on-failure",
  },
  webServer: [
    {
      command: `"${venvPython}" -m uvicorn main:app --port 8777`,
      cwd: serverDir,
      url: BACKEND_URL,
      reuseExistingServer: true,
      timeout: 120_000,
    },
    {
      command: "npm run dev",
      cwd: __dirname,
      url: FRONTEND_URL,
      reuseExistingServer: true,
      timeout: 120_000,
    },
  ],
  projects: [
    {
      name: "chromium",
      use: { ...devices["Desktop Chrome"] },
    },
  ],
});
