/**
 * Slice 1 — VIEWER happy path (Track B §5 S1).
 *
 * Proves the full pipeline the plan gates on:
 *   backend converts a base weapon's ActorX .psk -> GLB
 *   -> three.js renders it in an orbit camera
 *   -> Playwright screenshot shows the mesh
 *   -> browser console is clean
 *   -> scene-graph asserts: a mesh is present with >0 triangles.
 *
 * Target: Weapon_Armas_Magnum ("ACT 44"). It is the pytest-proven mesh
 * (server/tests/test_gltf.py already asserts its .psk parses and converts to a
 * well-formed GLB with correct accessor counts), so this spec exercises the
 * *networked* full stack on a known-good mesh rather than gambling on the
 * alphabetically-first catalog entry. If the Magnum is ever renamed/removed,
 * the test falls back to the first catalog weapon.
 */
import { test, expect } from "@playwright/test";

test.describe("Slice 1 — weapon viewer", () => {
  test("renders a base weapon mesh (.psk -> GLB -> three.js) with a clean console", async ({
    page,
  }) => {
    const errors: string[] = [];
    page.on("console", (msg) => {
      if (msg.type() === "error") errors.push(msg.text());
    });
    page.on("pageerror", (err) => errors.push(String(err)));

    await page.goto("/", { waitUntil: "domcontentloaded" });

    // 1. Catalog loaded -> the sidebar lists weapons (backend is alive + proxied).
    const weaponButtons = page.locator(".weapon");
    await expect(weaponButtons.first()).toBeVisible({ timeout: 30_000 });
    expect(await weaponButtons.count()).toBeGreaterThan(0);

    // 2. Select the proven-good Magnum ("ACT 44").
    const magnum = weaponButtons.filter({ hasText: "ACT 44" });
    if ((await magnum.count()) > 0) {
      await magnum.first().click();
    } else {
      await weaponButtons.first().click();
    }

    // 3. Mesh loaded -> the stage reports ready with >0 triangles.
    //    data-triangles / data-status are set by App.tsx from the three.js scene
    //    graph (countTriangles over all Mesh geometry), so this is the
    //    scene-graph assert: a real mesh with real geometry rendered.
    const stage = page.locator(".stage");
    await expect(stage).toHaveAttribute("data-status", "ready", { timeout: 30_000 });
    const triangles = Number((await stage.getAttribute("data-triangles")) ?? "0");
    expect(triangles).toBeGreaterThan(0);

    // 4. Capture the screenshot artifact (the "screenshot shows the mesh" proof).
    await stage.screenshot({ path: "test-results/slice1-viewer.png" });

    // 5. Console / page errors must be empty.
    expect(errors, errors.join("\n")).toEqual([]);
  });
});
