// Static-mode API shim.
//
// The studio normally runs against the FastAPI backend (:8777) which reads the
// extracted retail assets. `bake_static.py` pre-converts a catalog into plain
// JSON/GLB/PNG files; with VITE_STATIC=1 this module rewrites every /api URL to
// that baked layout so the same app runs from any static host (GitHub Pages,
// a file server, an intranet) with zero downloads or backend.

export function isStaticMode(): boolean {
  const value = (import.meta.env.VITE_STATIC as string | undefined) ?? "";
  return value === "1" || value === "true";
}

// Deterministic filename slug shared with the Python bake (bake_static.py).
// ASCII paths only -> identical output on both sides.
export function slug(value: string): string {
  return value.replace(/[^A-Za-z0-9._-]/g, "_");
}

const DATA = "data";

function param(path: string, key: string): string {
  const query = path.split("?")[1] ?? "";
  return (new URLSearchParams(query).get(key) ?? "").trim();
}

// Rewrite an API URL to its baked static twin. Non-API / already-static URLs
// pass through untouched.
export function staticUrl(input: string): string {
  if (!isStaticMode()) return input;
  if (input.startsWith("data/")) return input;
  if (!input.startsWith("/api/")) return input;
  const base = input.split("?")[0];
  switch (base) {
    case "/api/catalog/weapons": return `${DATA}/catalog/weapons.json`;
    case "/api/catalog/vehicles": return `${DATA}/catalog/vehicles.json`;
    case "/api/catalog/vehicle_parts": return `${DATA}/catalog/vehicle_parts.json`;
    case "/api/catalog/animations": return `${DATA}/catalog/animations.json`;
    case "/api/catalog/characters": return `${DATA}/catalog/characters.json`;
    case "/api/catalog/clothing": return `${DATA}/catalog/clothing.json`;
    case "/api/symbols/list": return `${DATA}/catalog/symbols.json`;
    case "/api/inventory/assets": return `${DATA}/catalog/inventory.json`;
    case "/api/mesh.glb": return `${DATA}/glb/weapons/${slug(param(input, "path"))}.glb`;
    case "/api/vehicle.glb": return `${DATA}/glb/vehicles/${slug(param(input, "path"))}.glb`;
    case "/api/vehicle_animation.glb": {
      return `${DATA}/glb/vehicles/${slug(param(input, "path"))}.wheelspin.glb`;
    }
    case "/api/vehicle_part.glb": return `${DATA}/glb/vehicle_parts/${slug(param(input, "path"))}.glb`;
    case "/api/vehicle.sockets": return `${DATA}/sockets/${slug(param(input, "path"))}.json`;
    case "/api/clothing/mesh.glb": return `${DATA}/glb/clothing/${slug(param(input, "item"))}.glb`;
    case "/api/animation.glb": {
      const mesh = slug(param(input, "mesh"));
      const animset = slug(param(input, "animset"));
      const clip = slug(param(input, "clip"));
      return `${DATA}/glb/animations/${mesh}__${animset}__${clip}.glb`;
    }
    case "/api/static_mesh.glb": return `${DATA}/glb/districts/${slug(param(input, "path"))}.glb`;
    case "/api/prop_animation.glb": return `${DATA}/glb/props/${slug(param(input, "path"))}.glb`;
    // The colmask file is keyed by the item *name*; the app requests it with
    // the catalog id ("Name/Name"), so take the first path segment.
    case "/api/colmask": {
      const item = param(input, "item").split("/")[0];
      return `${DATA}/colmask/${slug(item)}.json`;
    }
    case "/api/colmask/texture":
    case "/api/symbol/texture":
    case "/api/texture.png": {
      const path = param(input, "path");
      return path.startsWith("data/") ? path : `${DATA}/textures/misc/${slug(path)}.png`;
    }
    case "/api/media": return `${DATA}/media/unavailable.txt`;
    default: return input;
  }
}

// The baked inventory.json is one large file; page turns must not re-download
// it. Cache the first static fetch per URL as a Blob (immutable), and hand
// each caller its own Response wrapping that Blob (a Response body stream is
// single-use, so the cached object itself must never be returned twice).
const staticBlobCache = new Map<string, Promise<Blob | null>>();

// fetch() with the static rewrite applied. The clothing compose POST has no
// baked twin: in static mode it is served by the client-side Canvas compositor
// (composeClient.ts), which mirrors the Python compositor.
export function apiFetch(input: string, init?: RequestInit): Promise<Response> {
  const url = staticUrl(input);
  if (isStaticMode() && init?.method === "POST" && input.startsWith("/api/compose/clothing")) {
    return import("./composeClient").then((m) =>
      m.composeClothingStatic(JSON.parse(String(init.body)))
    );
  }
  if (isStaticMode() && (!init || init.method === undefined || init.method === "GET")) {
    let cached = staticBlobCache.get(url);
    if (!cached) {
      cached = fetch(url, init).then(async (response) => {
        if (!response.ok) return null;
        return response.blob();
      });
      staticBlobCache.set(url, cached);
    }
    return cached.then((blob) =>
      blob === null
        ? new Response(null, { status: 404, statusText: "Not Found" })
        : new Response(blob, { status: 200, headers: { "Content-Type": "application/octet-stream" } })
    );
  }
  return fetch(url, init);
}

export type InventoryFilter = {
  query: string;
  category: string;
  status: string;
  source_build: string;
  offset: number;
  limit: number;
};

export type InventoryPage = {
  total: number;
  count: number;
  has_more: boolean;
  categories: Record<string, number>;
  statuses: Record<string, number>;
  source_builds: string[];
  assets: unknown[];
};

// The baked inventory.json holds the full unfiltered list (the server-side
// filter/pagination endpoints do not exist statically). Apply the same
// contract client-side.
export function filterInventory(data: InventoryPage, filter: InventoryFilter): InventoryPage {
  const needle = filter.query.trim().toLowerCase();
  const assets = (data.assets ?? []).filter((raw) => {
    const asset = raw as Record<string, unknown>;
    if (filter.category && asset.category !== filter.category) return false;
    if (filter.status && asset.status !== filter.status) return false;
    if (filter.source_build && asset.source_build !== filter.source_build) return false;
    if (!needle) return true;
    const text = [
      asset.name,
      asset.category,
      asset.status,
      asset.source_build,
      asset.source_locator,
      asset.source_package,
      asset.destination,
      asset.asset_class,
    ].join(" ").toLowerCase();
    return text.includes(needle);
  });
  const count = assets.length;
  const page = assets.slice(filter.offset, filter.offset + filter.limit);
  return {
    ...data,
    count,
    has_more: filter.offset + filter.limit < count,
    assets: page,
  };
}
