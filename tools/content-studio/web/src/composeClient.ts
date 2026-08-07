// Client-side clothing compositor for static mode.
//
// The FastAPI backend composites ColMask region tints + symbol decals with
// Pillow (compositor.py). A static deploy has no backend, so this module ports
// `composite_clothing` and `composite_body_overlay` to Canvas2D. The math is
// kept byte-for-byte equivalent (sRGB byte domain, PIL paste = mask-as-alpha
// blend) so a composed texture in static mode matches the live endpoint.

import { slug } from "./api";

const DATA = "data";

function loadImage(url: string): Promise<HTMLImageElement> {
  return new Promise((resolve, reject) => {
    const image = new Image();
    image.onload = () => resolve(image);
    image.onerror = () => reject(new Error(`compose: failed to load ${url}`));
    image.src = url;
  });
}

function canvasFromImage(image: HTMLImageElement): [HTMLCanvasElement, CanvasRenderingContext2D] {
  const canvas = document.createElement("canvas");
  canvas.width = image.naturalWidth;
  canvas.height = image.naturalHeight;
  const ctx = canvas.getContext("2d", { willReadFrequently: true })!;
  ctx.drawImage(image, 0, 0);
  return [canvas, ctx];
}

function luma(r: number, g: number, b: number): number {
  return Math.round(0.2126 * r + 0.7152 * g + 0.0722 * b);
}

// PIL: multiplied = ImageChops.multiply(base, solid); base.paste(multiplied,
// mask=mask_l) -> dst = base*(1-a) + base*solid/255*a, a = mask_luma/255.
function tintRegion(
  image: ImageData,
  mask: ImageData,
  r: number,
  g: number,
  b: number,
): void {
  const data = image.data;
  const maskData = mask.data;
  for (let i = 0; i < data.length; i += 4) {
    const a = maskData[i] / 255;
    if (a <= 0) continue;
    data[i] = data[i] * (1 - a) + ((data[i] * r) / 255) * a;
    data[i + 1] = data[i + 1] * (1 - a) + ((data[i + 1] * g) / 255) * a;
    data[i + 2] = data[i + 2] * (1 - a) + ((data[i + 2] * b) / 255) * a;
    data[i + 3] = 255;
  }
}

function drawDecal(
  ctx: CanvasRenderingContext2D,
  image: HTMLImageElement,
  u: number,
  v: number,
  scale: number,
  rotation: number,
): void {
  const dw = Math.max(1, Math.round(image.naturalWidth * scale));
  const dh = Math.max(1, Math.round(image.naturalHeight * scale));
  const cx = u * ctx.canvas.width;
  const cy = v * ctx.canvas.height;
  ctx.save();
  ctx.translate(cx, cy);
  // PIL rotates counterclockwise; canvas angles are clockwise (y down), so
  // negate to match.
  ctx.rotate((-rotation * Math.PI) / 180);
  ctx.drawImage(image, -dw / 2, -dh / 2, dw, dh);
  ctx.restore();
}

// composite_body_overlay: luma mask of the overlay (threshold/feather), then
// paste overlay over the base skin atlas with that mask.
function overlayBody(
  base: ImageData,
  overlay: HTMLImageElement,
  threshold: number,
  feather: number,
): ImageData {
  const canvas = document.createElement("canvas");
  canvas.width = base.width;
  canvas.height = base.height;
  const ctx = canvas.getContext("2d", { willReadFrequently: true })!;
  ctx.drawImage(overlay, 0, 0, base.width, base.height);
  const overlayData = ctx.getImageData(0, 0, base.width, base.height);
  const scale = 255 / feather;
  for (let i = 0; i < base.data.length; i += 4) {
    const mask = Math.max(
      0,
      Math.min(255, Math.round((luma(overlayData.data[i], overlayData.data[i + 1], overlayData.data[i + 2]) - threshold + feather / 2) * scale)),
    );
    const a = mask / 255;
    if (a <= 0) continue;
    base.data[i] = base.data[i] * (1 - a) + overlayData.data[i] * a;
    base.data[i + 1] = base.data[i + 1] * (1 - a) + overlayData.data[i + 1] * a;
    base.data[i + 2] = base.data[i + 2] * (1 - a) + overlayData.data[i + 2] * a;
    base.data[i + 3] = base.data[i + 3] * (1 - a) + overlayData.data[i + 3] * a;
  }
  return base;
}

type PlacedDecal = { path: string; u: number; v: number; scale: number; rotation: number };
type ComposeRequest = { item: string; colors: Record<string, string>; decals: PlacedDecal[] };

export async function composeClothingStatic(body: ComposeRequest): Promise<Response> {
  try {
    const itemSlug = slug(body.item);
    const colmask = await (await fetch(`${DATA}/colmask/${itemSlug}.json`)).json();
    const diffuseUrl = `${DATA}/textures/clothing/${itemSlug}/diffuse.png`;
    const diffuse = await loadImage(diffuseUrl);
    const [canvas, ctx] = canvasFromImage(diffuse);

    for (const [region, hexColor] of Object.entries(body.colors ?? {})) {
      const maskUrl: string | undefined = colmask.regions?.[region];
      if (!maskUrl || !maskUrl.startsWith("data/")) continue;
      const clean = hexColor.replace("#", "");
      if (clean.length !== 6) continue;
      const r = parseInt(clean.slice(0, 2), 16);
      const g = parseInt(clean.slice(2, 4), 16);
      const b = parseInt(clean.slice(4, 6), 16);
      const maskImage = await loadImage(maskUrl);
      const [, maskCtx] = canvasFromImage(maskImage);
      const maskData = maskCtx.getImageData(0, 0, canvas.width, canvas.height);
      const baseData = ctx.getImageData(0, 0, canvas.width, canvas.height);
      tintRegion(baseData, maskData, r, g, b);
      ctx.putImageData(baseData, 0, 0);
    }

    for (const decal of body.decals ?? []) {
      if (!decal.path.startsWith("data/")) continue;
      const image = await loadImage(decal.path);
      drawDecal(ctx, image, decal.u, decal.v, decal.scale ?? 1, decal.rotation ?? 0);
    }

    // Body items (F/M_Body_Base...) composite the layer over the skin atlas,
    // mirroring the live endpoint's is_body_item branch.
    if (colmask.body && colmask.skin && !body.item.toLowerCase().includes("armpad")) {
      const skin = await loadImage(colmask.skin as string);
      const baseData = ctx.getImageData(0, 0, canvas.width, canvas.height);
      ctx.putImageData(overlayBody(baseData, skin, 48, 24), 0, 0);
    }

    // PIL: result alpha = luma of RGB (the compositor's final putalpha).
    const finalData = ctx.getImageData(0, 0, canvas.width, canvas.height);
    for (let i = 0; i < finalData.data.length; i += 4) {
      finalData.data[i + 3] = luma(finalData.data[i], finalData.data[i + 1], finalData.data[i + 2]);
    }
    ctx.putImageData(finalData, 0, 0);

    const blob = await new Promise<Blob>((resolve, reject) =>
      canvas.toBlob((out) => (out ? resolve(out) : reject(new Error("compose: PNG encode failed"))), "image/png"),
    );
    return new Response(blob, { status: 200, headers: { "Content-Type": "image/png" } });
  } catch (error) {
    return new Response(String(error), { status: 500 });
  }
}
