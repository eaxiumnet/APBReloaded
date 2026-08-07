/**
 * Static-mode gate: the studio must work from a plain static file server.
 *
 * The web app is built with VITE_STATIC=1 and served from `studio-static/`
 * (see playwright.static.config.ts). Every asset is a pre-baked file; no
 * request may hit a /api backend (the URL shim rewrites them to data/).
 */
import { test, expect, Page } from "@playwright/test";

function watchErrors(page: Page): string[] {
  const errors: string[] = [];
  page.on("pageerror", (error) => errors.push(`pageerror: ${error.message}`));
  page.on("console", (message) => {
    if (message.type() === "error") errors.push(`console: ${message.text()}`);
  });
  return errors;
}

test("weapons render from the baked bundle with zero backend requests", async ({ page }) => {
  const errors = watchErrors(page);
  const apiRequests: string[] = [];
  page.on("request", (request) => {
    if (request.url().includes("/api/")) apiRequests.push(request.url());
  });

  await page.goto("/");
  await page.getByRole("button", { name: "Weapons" }).click();

  await expect(page.locator(".stage")).toHaveAttribute("data-status", "ready", { timeout: 60_000 });
  const triangles = await page.locator(".stage").getAttribute("data-triangles");
  expect(Number(triangles)).toBeGreaterThan(0);
  // The catalog + GLB came from data/ files, never a backend route.
  expect(apiRequests).toEqual([]);
  expect(errors).toEqual([]);
});

test("vehicles assemble wheels from baked sockets + parts", async ({ page }) => {
  const errors = watchErrors(page);
  await page.goto("/");
  await page.getByRole("button", { name: "Vehicles" }).click();

  await expect(page.locator(".stage")).toHaveAttribute("data-status", "ready", { timeout: 60_000 });
  // Pick the wheel variant for the first vehicle, then the assembler mounts
  // four wheels from the baked sockets JSON.
  const wheelSelect = page.locator(".viewer-control:has-text(\"Wheel\") select").first();
  await wheelSelect.waitFor({ state: "visible", timeout: 30_000 });
  await wheelSelect.selectOption({ index: 1 });
  await expect(page.locator(".stage")).toHaveAttribute("data-assembled-wheels", "4", { timeout: 60_000 });
  // The four wheel clones joined the scene: triangle count grows beyond the
  // bare chassis.
  const triangles = await page.locator(".stage").getAttribute("data-triangles");
  expect(Number(triangles)).toBeGreaterThan(2000);
  expect(errors).toEqual([]);
});

test("clothing item loads and a region color composits client-side", async ({ page }) => {
  const errors = watchErrors(page);
  const composeRequests: string[] = [];
  page.on("request", (request) => {
    if (request.url().includes("/api/compose")) composeRequests.push(request.url());
  });

  await page.goto("/");
  await page.getByRole("button", { name: "Clothing" }).click();

  const firstItem = page.locator(".colmask-item").first();
  await firstItem.waitFor({ state: "visible", timeout: 30_000 });
  await firstItem.click();
  await expect(page.locator(".stage")).toHaveAttribute("data-status", "ready", { timeout: 60_000 });

  // Change a region color: the app debounces a compose call. In static mode
  // that call is served by the client-side Canvas compositor, so no network
  // request to any compose endpoint is ever made.
  const colorInput = page.locator(".region-color-picker input[type=color]").first();
  await colorInput.waitFor({ state: "visible", timeout: 30_000 });
  await colorInput.evaluate((input, value) => {
    const element = input as HTMLInputElement;
    element.value = value;
    element.dispatchEvent(new Event("input", { bubbles: true }));
    element.dispatchEvent(new Event("change", { bubbles: true }));
  }, "#22cc88");

  await page.waitForTimeout(2500);
  expect(composeRequests).toEqual([]);
  expect(errors).toEqual([]);
});
