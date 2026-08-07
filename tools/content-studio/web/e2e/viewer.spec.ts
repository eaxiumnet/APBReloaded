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
    const failedRequests: string[] = [];
    const meshResponses: string[] = [];
    page.on("console", (msg) => {
      if (msg.type() === "error") errors.push(msg.text());
    });
    page.on("pageerror", (err) => errors.push(String(err)));
    page.on("requestfailed", (request) => {
      const failure = request.failure()?.errorText;
      if (failure !== "net::ERR_ABORTED") failedRequests.push(`${request.url()} :: ${failure}`);
    });
    page.on("response", (response) => {
      if (response.url().includes("/api/mesh.glb?")) {
        meshResponses.push(`${response.status()} ${response.url()}`);
      }
    });

    await page.goto("/", { waitUntil: "domcontentloaded" });

    // 1. Open the dedicated weapon lane; the app starts on the asset inventory.
    await page.getByRole("button", { name: "Weapons" }).click();

    // 2. Catalog loaded -> the sidebar lists weapons (backend is alive + proxied).
    const weaponButtons = page.locator(".weapon");
    await expect(weaponButtons.first()).toBeVisible({ timeout: 30_000 });
    expect(await weaponButtons.count()).toBeGreaterThan(0);

    // 3. Select the proven-good Magnum ("ACT 44").
    const magnum = weaponButtons.filter({ hasText: "ACT 44" });
    const selectedButton = (await magnum.count()) > 0 ? magnum.first() : weaponButtons.first();
    const selectedPrimary = await selectedButton.getAttribute("data-primary");
    expect(selectedPrimary).toBeTruthy();
    const meshResponse = page.waitForResponse((response) => {
      const url = new URL(response.url());
      return response.status() === 200
        && url.pathname === "/api/mesh.glb"
        && url.searchParams.get("path") === selectedPrimary;
    });
    await selectedButton.click();
    await meshResponse;

    // 4. Mesh loaded -> the stage reports ready with >0 triangles.
    //    data-triangles / data-status are set by App.tsx from the three.js scene
    //    graph (countTriangles over all Mesh geometry), so this is the
    //    scene-graph assert: a real mesh with real geometry rendered.
    const stage = page.locator(".stage");
    await expect(stage).toHaveAttribute("data-status", "ready", { timeout: 30_000 });
    const triangles = Number((await stage.getAttribute("data-triangles")) ?? "0");
    expect(triangles).toBeGreaterThan(0);
    expect(meshResponses.some((entry) => entry.startsWith("200 "))).toBeTruthy();
    expect(failedRequests, failedRequests.join("\n")).toEqual([]);

    // 5. Capture the screenshot artifact (the "screenshot shows the mesh" proof).
    await stage.screenshot({ path: "test-results/slice1-viewer.png" });

    // 6. Console / page errors must be empty.
    expect(errors, errors.join("\n")).toEqual([]);
  });

  test("assembles a replacement wheel on an extracted vehicle body", async ({ page }) => {
    test.setTimeout(180_000);
    const errors: string[] = [];
    const failedRequests: string[] = [];
    const assemblyResponses: string[] = [];
    page.on("console", (msg) => {
      if (msg.type() === "error") errors.push(msg.text());
    });
    page.on("pageerror", (error) => errors.push(String(error)));
    page.on("requestfailed", (request) => {
      const failure = request.failure()?.errorText;
      if (failure !== "net::ERR_ABORTED") failedRequests.push(`${request.url()} :: ${failure}`);
    });
    page.on("response", (response) => {
      if (response.url().includes("/api/vehicle_part.glb") || response.url().includes("/api/vehicle.sockets")) {
        assemblyResponses.push(`${response.url()} :: ${response.status()}`);
      }
    });
    await page.goto("/", { waitUntil: "domcontentloaded" });
    await page.getByRole("button", { name: "Vehicles" }).click();
    const vehicle = page.locator(".weapon").filter({ hasText: /^A 2DrCoupe/ });
    await expect(vehicle).toHaveCount(1, { timeout: 30_000 });
    await vehicle.click();
    const stage = page.locator(".stage");
    await expect(stage).toHaveAttribute("data-status", "ready", { timeout: 30_000 });
    const stockTriangles = Number((await stage.getAttribute("data-triangles")) ?? "0");
    expect(stockTriangles).toBeGreaterThan(0);

    const wheel = page.getByRole("combobox", { name: /^Wheel/ });
    await expect(wheel).toBeVisible({ timeout: 30_000 });
    const wheelOption = wheel.locator("option").nth(1);
    const wheelMesh = await wheelOption.getAttribute("value");
    expect(wheelMesh).toContain("Wheels");
    await wheel.selectOption(wheelMesh!);

    await expect(stage).toHaveAttribute("data-status", "ready", { timeout: 30_000 });
    const assembledTriangles = Number((await stage.getAttribute("data-triangles")) ?? "0");
    expect(assembledTriangles).toBeGreaterThan(0);
    await expect(stage).toHaveAttribute("data-assembled-wheels", "4");
    expect(assembledTriangles).not.toBe(stockTriangles);
    expect(assemblyResponses.some((entry) => entry.includes("/api/vehicle_part.glb") && entry.endsWith(":: 200"))).toBeTruthy();
    expect(assemblyResponses.some((entry) => entry.includes("/api/vehicle.sockets") && entry.endsWith(":: 200"))).toBeTruthy();
    expect(failedRequests, failedRequests.join("\n")).toEqual([]);
    expect(errors, errors.join("\n")).toEqual([]);
  });

  // Special chassis whose wheel_base resolves to a part family (TruckCurtain for
  // the cement/christmas/garbage trucks, E_Compact for the Crim Vaquero, C_Perf/
  // E_Perf for the Gumballs, Aletta, Enf V20, and both Jericho Phantoms). Each
  // must list its family in the Wheel dropdown and assemble 4 wheels once a
  // family variant is selected.
  const specialWheelVehicles = [
    { display: "TruckCement", needle: "TruckCement", family: "TruckCurtain" },
    { display: "TruckChristmas", needle: "TruckChristmas", family: "TruckCurtain" },
    { display: "TruckGarbage", needle: "TruckGarbage", family: "TruckCurtain" },
    { display: "Crim Vaquero", needle: "Crim Vaquero", family: "Compact" },
    { display: "Gumball", needle: "Gumball", family: "Perf" },
    { display: "Crim Performance Aletta", needle: "Aletta", family: "Perf" },
    { display: "Marketing 117 Enf V20", needle: "V20", family: "Perf" },
    { display: "Marketing DressToKill Jericho Phantom Crim", needle: "Phantom Crim", family: "Perf" },
  ];

  for (const vehicle of specialWheelVehicles) {
    test(`${vehicle.display} lists its ${vehicle.family} wheel family and assembles on ready`, async ({
      page,
    }) => {
      test.setTimeout(180_000);
      const errors: string[] = [];
      const failedRequests: string[] = [];
      page.on("console", (msg) => {
        if (msg.type() === "error") errors.push(msg.text());
      });
      page.on("pageerror", (err) => errors.push(String(err)));
      page.on("requestfailed", (request) => {
        const failure = request.failure()?.errorText;
        if (failure !== "net::ERR_ABORTED") failedRequests.push(`${request.url()} :: ${failure}`);
      });

      await page.goto("/", { waitUntil: "domcontentloaded" });
      await page.getByRole("button", { name: "Vehicles" }).click();

      // 1. The row resolves the chassis to its wheel family.
      const row = page.locator(".weapon").filter({ hasText: vehicle.needle }).first();
      await expect(row).toBeVisible({ timeout: 30_000 });
      await row.click();
      const stage = page.locator(".stage");
      await expect(stage).toHaveAttribute("data-status", "ready", { timeout: 30_000 });

      // 2. The Wheel dropdown lists Stock first, then the family variants.
      const wheel = page.getByRole("combobox", { name: /^Wheel/ });
      await expect(wheel).toBeVisible({ timeout: 30_000 });
      const options = wheel.locator("option");
      expect(await options.count()).toBeGreaterThanOrEqual(2);
      await expect(options.nth(0)).toHaveText("Stock");
      const familyLabels = await options.evaluateAll((els) =>
        els.slice(1).map((el) => el.textContent ?? ""));
      const familyNeedle = vehicle.family.toLowerCase();
      expect(
        familyLabels.some((label) => label.toLowerCase().includes(familyNeedle)),
        `${vehicle.display} options were: ${familyLabels.join(", ")}`,
      ).toBeTruthy();

      // 3. Select the first family variant: the vehicle assembles 4 wheels.
      await wheel.selectOption({ index: 1 });
      await expect(stage).toHaveAttribute("data-status", "ready", { timeout: 30_000 });
      await expect(stage).toHaveAttribute("data-assembled-wheels", "4");
      expect(Number((await stage.getAttribute("data-triangles")) ?? "0")).toBeGreaterThan(0);
      await stage.screenshot({
        path: `test-results/${vehicle.display.replace(/\W+/g, "-").toLowerCase()}-wheels.png`,
      });

      expect(failedRequests, failedRequests.join("\n")).toEqual([]);
      expect(errors, errors.join("\n")).toEqual([]);
    });
  }

  test("renders a clothing body item with the composited skin atlas", async ({ page }) => {
    test.setTimeout(180_000);
    let colmaskRequests = 0;
    page.on("request", (request) => {
      if (request.url().includes("/api/colmask?")) colmaskRequests += 1;
    });
    await page.goto("/", { waitUntil: "domcontentloaded" });
    await page.getByRole("button", { name: "Clothing" }).click();
    const clothingButtons = page.locator(".colmask-item");
    await expect(clothingButtons.first()).toBeVisible({ timeout: 30_000 });
    const armpads = clothingButtons.filter({ hasText: "F_Armpads_Armoured" });
    if ((await armpads.count()) > 0) {
      await armpads.first().click();
    } else {
      await clothingButtons.first().click();
    }
    const stage = page.locator(".stage");
    await expect(stage).toHaveAttribute("data-status", "ready", { timeout: 30_000 });
    expect(Number((await stage.getAttribute("data-triangles")) ?? "0")).toBeGreaterThan(0);
    await expect(page.locator(".region-grid")).toBeVisible({ timeout: 30_000 });
    await page.waitForTimeout(500);
    expect(colmaskRequests).toBeLessThanOrEqual(2);
    await stage.screenshot({ path: "test-results/slice1-clothing.png" });
  });

  test("renders a layer-only clothing item through the canonical body mesh", async ({ page }) => {
    const errors: string[] = [];
    page.on("pageerror", (error) => errors.push(String(error)));
    await page.goto("/", { waitUntil: "domcontentloaded" });
    await page.getByRole("button", { name: "Clothing" }).click();
    const tee = page.locator(".colmask-item").filter({ hasText: "F_Top_Shortsleeved_VNeck_Tee" });
    await expect(tee).toHaveCount(1, { timeout: 30_000 });
    await tee.click();
    const stage = page.locator(".stage");
    await expect(stage).toHaveAttribute("data-status", "ready", { timeout: 30_000 });
    await expect(page.locator(".app")).toBeVisible();
    expect(errors, errors.join("\n")).toEqual([]);
  });

  test("click-places a decal on the clothing model and lists it", async ({ page }) => {
    await page.goto("/", { waitUntil: "domcontentloaded" });
    await page.getByRole("button", { name: "Clothing" }).click();
    const stage = page.locator(".stage");
    await expect(stage).toHaveAttribute("data-status", "ready", { timeout: 30_000 });
    await expect(stage).toHaveAttribute("data-decals", "0");

    // Arm a symbol; the banner arms placement and the cursor switches to crosshair.
    await page.locator(".symbol-thumb").first().click();
    await expect(stage).toHaveAttribute("data-armed", "1");

    // Click the torso. The stage's exact center (50% height) is a gap in the
    // character's bounds, so aim at the proven torso line (~42% height).
    const box = await stage.boundingBox();
    expect(box).not.toBeNull();
    await page.mouse.click(box!.x + box!.width * 0.5, box!.y + box!.height * 0.42);

    await expect(stage).toHaveAttribute("data-decals", "1");
    await expect(page.locator(".decal-row")).toHaveCount(1);
  });

  test("auto-rotate pauses on interaction and resumes after the idle window", async ({ page }) => {
    await page.goto("/", { waitUntil: "domcontentloaded" });

    // Auto-rotate only drives the weapon lane (OrbitControls autoRotate prop is
    // gated on activeTab === "weapons"); data-auto-rotate mirrors the App state
    // that prop is derived from, so the weapons lane is the right host.
    await page.getByRole("button", { name: "Weapons" }).click();
    await page.locator(".weapon").first().click();
    const stage = page.locator(".stage");
    await expect(stage).toHaveAttribute("data-status", "ready", { timeout: 30_000 });

    // 1. Selecting a weapon is an interaction: rotation must be paused so the
    //    freshly framed front view is visible.
    await expect(stage).toHaveAttribute("data-auto-rotate", "0");

    // 2. Idle window (2500ms) elapses -> rotation resumes.
    await page.waitForTimeout(3_200);
    await expect(stage).toHaveAttribute("data-auto-rotate", "1");

    // 3. Drag the viewport -> OrbitControls onStart pauses rotation. Assert the
    //    paused state quickly (short timeout): the resume timer is 2500ms, so a
    //    slow assert could race the flip back to "1".
    const box = await stage.boundingBox();
    expect(box).not.toBeNull();
    await page.mouse.move(box!.x + box!.width / 2, box!.y + box!.height / 2);
    await page.mouse.down();
    await page.mouse.move(box!.x + box!.width / 2 + 80, box!.y + box!.height / 2 + 40, { steps: 10 });
    await page.mouse.up();
    await expect(stage).toHaveAttribute("data-auto-rotate", "0", { timeout: 2_000 });

    // 4. Idle window (2500ms) elapses again -> rotation resumes.
    await page.waitForTimeout(3_200);
    await expect(stage).toHaveAttribute("data-auto-rotate", "1");
  });

  test("plays a character animation clip with transport controls", async ({ page }) => {
    const errors: string[] = [];
    const failedRequests: string[] = [];
    page.on("console", (msg) => {
      if (msg.type() === "error") errors.push(msg.text());
    });
    page.on("pageerror", (err) => errors.push(String(err)));
    page.on("requestfailed", (request) => {
      const failure = request.failure()?.errorText;
      if (failure !== "net::ERR_ABORTED") failedRequests.push(`${request.url()} :: ${failure}`);
    });

    await page.goto("/", { waitUntil: "domcontentloaded" });

    // 1. Open the animation lane; the app starts on the asset inventory.
    await page.getByRole("button", { name: "Animations" }).click();

    // 2. Catalog loaded -> animset list appears (backend is alive + proxied).
    const animsetButtons = page.locator(".weapon[data-animset]");
    await expect(animsetButtons.first()).toBeVisible({ timeout: 30_000 });
    expect(await animsetButtons.count()).toBeGreaterThan(0);

    // 3. Prefer an animset whose first clip is long enough that the clock
    //    advance assertion (>=0.4s over 900ms) cannot wrap on a dummy clip.
    const catalog = await page.request.get("/api/catalog/animations").then((r) => r.json());
    const animsets: Array<{ relpath: string; clips: Array<{ name: string; duration: number }> }> =
      catalog.animsets ?? [];
    const good = animsets.find((a) => a.clips[0]?.duration >= 1.0)
      ?? animsets.find((a) => a.clips.length > 0);
    expect(good).toBeTruthy();
    const chosenButton = page.locator(`.weapon[data-animset="${good!.relpath}"]`);
    await chosenButton.click();

    // 4. The transport appears and the skinned GLB loads into the stage.
    const stage = page.locator(".stage");
    await expect(page.locator(".anim-transport")).toBeVisible({ timeout: 30_000 });
    await expect(stage).toHaveAttribute("data-status", "ready", { timeout: 120_000 });
    expect(Number((await stage.getAttribute("data-triangles")) ?? "0")).toBeGreaterThan(0);
    expect(Number((await stage.getAttribute("data-anim-duration")) ?? "0")).toBeGreaterThan(0);

    // 5. Playback runs: the clock advances while playing. The 2.633s clip can
    //    wrap at the end mid-window (looped), so probe and re-base instead of
    //    assuming one fixed window.
    await expect(stage).toHaveAttribute("data-anim-playing", "1");
    await expect(async () => {
      const probeStart = Number(await stage.getAttribute("data-anim-time"));
      await page.waitForTimeout(350);
      const probeEnd = Number(await stage.getAttribute("data-anim-time"));
      expect(probeEnd).toBeGreaterThan(probeStart + 0.15);
    }).toPass({ timeout: 20_000 });

    // 6. Pause freezes the clock.
    await page.locator(".anim-play").click();
    await expect(stage).toHaveAttribute("data-anim-playing", "0");
    const tPause = Number(await stage.getAttribute("data-anim-time"));
    await page.waitForTimeout(500);
    const tPaused = Number(await stage.getAttribute("data-anim-time"));
    expect(Math.abs(tPaused - tPause)).toBeLessThan(0.2);

    // 7. Scrub slider seeks to a new time while paused.
    const slider = page.getByLabel("Seek animation");
    const duration = Number(await stage.getAttribute("data-anim-duration"));
    const target = Math.max(0.5, Math.min(duration - 0.2, duration * 0.5));
    await slider.fill(String(target));
    await page.waitForTimeout(300);
    const tSeek = Number(await stage.getAttribute("data-anim-time"));
    expect(Math.abs(tSeek - target)).toBeLessThan(0.4);

    // 8. Play resumes from the seeked position.
    await page.locator(".anim-play").click();
    await expect(stage).toHaveAttribute("data-anim-playing", "1");
    await stage.screenshot({ path: "test-results/animation-playback.png" });

    // 9. Clean console / network.
    expect(failedRequests, failedRequests.join("\n")).toEqual([]);
    expect(errors, errors.join("\n")).toEqual([]);
  });

  test("previews a clothing character mesh with the animation transport", async ({ page }) => {
    const errors: string[] = [];
    const failedRequests: string[] = [];
    page.on("console", (msg) => {
      if (msg.type() === "error") errors.push(msg.text());
    });
    page.on("pageerror", (err) => errors.push(String(err)));
    page.on("requestfailed", (request) => {
      const failure = request.failure()?.errorText;
      if (failure !== "net::ERR_ABORTED") failedRequests.push(`${request.url()} :: ${failure}`);
    });

    await page.goto("/", { waitUntil: "domcontentloaded" });

    // 1. Open the Characters lane; the app starts on the asset inventory.
    await page.getByRole("button", { name: "Characters" }).click();

    // 2. Catalog loaded -> the character list appears.
    const characterButtons = page.locator(".weapon[data-character]");
    await expect(characterButtons.first()).toBeVisible({ timeout: 30_000 });
    expect(await characterButtons.count()).toBeGreaterThan(0);

    // 3. Pick a real clothing accessory (a backpack with its own Xtra mesh).
    const catalog = await page.request.get("/api/catalog/characters").then((r) => r.json());
    const items: Array<{ relpath: string; category: string; name: string }> =
      catalog.characters ?? [];
    const backpack = items.find((item) =>
      item.category === "clothing" && item.name.includes("Backpack"));
    expect(backpack).toBeTruthy();
    await page.locator(`.weapon[data-character="${backpack!.relpath}"]`).click();

    // 4. Transport appears and the skinned clothing GLB loads + animates.
    const stage = page.locator(".stage");
    await expect(page.locator(".anim-transport")).toBeVisible({ timeout: 30_000 });
    await expect(stage).toHaveAttribute("data-status", "ready", { timeout: 120_000 });
    expect(Number((await stage.getAttribute("data-triangles")) ?? "0")).toBeGreaterThan(0);
    expect(Number((await stage.getAttribute("data-anim-duration")) ?? "0")).toBeGreaterThan(0);

    // 5. The clock advances while playing. The clip can wrap at the end
    //    (looped), so probe and re-base instead of assuming one fixed window.
    await expect(stage).toHaveAttribute("data-anim-playing", "1");
    await expect(async () => {
      const probeStart = Number(await stage.getAttribute("data-anim-time"));
      await page.waitForTimeout(350);
      const probeEnd = Number(await stage.getAttribute("data-anim-time"));
      expect(probeEnd).toBeGreaterThan(probeStart + 0.15);
    }).toPass({ timeout: 20_000 });

    // 6. Pause freezes the clock.
    await page.locator(".anim-play").click();
    await expect(stage).toHaveAttribute("data-anim-playing", "0");
    await stage.screenshot({ path: "test-results/characters-clothing.png" });

    // 7. Clean console / network.
    expect(failedRequests, failedRequests.join("\n")).toEqual([]);
    expect(errors, errors.join("\n")).toEqual([]);
  });

  test("animates a vehicle with wheel spin through the animation pipeline", async ({ page }) => {
    const errors: string[] = [];
    const failedRequests: string[] = [];
    page.on("console", (msg) => {
      if (msg.type() === "error") errors.push(msg.text());
    });
    page.on("pageerror", (err) => errors.push(String(err)));
    page.on("requestfailed", (request) => {
      const failure = request.failure()?.errorText;
      if (failure !== "net::ERR_ABORTED") failedRequests.push(`${request.url()} :: ${failure}`);
    });

    await page.goto("/", { waitUntil: "domcontentloaded" });

    // 1. Open the Vehicles lane and pick the proven A 2DrCoupe.
    await page.getByRole("button", { name: "Vehicles" }).click();
    const vehicle = page.locator(".weapon").filter({ hasText: /^A 2DrCoupe/ });
    await expect(vehicle).toHaveCount(1, { timeout: 30_000 });
    await vehicle.click();
    const stage = page.locator(".stage");
    await expect(stage).toHaveAttribute("data-status", "ready", { timeout: 30_000 });
    await expect(stage).toHaveAttribute("data-vehicle-anim", "0");

    // 2. Toggle Animate: the wheel-spin GLB loads and plays.
    await page.locator("label.anim-check", { hasText: "Animate" }).locator("input").check();
    await expect(stage).toHaveAttribute("data-vehicle-anim", "1");
    await expect(stage).toHaveAttribute("data-status", "ready", { timeout: 120_000 });
    expect(Number((await stage.getAttribute("data-triangles")) ?? "0")).toBeGreaterThan(0);
    expect(Number((await stage.getAttribute("data-anim-duration")) ?? "0")).toBeGreaterThan(0);
    await expect(page.locator(".anim-transport")).toBeVisible();

    // 3. The wheel-spin clock advances while playing. The synthetic clip is 2s
    //    and loops, so it can wrap mid-window; probe and re-base instead of
    //    assuming one fixed window.
    await expect(stage).toHaveAttribute("data-anim-playing", "1");
    await expect(async () => {
      const probeStart = Number(await stage.getAttribute("data-anim-time"));
      await page.waitForTimeout(350);
      const probeEnd = Number(await stage.getAttribute("data-anim-time"));
      expect(probeEnd).toBeGreaterThan(probeStart + 0.15);
    }).toPass({ timeout: 20_000 });

    // 4. Pause freezes the clock.
    await page.locator(".anim-play").click();
    await expect(stage).toHaveAttribute("data-anim-playing", "0");
    const tPause = Number(await stage.getAttribute("data-anim-time"));
    await page.waitForTimeout(400);
    const tPaused = Number(await stage.getAttribute("data-anim-time"));
    expect(Math.abs(tPaused - tPause)).toBeLessThan(0.2);
    await stage.screenshot({ path: "test-results/vehicle-wheelspin.png" });

    expect(failedRequests, failedRequests.join("\n")).toEqual([]);
    expect(errors, errors.join("\n")).toEqual([]);
  });

  test("attaches a weapon to the character hand and it rides the animation", async ({ page }) => {
    const errors: string[] = [];
    const failedRequests: string[] = [];
    page.on("console", (msg) => {
      if (msg.type() === "error") errors.push(msg.text());
    });
    page.on("pageerror", (err) => errors.push(String(err)));
    page.on("requestfailed", (request) => {
      const failure = request.failure()?.errorText;
      if (failure !== "net::ERR_ABORTED") failedRequests.push(`${request.url()} :: ${failure}`);
    });

    await page.goto("/", { waitUntil: "domcontentloaded" });
    await page.getByRole("button", { name: "Animations" }).click();

    // 1. Load a long clip on the default body so the clock can advance.
    const animsetButtons = page.locator(".weapon[data-animset]");
    await expect(animsetButtons.first()).toBeVisible({ timeout: 30_000 });
    const catalog = await page.request.get("/api/catalog/animations").then((r) => r.json());
    const animsets: Array<{ relpath: string; clips: Array<{ name: string; duration: number }> }> =
      catalog.animsets ?? [];
    const good = animsets.find((a) => a.clips[0]?.duration >= 1.0)
      ?? animsets.find((a) => a.clips.length > 0);
    await page.locator(`.weapon[data-animset="${good!.relpath}"]`).click();
    const stage = page.locator(".stage");
    await expect(stage).toHaveAttribute("data-status", "ready", { timeout: 120_000 });
    const baseTriangles = Number((await stage.getAttribute("data-triangles")) ?? "0");
    expect(baseTriangles).toBeGreaterThan(0);
    await expect(stage).toHaveAttribute("data-weapon", "");

    // 2. Pick a weapon from the Animations lane weapon select.
    const weapons = await page.request.get("/api/catalog/weapons").then((r) => r.json());
    const weapon = (weapons.weapons ?? [])[0];
    expect(weapon).toBeTruthy();
    const weaponSelect = page.getByLabel("Weapon");
    await expect(weaponSelect).toBeVisible();
    await weaponSelect.selectOption(weapon.id);
    await expect(stage).toHaveAttribute("data-weapon", weapon.display);

    // 3. The weapon clone adds geometry: triangle count rises.
    await expect(async () => {
      const tris = Number(await stage.getAttribute("data-triangles"));
      expect(tris).toBeGreaterThan(baseTriangles);
    }).toPass({ timeout: 15_000 });

    // 4. The clip still plays with the weapon attached. The 2.633s first clip
    //    can wrap between samples, and the weapon GLB load can remount the
    //    stage (Suspense) and restart the mixer — either jumps the clock
    //    backwards. Probe for monotonic advance and re-base after a jump.
    await expect(stage).toHaveAttribute("data-anim-playing", "1");
    await expect(async () => {
      const probeStart = Number(await stage.getAttribute("data-anim-time"));
      await page.waitForTimeout(350);
      const probeEnd = Number(await stage.getAttribute("data-anim-time"));
      expect(probeEnd).toBeGreaterThan(probeStart + 0.15);
    }).toPass({ timeout: 20_000 });
    await stage.screenshot({ path: "test-results/character-weapon-attached.png" });

    expect(failedRequests, failedRequests.join("\n")).toEqual([]);
    expect(errors, errors.join("\n")).toEqual([]);
  });

  test("camera reframes to a clean front view when switching weapons", async ({ page }) => {
    await page.goto("/", { waitUntil: "domcontentloaded" });
    await page.getByRole("button", { name: "Weapons" }).click();
    const weapons = page.locator(".weapon");
    await expect(weapons.first()).toBeVisible({ timeout: 30_000 });
    const stage = page.locator(".stage");

    // Front-on framing: camera on the +Z axis looking at the origin, identity
    // orientation. Tolerate the small numeric drift the probe formats.
    const isFramed = (raw: string | null) => {
      if (!raw) return false;
      const m = raw.match(/p\((-?[\d.]+),(-?[\d.]+),(-?[\d.]+)\) q\((-?[\d.]+),(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)\)/);
      if (!m) return false;
      const [, px, py, pz, qx, qy, qz, qw] = m.map(Number);
      return Math.abs(px) < 2 && Math.abs(py) < 2 && pz > 10
        && Math.abs(qx) < 0.01 && Math.abs(qy) < 0.01 && Math.abs(qz) < 0.01 && Math.abs(qw - 1) < 0.01;
    };

    // Load the first weapon -> framed straight-on.
    await weapons.nth(0).click();
    await expect(stage).toHaveAttribute("data-status", "ready", { timeout: 30_000 });
    await expect(async () => {
      expect(isFramed(await stage.getAttribute("data-camera"))).toBeTruthy();
    }).toPass({ timeout: 10_000 });

    // Orbit far away from the front framing.
    const box = await stage.boundingBox();
    expect(box).not.toBeNull();
    await page.mouse.move(box!.x + box!.width / 2, box!.y + box!.height / 2);
    await page.mouse.down();
    await page.mouse.move(box!.x + box!.width / 2 + 180, box!.y + box!.height / 2 - 100, { steps: 12 });
    await page.mouse.up();
    await page.waitForTimeout(400);
    expect(isFramed(await stage.getAttribute("data-camera"))).toBeFalsy();

    // Switching to the second weapon must reset the framing to front-on.
    await weapons.nth(1).click();
    await expect(stage).toHaveAttribute("data-status", "ready", { timeout: 30_000 });
    await expect(async () => {
      expect(isFramed(await stage.getAttribute("data-camera"))).toBeTruthy();
    }).toPass({ timeout: 10_000 });
  });

  test("previews the BreakableDoors prop animation in the asset inventory", async ({ page }) => {
    test.setTimeout(240_000);
    const errors: string[] = [];
    const failedRequests: string[] = [];
    page.on("console", (msg) => {
      if (msg.type() === "error") errors.push(msg.text());
    });
    page.on("pageerror", (err) => errors.push(String(err)));
    page.on("requestfailed", (request) => {
      const failure = request.failure()?.errorText;
      if (failure !== "net::ERR_ABORTED") failedRequests.push(`${request.url()} :: ${failure}`);
    });

    await page.goto("/", { waitUntil: "domcontentloaded" });

    // 1. Pre-warm the inventory index. It builds lazily over the district and
    //    material trees on first call (measured ~280s cold); the build cache is
    //    keyed on the repo content, so any request warms it for the UI fetch.
    //    The default 30s request timeout would throw mid-build, so extend it.
    const warm = await page.request.get(
      "/api/inventory/assets?query=BreakableDoors&limit=5",
      { timeout: 300_000 },
    );
    expect(warm.ok()).toBeTruthy();

    // 2. Search the All assets lane (the app's default tab) for the door.
    await page.getByLabel("Search all assets").fill("BreakableDoors");
    const row = page.locator(".asset-row", { hasText: "ActiveBreakableDoor_Animset" }).first();
    await expect(row).toBeVisible({ timeout: 60_000 });
    await row.click();

    // 3. The prop_animation preview loads: stage ready, real geometry, and an
    //    animation clip with duration (the served GLB carries the rebased keys).
    const stage = page.locator(".stage");
    await expect(stage).toHaveAttribute("data-status", "ready", { timeout: 120_000 });
    expect(Number((await stage.getAttribute("data-triangles")) ?? "0")).toBeGreaterThan(0);
    expect(Number((await stage.getAttribute("data-anim-duration")) ?? "0")).toBeGreaterThan(0);

    // 4. The door clip plays on load (looped ~3.73s). Probe and re-base so a
    //    wrap at the loop point cannot fake a paused clock.
    await expect(async () => {
      const probeStart = Number(await stage.getAttribute("data-anim-time"));
      await page.waitForTimeout(350);
      const probeEnd = Number(await stage.getAttribute("data-anim-time"));
      expect(probeEnd).toBeGreaterThan(probeStart + 0.15);
    }).toPass({ timeout: 20_000 });

    // 5. Capture the artifact; require a clean console and network.
    await stage.screenshot({ path: "test-results/prop-breakable-doors.png" });
    expect(failedRequests, failedRequests.join("\n")).toEqual([]);
    expect(errors, errors.join("\n")).toEqual([]);
  });
});
