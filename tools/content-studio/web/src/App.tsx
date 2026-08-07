import React, { Suspense, useCallback, useEffect, useLayoutEffect, useMemo, useRef, useState } from "react";
import { Canvas, useFrame, useLoader, useThree } from "@react-three/fiber";
import { Bounds, Center, OrbitControls, useGLTF, useTexture as dreiUseTexture } from "@react-three/drei";
import { GLTFLoader } from "three/examples/jsm/loaders/GLTFLoader.js";
import * as THREE from "three";

import { ClothingSidebar } from "./ColMaskPanel";
import { SymbolSidebar, SymbolData } from "./SymbolBrowser";

import { apiFetch, filterInventory, isStaticMode, staticUrl } from "./api";

type Part = { id: string; name: string; label: string; bytes: number };
type Skin = { id: string; label: string };
type Weapon = {
  id: string;
  folder: string;
  display: string;
  name_confidence: "curated" | "alias" | "exact" | "catalog" | "derived";
  sapbdb: string | null;
  primary: string;
  parts: Part[];
  skins: Skin[];
};

type Vehicle = {
  id: string;
  display: string;
  primary: string;
  parts: Part[];
  wheel_base?: string | null;
};

type VehiclePartVariant = { id: string; label: string; mesh: string };
type VehiclePart = {
  family: string;
  base: string;
  slot: string;
  display: string;
  variants: VehiclePartVariant[];
};
type SocketTransform = {
  name: string;
  bone: string;
  position: [number, number, number];
  quat: [number, number, number, number];
};

type PreviewKind = "weapon_mesh" | "vehicle_mesh" | "character_mesh" | "static_mesh" | "prop_animation" | "texture" | "video" | "none";

type Asset = {
  id: string;
  name: string;
  category: string;
  status: string;
  source_build: string;
  source_locator: string | null;
  source_sha256: string | null;
  source_package: string | null;
  source_object: string | null;
  asset_class: string | null;
  consumer_domain: string | null;
  destination: string;
  physical: boolean;
  provenance: "complete" | "incomplete" | "untracked";
  preview_kind: PreviewKind;
  preview_path: string | null;
};

type Tab = "inventory" | "weapons" | "vehicles" | "clothing" | "symbols" | "animations" | "characters";

type CharacterCategory = "body" | "clothing" | "crowd" | "character";

type CharacterItem = {
  id: string;
  name: string;
  category: CharacterCategory;
  slot: string | null;
  psk: string;
  relpath: string;
  bytes: number;
};

type AnimsetClip = {
  name: string;
  frames: number;
  rate: number;
  duration: number;
  tracks: number;
};

type Animset = {
  id: string;
  display: string;
  relpath: string;
  bone_count: number;
  clips: AnimsetClip[];
};

type PlacedDecal = {
  id: number;
  path: string;
  u: number;
  v: number;
  scale: number;
  rotation: number;
};

function normalizeAsset(raw: Partial<Asset>): Asset {
  return {
    id: raw.id ?? `asset:${raw.destination ?? raw.name ?? "unknown"}`,
    name: raw.name ?? "Unnamed asset",
    category: raw.category ?? "other",
    status: raw.status ?? "unknown",
    source_build: raw.source_build ?? "unknown",
    source_locator: raw.source_locator ?? null,
    source_sha256: raw.source_sha256 ?? null,
    source_package: raw.source_package ?? null,
    source_object: raw.source_object ?? null,
    asset_class: raw.asset_class ?? null,
    consumer_domain: raw.consumer_domain ?? null,
    destination: raw.destination ?? "",
    physical: raw.physical ?? false,
    provenance: raw.provenance ?? "incomplete",
    preview_kind: ["weapon_mesh", "vehicle_mesh", "character_mesh", "static_mesh", "prop_animation", "texture", "video"]
      .includes(raw.preview_kind ?? "")
      ? (raw.preview_kind as PreviewKind)
      : "none",
    preview_path: raw.preview_path ?? null,
  };
}

function previewLabel(kind: PreviewKind): string {
  return kind === "none" ? "Metadata only — no safe preview source" : `Preview: ${kind.replace("_", " ")}`;
}

function meshUrl(path: string, skin: string | null): string {
  const skinParam = skin ? `&skin=${encodeURIComponent(skin)}` : "";
  return staticUrl(`/api/mesh.glb?path=${encodeURIComponent(path)}${skinParam}`);
}

function countTriangles(object: THREE.Object3D): number {
  let tris = 0;
  object.traverse((child) => {
    const mesh = child as THREE.Mesh;
    if (mesh.isMesh && mesh.visible && mesh.geometry) {
      const geo = mesh.geometry;
      if (geo.index) tris += geo.index.count / 3;
      else if (geo.attributes.position) tris += geo.attributes.position.count / 3;
    }
  });
  return Math.round(tris);
}

function cloneScene(source: THREE.Object3D): THREE.Object3D {
  const clone = source.clone(true);
  // SkinnedMesh.clone() shares the source skeleton by reference (three r169:
  // copy() sets this.skeleton = source.skeleton), so the clone's bone pointers
  // target the cached scene's bones, not the cloned nodes. The animation mixer
  // drives the cloned nodes, so without a rebind the clone would render at bind
  // pose while the clock advances. Rebuild each skeleton from the cloned bones.
  const bonesByName = new Map<string, THREE.Bone>();
  clone.traverse((child) => {
    const bone = child as THREE.Bone;
    if (bone.isBone) bonesByName.set(bone.name, bone);
  });
  clone.traverse((child) => {
    const mesh = child as THREE.SkinnedMesh;
    if (!mesh.isSkinnedMesh || !mesh.skeleton) return;
    const bones = mesh.skeleton.bones.map((bone) => bonesByName.get(bone.name) ?? bone);
    mesh.skeleton = new THREE.Skeleton(bones, mesh.skeleton.boneInverses);
    mesh.updateMatrixWorld(true);
    mesh.bind(mesh.skeleton, mesh.matrixWorld);
  });
  clone.traverse((child) => {
    // Some converted PSK/GLB assets mark scene nodes invisible (e.g. hidden
    // editor root or skeleton child). An inspection viewer must show the mesh,
    // so force visibility down the whole tree. This is clone-only; it never
    // mutates the cached glTF source.
    child.visible = true;
    const mesh = child as THREE.Mesh;
    if (!mesh.isMesh) return;
    const materials = Array.isArray(mesh.material) ? mesh.material : [mesh.material];
    const cloned = materials.map((material) => material.clone());
    // THREE only pushes render items for array materials via geometry.groups;
    // an array with no groups silently renders nothing (the implicit group is
    // used only for plain materials). Collapse to material[0] when there is
    // nothing to index against, so the mesh always renders.
    const groups = (mesh.geometry as THREE.BufferGeometry).groups ?? [];
    mesh.material = cloned.length === 1 || groups.length === 0 ? cloned[0] : cloned;
  });
  return clone;
}

// Unreal (Z-up, left-handed) -> glTF (Y-up, right-handed): x -> x, y -> z, z -> -y.
const UE_TO_GLTF = new THREE.Matrix4().set(
  1, 0, 0, 0,
  0, 0, 1, 0,
  0, -1, 0, 0,
  0, 0, 0, 1,
);

function ueToGltf(position: [number, number, number], quat: [number, number, number, number]) {
  const ueMatrix = new THREE.Matrix4().compose(
    new THREE.Vector3(position[0], position[1], position[2]),
    new THREE.Quaternion(quat[0], quat[1], quat[2], quat[3]),
    new THREE.Vector3(1, 1, 1),
  );
  const gltfMatrix = new THREE.Matrix4()
    .multiplyMatrices(UE_TO_GLTF, ueMatrix)
    .multiply(UE_TO_GLTF.clone().invert());
  const resultPosition = new THREE.Vector3();
  const resultQuat = new THREE.Quaternion();
  const scale = new THREE.Vector3();
  gltfMatrix.decompose(resultPosition, resultQuat, scale);
  return { position: resultPosition, quat: resultQuat };
}

// VehiclesBulk primary ("Baked_A_Taxi/Baked_A_Taxi/SkeletalMesh3/EditorVehicle.psk")
// -> Retail/Vehicles path ("Baked_A_Taxi/SkeletalMesh3/EditorVehicle.psk").
function retailVehiclePath(primary: string): string {
  const first = primary.split("/")[0];
  return `${first}/SkeletalMesh3/EditorVehicle.psk`;
}

function partLabel(id: string): string {
  return id.replace(/^[Vv]_[A-Za-z]_/, "").replace(/_/g, " ");
}

function Model({ url, baseColorUrl, baseColorMaterialName, unlitPreview = false, onStats, onRoot, marker }: {
  url: string;
  baseColorUrl?: string | null;
  baseColorMaterialName?: string | null;
  unlitPreview?: boolean;
  onStats: (tris: number) => void;
  onRoot?: (root: THREE.Object3D | null) => void;
  marker?: [number, number, number] | null;
}) {
  const gltf = useGLTF(url);
  const camera = useThree((state) => state.camera);
  const invalidate = useThree((state) => state.invalidate);

  // Clone once so the shared glTF cache is never mutated, force visibility
  // (converted PSK scenes hide their editor root), and — on preview tabs —
  // convert PBR to unlit so lighting can never black-out a surface.
  // Weapons and clothing carry specular/BRDF defaults that read near-black
  // under standard lights without an environment map.
  const scene = useMemo(() => {
    const clone = cloneScene(gltf.scene);
    if (unlitPreview) {
      clone.traverse((child) => {
        const mesh = child as THREE.Mesh;
        if (!mesh.isMesh || !mesh.material) return;
        const materials = Array.isArray(mesh.material) ? mesh.material : [mesh.material];
        const unlitMaterials = materials.map((material) => {
          const source = material as THREE.MeshStandardMaterial;
          const preview = new THREE.MeshBasicMaterial({
            map: source.map ?? null,
            color: source.color ?? new THREE.Color(0xd5d8dc),
            side: THREE.DoubleSide,
          });
          preview.name = source.name;
          return preview;
        });
        mesh.material = unlitMaterials.length === 1 ? unlitMaterials[0] : unlitMaterials;
      });
    }
    return clone;
  }, [gltf.scene, unlitPreview]);

  useEffect(() => {
    onRoot?.(scene);
    return () => onRoot?.(null);
  }, [scene, onRoot]);

  useEffect(() => {
    if (!marker) return;
    const bounds = new THREE.Box3().setFromObject(scene);
    const size = bounds.getSize(new THREE.Vector3()).length() * 0.02;
    const mesh = new THREE.Mesh(
      new THREE.SphereGeometry(Math.max(size, 0.05), 14, 14),
      new THREE.MeshBasicMaterial({ color: 0xf0b35a, depthTest: false, transparent: true, opacity: 0.95 }),
    );
    mesh.position.set(marker[0], marker[1], marker[2]);
    scene.add(mesh);
    return () => {
      scene.remove(mesh);
    };
  }, [scene, marker]);

  useEffect(() => {
    scene.updateMatrixWorld(true);
    const bounds = new THREE.Box3().setFromObject(scene);
    if (bounds.isEmpty()) return;
    const center = bounds.getCenter(new THREE.Vector3());
    const localCenter = scene.worldToLocal(center.clone());
    const size = bounds.getSize(new THREE.Vector3());
    const maxDimension = Math.max(size.x, size.y, size.z);
    if (!Number.isFinite(maxDimension) || maxDimension <= 0) return;

    const perspectiveCamera = camera as THREE.PerspectiveCamera;
    const fov = THREE.MathUtils.degToRad(perspectiveCamera.fov || 45);
    scene.position.sub(localCenter);
    scene.traverse((child) => {
      const mesh = child as THREE.Mesh;
      if (!mesh.isMesh) return;
      mesh.visible = true;
      mesh.frustumCulled = false;
    });
    scene.updateMatrixWorld(true);
    const distance = (maxDimension / (2 * Math.tan(fov / 2))) * 1.35;
    camera.position.set(0, 0, distance);
    camera.near = Math.max(0.01, distance / 100);
    camera.far = Math.max(1000, distance * 20);
    camera.lookAt(0, 0, 0);
    camera.updateProjectionMatrix();
    camera.updateMatrixWorld(true);
    invalidate();
  }, [camera, invalidate, scene]);

  useEffect(() => {
    if (!baseColorUrl) return;
    const loader = new THREE.TextureLoader();
    loader.load(baseColorUrl, (tex) => {
      tex.flipY = false;
      tex.colorSpace = THREE.SRGBColorSpace;
        scene.traverse((child) => {
          const mesh = child as THREE.Mesh;
          if (mesh.isMesh && mesh.material) {
            const materials = Array.isArray(mesh.material) ? mesh.material : [mesh.material];
            materials.forEach((material) => {
              const mat = material as THREE.MeshStandardMaterial;
              if (baseColorMaterialName && mat.name !== baseColorMaterialName) return;
              if (!baseColorMaterialName && (mat.name ?? "").toLowerCase().includes("hair")) return;
              mat.map = tex;
              mat.needsUpdate = true;
            });
          }
      });
    });
  }, [scene, baseColorUrl, baseColorMaterialName]);





  useEffect(() => { onStats(countTriangles(scene)); }, [onStats, scene]);

  return <primitive object={scene} />;
}

// Weapon bones in the character skeleton. Prop1/Prop2 are the actual weapon
// sockets APB rigs weapons to; fall back to the hand if a skeleton lacks them.
const WEAPON_ATTACH_BONES = ["Bip01_Prop1", "Bip01_R_Hand"];

// Grip: the weapon's barrel (+X, its long axis) rides along the hand bone's +X
// so it rigidly follows the animation; a fixed -90deg roll about the barrel
// sits the top of the weapon against the back of the hand (a natural pistol
// grip). Derived and validated against the F_Body_Base skeleton + clips.
const WEAPON_GRIP_ROLL = new THREE.Quaternion().setFromAxisAngle(
  new THREE.Vector3(1, 0, 0),
  -Math.PI / 2,
);

// Attaches a weapon clone to the hand bone of an animated character. The
// weapon is a plain child of the bone, so the mixer carries it rigidly with
// the hand while the clip plays. Reports the full scene triangle count so the
// HUD reflects the character + weapon combined.
function WeaponProp({ url, root, onStats }: {
  url: string;
  root: THREE.Object3D;
  onStats: (tris: number) => void;
}) {
  const gltf = useGLTF(url);
  const weapon = useMemo(() => cloneScene(gltf.scene), [gltf.scene]);

  useEffect(() => {
    let bone: THREE.Object3D | null = null;
    root.traverse((node) => {
      if (bone) return;
      if ((node as THREE.Bone).isBone && WEAPON_ATTACH_BONES.includes(node.name)) bone = node;
    });
    if (bone === null) return;
    const attach = bone as THREE.Object3D;
    weapon.quaternion.copy(WEAPON_GRIP_ROLL);
    attach.add(weapon);
    root.updateMatrixWorld(true);
    onStats(countTriangles(root));
    return () => {
      attach.remove(weapon);
      root.updateMatrixWorld(true);
      onStats(countTriangles(root));
    };
  }, [root, weapon, onStats]);

  return null;
}

// Plays the single clip baked into an animation.glb fetch. Transport state
// (playing/speed/loop) is owned by the App; the model owns the mixer and the
// clock. seek bumps the action time directly (used by the scrub slider);
// time/duration stream back through callbacks without re-rendering the mesh.
function AnimatedModel({ url, weaponUrl, onStats, onDuration, onTime, playing, speed, loop, seek }: {
  url: string;
  weaponUrl?: string | null;
  onStats: (tris: number) => void;
  onDuration: (duration: number) => void;
  onTime: (time: number) => void;
  playing: boolean;
  speed: number;
  loop: boolean;
  seek: number | null;
}) {
  const gltf = useGLTF(url);
  const camera = useThree((state) => state.camera);
  const invalidate = useThree((state) => state.invalidate);
  const mixerRef = useRef<THREE.AnimationMixer | null>(null);
  const actionRef = useRef<THREE.AnimationAction | null>(null);
  const seekRef = useRef<number | null>(null);
  const prevSeekRef = useRef<number | null>(null);
  const playingRef = useRef(playing);
  const speedRef = useRef(speed);
  const loopRef = useRef(loop);
  playingRef.current = playing;
  speedRef.current = speed;
  loopRef.current = loop;
  // Re-arm the seek only when the value actually changes: App keeps animSeek
  // at its last value between re-renders, so blindly copying it every render
  // would re-apply the seek each frame and freeze the clock at that time.
  if (seek !== prevSeekRef.current) {
    prevSeekRef.current = seek;
    seekRef.current = seek;
  }

  const scene = useMemo(() => cloneScene(gltf.scene), [gltf.scene]);

  useEffect(() => {
    const clip = gltf.animations[0];
    if (!clip) return;
    const mixer = new THREE.AnimationMixer(scene);
    const action = mixer.clipAction(clip);
    action.play();
    action.paused = true;
    mixerRef.current = mixer;
    actionRef.current = action;
    onDuration(clip.duration);
    return () => {
      mixer.stopAllAction();
      mixerRef.current = null;
      actionRef.current = null;
    };
  }, [scene, gltf.animations, onDuration]);

  useFrame((_, delta) => {
    const mixer = mixerRef.current;
    const action = actionRef.current;
    if (!mixer || !action) return;
    if (seekRef.current !== null) {
      action.time = seekRef.current;
      seekRef.current = null;
    }
    // A LoopOnce clip finishes with the action disabled; pressing Play again
    // must restart it, not silently keep it stuck at the final pose.
    if (playingRef.current && !action.enabled) {
      action.reset();
      action.play();
    }
    action.paused = !playingRef.current;
    action.timeScale = speedRef.current;
    action.setLoop(loopRef.current ? THREE.LoopRepeat : THREE.LoopOnce, 1);
    mixer.update(playingRef.current ? delta : 0);
    onTime(action.time);
  });

  // Frame the character like the static Model path: center it, then place the
  // camera at the +Z distance the FOV implies so idle clips stay in view.
  useEffect(() => {
    scene.updateMatrixWorld(true);
    const bounds = new THREE.Box3().setFromObject(scene);
    if (bounds.isEmpty()) return;
    const center = bounds.getCenter(new THREE.Vector3());
    const localCenter = scene.worldToLocal(center.clone());
    const size = bounds.getSize(new THREE.Vector3());
    const maxDimension = Math.max(size.x, size.y, size.z);
    if (!Number.isFinite(maxDimension) || maxDimension <= 0) return;

    const perspectiveCamera = camera as THREE.PerspectiveCamera;
    const fov = THREE.MathUtils.degToRad(perspectiveCamera.fov || 45);
    scene.position.sub(localCenter);
    scene.traverse((child) => {
      const mesh = child as THREE.Mesh;
      if (!mesh.isMesh) return;
      mesh.visible = true;
      mesh.frustumCulled = false;
    });
    scene.updateMatrixWorld(true);
    const distance = (maxDimension / (2 * Math.tan(fov / 2))) * 1.6;
    camera.position.set(0, 0, distance);
    camera.near = Math.max(0.01, distance / 100);
    camera.far = Math.max(1000, distance * 20);
    camera.lookAt(0, 0, 0);
    camera.updateProjectionMatrix();
    camera.updateMatrixWorld(true);
    invalidate();
  }, [camera, invalidate, scene]);

  // While a weapon is attached, WeaponProp owns the triangle report (it counts
  // the whole scene including the weapon); without one, report the character.
  useEffect(() => {
    if (weaponUrl) return;
    onStats(countTriangles(scene));
  }, [onStats, scene, weaponUrl]);

  return (
    <>
      <primitive object={scene} />
      {weaponUrl && (
        <WeaponProp key={weaponUrl} url={weaponUrl} root={scene} onStats={onStats} />
      )}
    </>
  );
}

// Shared transport for the Animations and Characters lanes: clip select +
// play/pause/loop/speed + scrub. extra renders above the clip select (the
// Animations lane adds its body-mesh quick picker there).
function AnimTransportPanel({ animset, clip, playing, speed, loop, duration, time, extra, title, onSelectClip, onTogglePlay, onSetLoop, onSetSpeed, onSeek }: {
  animset: Animset;
  clip: AnimsetClip | null;
  playing: boolean;
  speed: number;
  loop: boolean;
  duration: number;
  time: number;
  extra?: React.ReactNode;
  title?: string;
  onSelectClip: (clip: AnimsetClip) => void;
  onTogglePlay: () => void;
  onSetLoop: (loop: boolean) => void;
  onSetSpeed: (speed: number) => void;
  onSeek: (time: number) => void;
}) {
  return (
    <div className="viewer-controls">
      <div className="viewer-controls-title">{title ?? `${animset.display} · ${animset.bone_count} bones`}</div>
      {extra}
      <label className="viewer-control">
        <span>Clip</span>
        <select value={clip?.name ?? ""} onChange={(e) => {
          const found = animset.clips.find((c) => c.name === e.target.value);
          if (found) onSelectClip(found);
        }}>
          {animset.clips.map((clip) => (
            <option key={clip.name} value={clip.name}>{clip.name} · {clip.duration.toFixed(1)}s</option>
          ))}
        </select>
      </label>
      <div className="anim-transport">
        <div className="anim-transport-row">
          <button className="anim-play" onClick={onTogglePlay}>
            {playing ? "⏸ Pause" : "▶ Play"}
          </button>
          <label className="anim-check">
            <input type="checkbox" checked={loop} onChange={(e) => onSetLoop(e.target.checked)} />
            <span>Loop</span>
          </label>
          <label className="viewer-control anim-speed">
            <span>Speed</span>
            <select value={speed} onChange={(e) => onSetSpeed(Number(e.target.value))}>
              <option value={0.25}>0.25×</option>
              <option value={0.5}>0.5×</option>
              <option value={1}>1×</option>
              <option value={2}>2×</option>
            </select>
          </label>
        </div>
        <div className="anim-scrub">
          <input
            type="range"
            min={0}
            max={duration || 1}
            step={0.01}
            value={Math.min(time, duration || 1)}
            onChange={(e) => onSeek(Number(e.target.value))}
            aria-label="Seek animation"
          />
          <span className="anim-time">{time.toFixed(1)}s / {duration.toFixed(1)}s</span>
        </div>
      </div>
    </div>
  );
}

function AssembledVehicle({ baseUrl, partUrl, socketsUrl, onStats, onWheelCount, onError }: {
  baseUrl: string;
  partUrl: string;
  socketsUrl: string;
  onStats: (tris: number) => void;
  onWheelCount: (count: number) => void;
  onError: (message: string) => void;
}) {
  const gltfBase = useLoader(GLTFLoader, baseUrl);
  const gltfPart = useLoader(GLTFLoader, partUrl);
  const baseScene = useMemo(() => cloneScene(gltfBase.scene), [gltfBase.scene]);
  const partScene = useMemo(() => cloneScene(gltfPart.scene), [gltfPart.scene]);
  const [sockets, setSockets] = useState<SocketTransform[] | null>(null);
  const wheelGroupRef = useRef<THREE.Group>(null);

  useEffect(() => {
    const controller = new AbortController();
    setSockets(null);
    apiFetch(socketsUrl, { signal: controller.signal })
      .then(async (response) => {
        if (!response.ok) throw new Error(`sockets failed: ${response.status}`);
        const data = await response.json();
        // Cars name every corner "Wheel"; trucks/vans name the front pair
        // "Wheel" and the rear pair "RearWheel" — union both so chassis with
        // mixed naming still assemble their full wheel set.
        const wheelSockets = (data.sockets ?? []).filter((socket: SocketTransform) =>
          socket.name === "Wheel" || socket.name === "RearWheel");
        if (wheelSockets.length !== 4) {
          throw new Error(`expected 4 wheel sockets, received ${wheelSockets.length}`);
        }
        setSockets(wheelSockets);
      })
      .catch((error: unknown) => {
        if ((error as DOMException)?.name !== "AbortError") {
          setSockets([]);
          onError(error instanceof Error ? error.message : String(error));
        }
      });
    return () => controller.abort();
  }, [socketsUrl, onError]);

  useEffect(() => {
    const group = wheelGroupRef.current;
    if (!group || !sockets || sockets.length !== 4) return;
    group.clear();
    baseScene.traverse((child) => {
      const mesh = child as THREE.Mesh;
      if (!mesh.isMesh) return;
      const materials = Array.isArray(mesh.material) ? mesh.material : [mesh.material];
      if (materials.some((mat) => (mat.name ?? "").toLowerCase().includes("wheel"))) {
        mesh.visible = false;
      }
    });
    for (const socket of sockets) {
      const { position, quat } = ueToGltf(socket.position, socket.quat);
      const wheel = cloneScene(partScene);
      wheel.position.copy(position);
      wheel.quaternion.copy(quat);
      group.add(wheel);
    }
    onStats(countTriangles(baseScene) + countTriangles(group));
    onWheelCount(sockets.length);
  }, [baseScene, partScene, sockets, onStats, onWheelCount]);

  return (
    <group>
      <primitive object={baseScene} />
      <group ref={wheelGroupRef} />
    </group>
  );
}

function DecalDropTarget({ rootRef, onDropDecal }: {
  rootRef: React.RefObject<THREE.Object3D | null>;
  onDropDecal: (symbol: SymbolData, uv: THREE.Vector2, localPoint: THREE.Vector3) => void;
}) {
  const gl = useThree((state) => state.gl);
  const camera = useThree((state) => state.camera);
  const raycaster = useMemo(() => new THREE.Raycaster(), []);
  const handlerRef = useRef(onDropDecal);
  handlerRef.current = onDropDecal;

  useEffect(() => {
    const element = gl.domElement;
    const onDragOver = (event: DragEvent) => {
      event.preventDefault();
    };
    const onDrop = (event: DragEvent) => {
      const raw = event.dataTransfer?.getData("application/x-apb-symbol");
      if (!raw) return;
      event.preventDefault();
      const rect = element.getBoundingClientRect();
      const ndc = new THREE.Vector2(
        ((event.clientX - rect.left) / rect.width) * 2 - 1,
        -((event.clientY - rect.top) / rect.height) * 2 + 1,
      );
      raycaster.setFromCamera(ndc, camera);
      const root = rootRef.current;
      if (!root) return;
      const hits = raycaster.intersectObject(root, true);
      for (const hit of hits) {
        if (!hit.uv) continue;
        try {
          const symbol = JSON.parse(raw) as SymbolData;
          const local = root.worldToLocal(hit.point.clone());
          handlerRef.current(symbol, hit.uv, local);
        } catch {
          /* malformed drag payload */
        }
        break;
      }
    };
    element.addEventListener("dragover", onDragOver);
    element.addEventListener("drop", onDrop);
    return () => {
      element.removeEventListener("dragover", onDragOver);
      element.removeEventListener("drop", onDrop);
    };
  }, [gl, camera, raycaster, rootRef]);
  return null;
}

// Same raycaster-on-canvas approach as DecalDropTarget, for click placement.
// Raycasting against the mounted root is more reliable than R3F declarative
// event props on imported scenes, whose child meshes are plain three.js
// objects outside the reconciler.
function ClickPicker({ rootRef, active, onPick }: {
  rootRef: React.RefObject<THREE.Object3D | null>;
  active: boolean;
  onPick: (uv: THREE.Vector2, localPoint: THREE.Vector3) => void;
}) {
  const gl = useThree((state) => state.gl);
  const camera = useThree((state) => state.camera);
  const raycaster = useMemo(() => new THREE.Raycaster(), []);
  const handlerRef = useRef(onPick);
  handlerRef.current = onPick;

  useEffect(() => {
    if (!active) return;
    const element = gl.domElement;
    // Place on pointerup only when the pointer barely moved: a drag is the
    // user rotating the model (OrbitControls), not a placement intent.
    let startX = 0;
    let startY = 0;
    let down = false;
    const onPointerDown = (event: PointerEvent) => {
      startX = event.clientX;
      startY = event.clientY;
      down = true;
    };
    const onPointerUp = (event: PointerEvent) => {
      if (!down) return;
      down = false;
      const dx = event.clientX - startX;
      const dy = event.clientY - startY;
      if (dx * dx + dy * dy > 36) return; // 6px threshold: treat as a drag
      const rect = element.getBoundingClientRect();
      const ndc = new THREE.Vector2(
        ((event.clientX - rect.left) / rect.width) * 2 - 1,
        -((event.clientY - rect.top) / rect.height) * 2 + 1,
      );
      raycaster.setFromCamera(ndc, camera);
      const root = rootRef.current;
      if (!root) return;
      const hits = raycaster.intersectObject(root, true);
      for (const hit of hits) {
        if (!hit.uv) continue;
        const local = root.worldToLocal(hit.point.clone());
        handlerRef.current(hit.uv, local);
        break;
      }
    };
    element.addEventListener("pointerdown", onPointerDown);
    element.addEventListener("pointerup", onPointerUp);
    return () => {
      element.removeEventListener("pointerdown", onPointerDown);
      element.removeEventListener("pointerup", onPointerUp);
    };
  }, [gl, camera, raycaster, rootRef, active]);
  return null;
}

function CameraProbe() {
  const last = useRef(0);
  const rscene = useThree((state) => state.scene);
  useFrame(({ camera, gl }) => {
    const now = performance.now();
    if (now - last.current < 200) return;
    last.current = now;
    const q = camera.quaternion;
    const stage = document.querySelector(".stage");
    if (stage) {
      let frustum = "?";
      try {
        const matrix = new THREE.Matrix4().multiplyMatrices(camera.projectionMatrix, camera.matrixWorldInverse);
        const frust = new THREE.Frustum().setFromProjectionMatrix(matrix);
        const hits: string[] = [];
        rscene.traverse((child) => {
          const mesh = child as THREE.Mesh;
          if (!mesh.isMesh) return;
          const inside = frust.intersectsObject(mesh);
          if (inside) hits.push(mesh.name || "anon");
        });
        frustum = hits.join(",") || "NONE";
      } catch (err) {
        frustum = "err:" + String(err).slice(0, 60);
      }
      stage.setAttribute("data-camera", `p(${camera.position.x.toFixed(2)},${camera.position.y.toFixed(2)},${camera.position.z.toFixed(2)}) q(${q.x.toFixed(3)},${q.y.toFixed(3)},${q.z.toFixed(3)},${q.w.toFixed(3)}) calls=${gl.info.render.calls} tris=${gl.info.render.triangles} frustum=${frustum}`);
    }
  });
  return null;
}

function SymbolPlane({ textureUrl, onStats }: { textureUrl: string; onStats: (tris: number) => void }) {
  const texture = dreiUseTexture(textureUrl);
  useEffect(() => { onStats(2); }, [onStats]);
  return (
    <mesh>
      <planeGeometry args={[1, 1]} />
      <meshStandardMaterial map={texture} transparent side={THREE.DoubleSide} />
    </mesh>
  );
}

// Click-to-zoom + drag-pan for flat texture previews. The image fits the
// stage at zoom=1; clicking (or wheel-scrolling) zooms toward the cursor up
// to 8x, dragging pans while zoomed, double-click resets. Zoom anchors keep
// the texture point under the cursor stationary across scale changes. The
// keyed remount in App resets all state when a new asset is selected.
function ZoomableTexture({ src, alt }: { src: string; alt: string }) {
  const wrapRef = useRef<HTMLDivElement>(null);
  const [zoom, setZoom] = useState(1);
  const [pan, setPan] = useState({ x: 0, y: 0 });
  const [dragging, setDragging] = useState(false);
  const [view, setView] = useState({ w: 0, h: 0 });
  const [natural, setNatural] = useState({ w: 0, h: 0 });
  const zoomRef = useRef(zoom);
  const panRef = useRef(pan);
  const dragRef = useRef<{ startX: number; startY: number; panX: number; panY: number; moved: boolean } | null>(null);
  const lastClickRef = useRef(0);
  zoomRef.current = zoom;
  panRef.current = pan;

  useEffect(() => {
    const wrap = wrapRef.current;
    if (!wrap) return;
    const observer = new ResizeObserver((entries) => {
      const rect = entries[0]?.contentRect;
      if (rect) setView({ w: rect.width, h: rect.height });
    });
    observer.observe(wrap);
    return () => observer.disconnect();
  }, []);

  const fitScale = () => {
    if (natural.w <= 0 || natural.h <= 0 || view.w <= 0 || view.h <= 0) return 1;
    return Math.min(view.w / natural.w, view.h / natural.h);
  };
  const center = (scale: number, size: number, viewport: number) => (viewport - size * scale) / 2;
  const clampPan = (value: number, scale: number, axis: "x" | "y") => {
    const naturalSize = axis === "x" ? natural.w : natural.h;
    const viewport = axis === "x" ? view.w : view.h;
    const maxPan = Math.max(0, (naturalSize * scale - viewport) / 2);
    return Math.max(-maxPan, Math.min(maxPan, value));
  };

  // Re-zoom so the texture point under the cursor stays put (image coordinates
  // are invariant across the scale change).
  const applyZoom = useCallback((target: number, clientX: number, clientY: number) => {
    const wrap = wrapRef.current;
    if (!wrap || natural.w <= 0 || natural.h <= 0) return;
    const rect = wrap.getBoundingClientRect();
    const cursor = { x: clientX - rect.left, y: clientY - rect.top };
    const clamped = Math.max(1, Math.min(8, target));
    const fromScale = fitScale() * zoomRef.current;
    const toScale = fitScale() * clamped;
    const fromPan = panRef.current;
    const imageX = (cursor.x - (center(fromScale, natural.w, view.w) + fromPan.x)) / fromScale;
    const imageY = (cursor.y - (center(fromScale, natural.h, view.h) + fromPan.y)) / fromScale;
    const nextPan = {
      x: clampPan(cursor.x - center(toScale, natural.w, view.w) - imageX * toScale, toScale, "x"),
      y: clampPan(cursor.y - center(toScale, natural.h, view.h) - imageY * toScale, toScale, "y"),
    };
    setZoom(clamped);
    setPan(nextPan);
  }, [view.w, view.h, natural.w, natural.h]);

  const reset = useCallback(() => {
    setZoom(1);
    setPan({ x: 0, y: 0 });
  }, []);

  useEffect(() => {
    const wrap = wrapRef.current;
    if (!wrap) return;
    const onWheel = (event: WheelEvent) => {
      event.preventDefault();
      const factor = Math.pow(1.0018, -event.deltaY);
      applyZoom(zoomRef.current * factor, event.clientX, event.clientY);
    };
    wrap.addEventListener("wheel", onWheel, { passive: false });
    return () => wrap.removeEventListener("wheel", onWheel);
  }, [applyZoom]);

  const onPointerDown = (event: React.PointerEvent) => {
    event.currentTarget.setPointerCapture(event.pointerId);
    dragRef.current = {
      startX: event.clientX,
      startY: event.clientY,
      panX: panRef.current.x,
      panY: panRef.current.y,
      moved: false,
    };
    setDragging(true);
  };
  const onPointerMove = (event: React.PointerEvent) => {
    const drag = dragRef.current;
    if (!drag) return;
    const dx = event.clientX - drag.startX;
    const dy = event.clientY - drag.startY;
    if (Math.abs(dx) + Math.abs(dy) > 4) drag.moved = true;
    if (zoomRef.current > 1) {
      const scale = fitScale() * zoomRef.current;
      setPan({
        x: clampPan(drag.panX + dx, scale, "x"),
        y: clampPan(drag.panY + dy, scale, "y"),
      });
    }
  };
  const onPointerUp = (event: React.PointerEvent) => {
    const drag = dragRef.current;
    dragRef.current = null;
    setDragging(false);
    if (!drag || drag.moved) return; // a pan, not a click
    // Suppress the second half of a double-click (onDoubleClick resets); the
    // suppression timestamp updates even on skipped clicks, so a rapid
    // triple-click collapses to one toggle + reset — the intended contract.
    const now = Date.now();
    if (now - lastClickRef.current < 350) {
      lastClickRef.current = now;
      return;
    }
    lastClickRef.current = now;
    applyZoom(zoomRef.current === 1 ? 2.5 : 1, event.clientX, event.clientY);
  };
  // Pointer capture can be cancelled without a matching pointerup (touch
  // interrupt, OS gesture, Alt-Tab mid-drag): clear the drag so the cursor
  // never sticks at "grabbing" and the next move doesn't pan from a stale
  // start.
  const onPointerCancel = () => {
    dragRef.current = null;
    setDragging(false);
  };
  const onDoubleClick = () => {
    lastClickRef.current = Date.now();
    reset();
  };

  const scale = fitScale() * zoom;
  // Before ResizeObserver has measured the stage, view is {0,0} and the
  // centering math would park the texture half off-screen; keep it invisible
  // for the single frame until the stage size lands.
  const measured = view.w > 0 && view.h > 0;
  const tx = measured ? center(scale, natural.w, view.w) + pan.x : 0;
  const ty = measured ? center(scale, natural.h, view.h) + pan.y : 0;
  return (
    <div
      ref={wrapRef}
      className={`flat-preview-zoom ${zoom > 1 ? "zoomed" : ""} ${dragging ? "dragging" : ""}`}
      onPointerDown={onPointerDown}
      onPointerMove={onPointerMove}
      onPointerUp={onPointerUp}
      onPointerCancel={onPointerCancel}
      onDoubleClick={onDoubleClick}
      aria-label={`${alt} — click to zoom, drag to pan`}
    >
      <img
        className="flat-preview-media zoomable"
        src={src}
        alt={alt}
        style={{ transform: `translate(${tx}px, ${ty}px) scale(${scale})`, visibility: measured ? undefined : "hidden" }}
        draggable={false}
        onLoad={(event) => {
          const img = event.currentTarget;
          setNatural({ w: img.naturalWidth, h: img.naturalHeight });
        }}
      />
      <div className="flat-preview-hud">
        {zoom > 1 ? (
          <>
            <span>Zoom {Math.round(zoom * 100)}%</span>
            <button
              type="button"
              onClick={reset}
              onPointerDown={(event) => event.stopPropagation()}
              onPointerUp={(event) => event.stopPropagation()}
            >
              Reset
            </button>
          </>
        ) : (
          <span>Click to zoom · drag to pan</span>
        )}
      </div>
    </div>
  );
}

class ModelErrorBoundary extends React.Component<
  { url: string; onError: (msg: string) => void; children: React.ReactNode },
  { failedUrl: string | null }
> {
  state = { failedUrl: null as string | null };
  static getDerivedStateFromError() { return { failedUrl: "__current__" }; }
  componentDidCatch(error: Error) {
    this.setState({ failedUrl: this.props.url });
    this.props.onError(error.message);
  }
  componentDidUpdate(prev: { url: string }) {
    if (prev.url !== this.props.url && this.state.failedUrl) {
      this.setState({ failedUrl: null });
    }
  }
  render() {
    if (this.state.failedUrl === "__current__" || this.state.failedUrl === this.props.url) return null;
    return this.props.children;
  }
}

export function App() {
  const [activeTab, setActiveTab] = useState<Tab>("inventory");
  const [assets, setAssets] = useState<Asset[]>([]);
  const [assetTotal, setAssetTotal] = useState(0);
  const [assetMatchCount, setAssetMatchCount] = useState(0);
  const [assetOffset, setAssetOffset] = useState(0);
  const [assetHasMore, setAssetHasMore] = useState(false);
  const [assetCategories, setAssetCategories] = useState<Record<string, number>>({});
  const [assetStatuses, setAssetStatuses] = useState<Record<string, number>>({});
  const [assetSourceBuilds, setAssetSourceBuilds] = useState<string[]>([]);
  const [assetQuery, setAssetQuery] = useState("");
  const [assetCategory, setAssetCategory] = useState("");
  const [assetStatus, setAssetStatus] = useState("");
  const [assetSourceBuild, setAssetSourceBuild] = useState("");
  const [selectedAsset, setSelectedAsset] = useState<Asset | null>(null);
  const [weapons, setWeapons] = useState<Weapon[]>([]);
  const [vehicles, setVehicles] = useState<Vehicle[]>([]);
  const [selectedWeapon, setSelectedWeapon] = useState<Weapon | null>(null);
  const [selectedVehicle, setSelectedVehicle] = useState<Vehicle | null>(null);
  const [selectedPart, setSelectedPart] = useState<string | null>(null);
  const [vehicleParts, setVehicleParts] = useState<VehiclePart[]>([]);
  const [selectedWheelVariant, setSelectedWheelVariant] = useState<string | null>(null);
  const [vehicleAnim, setVehicleAnim] = useState(false);
  const [selectedSkin, setSelectedSkin] = useState<string | null>(null);
  const [clothingMeshUrl, setClothingMeshUrl] = useState<string | null>(null);
  const [clothingName, setClothingName] = useState("");
  const [symbolTextureUrl, setSymbolTextureUrl] = useState<string | null>(null);
  const [symbolName, setSymbolName] = useState("");
  const [animsets, setAnimsets] = useState<Animset[]>([]);
  const [selectedAnimset, setSelectedAnimset] = useState<Animset | null>(null);
  const [selectedClip, setSelectedClip] = useState<AnimsetClip | null>(null);
  const [characters, setCharacters] = useState<CharacterItem[]>([]);
  const [selectedCharacter, setSelectedCharacter] = useState<CharacterItem | null>(null);
  const [charCategory, setCharCategory] = useState<"all" | CharacterCategory>("all");
  const [charQuery, setCharQuery] = useState("");
  const [previewWeapon, setPreviewWeapon] = useState<Weapon | null>(null);
  const [animMesh, setAnimMesh] = useState("F_Body_Base");
  const [animPlaying, setAnimPlaying] = useState(true);
  const [animSpeed, setAnimSpeed] = useState(1);
  const [animLoop, setAnimLoop] = useState(true);
  const [animSeek, setAnimSeek] = useState<number | null>(null);
  const [animDuration, setAnimDuration] = useState(0);
  const [animTime, setAnimTime] = useState(0);
  const [status, setStatus] = useState<"idle" | "loading" | "ready" | "error">("idle");
  const [triangles, setTriangles] = useState(0);
  const [assembledWheelCount, setAssembledWheelCount] = useState(0);
  const [error, setError] = useState<string | null>(null);

  // Auto-rotate the preview while the user is idle; any viewport interaction
  // pauses it and a short idle window resumes it.
  const [autoRotate, setAutoRotate] = useState(true);
  const autoRotateTimer = useRef<number | null>(null);
  const clearAutoRotateTimer = useCallback(() => {
    if (autoRotateTimer.current !== null) {
      window.clearTimeout(autoRotateTimer.current);
      autoRotateTimer.current = null;
    }
  }, []);
  const pauseAutoRotate = useCallback(() => {
    setAutoRotate(false);
    clearAutoRotateTimer();
  }, [clearAutoRotateTimer]);
  const scheduleAutoRotateResume = useCallback(() => {
    clearAutoRotateTimer();
    autoRotateTimer.current = window.setTimeout(() => setAutoRotate(true), 2500);
  }, [clearAutoRotateTimer]);
  useEffect(() => clearAutoRotateTimer, [clearAutoRotateTimer]);
  const controlsRef = useRef<any>(null);

  // Clothing Compositor State
  const [regionColors, setRegionColors] = useState<Record<string, string>>({});
  const [decals, setDecals] = useState<PlacedDecal[]>([]);
  const [composedBaseColorUrl, setComposedBaseColorUrl] = useState<string | null>(null);
  const [selectedSymbolForDecal, setSelectedSymbolForDecal] = useState<SymbolData | null>(null);
  const [pickMarker, setPickMarker] = useState<{ id: number; pos: [number, number, number] } | null>(null);
  const pickMarkerTimer = useRef<number | null>(null);
  const modelRootRef = useRef<THREE.Object3D | null>(null);
  const armed = activeTab === "clothing" && !!selectedSymbolForDecal;

  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if (event.key === "Escape") setSelectedSymbolForDecal(null);
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, []);

  const handleModelRoot = useCallback((root: THREE.Object3D | null) => {
    modelRootRef.current = root;
  }, []);

  // UV from the three.js pick is glTF-convention (v=0 at the image bottom);
  // the server compositor pastes with PIL where v=0 is the image top, so flip.
  const placeDecal = useCallback((path: string, u: number, v: number, localPoint?: THREE.Vector3) => {
    setDecals((prev) => [...prev, {
      id: Date.now() + Math.random(),
      path,
      u,
      v: 1 - v,
      scale: 0.5,
      rotation: 0,
    }]);
    if (localPoint) {
      setPickMarker({ id: Date.now(), pos: [localPoint.x, localPoint.y, localPoint.z] });
      if (pickMarkerTimer.current !== null) window.clearTimeout(pickMarkerTimer.current);
      pickMarkerTimer.current = window.setTimeout(() => setPickMarker(null), 1100);
    }
  }, []);

  const updateDecal = useCallback((index: number, patch: Partial<Pick<PlacedDecal, "scale" | "rotation">>) => {
    setDecals((prev) => prev.map((decal, i) => (i === index ? { ...decal, ...patch } : decal)));
  }, []);

  const removeDecal = useCallback((index: number) => {
    setDecals((prev) => prev.filter((_, i) => i !== index));
  }, []);

  const handleClothingMeshChange = useCallback((url: string | null, name: string) => {
    setClothingMeshUrl(url);
    setClothingName(name);
    setRegionColors({});
    setDecals([]);
    setComposedBaseColorUrl(null);
  }, []);

  const handleClothingColorChange = useCallback((_itemId: string, region: string, color: string) => {
    setRegionColors((prev) => ({ ...prev, [region]: color }));
  }, []);

  useEffect(() => {
    if (!clothingName) return;
    const timer = setTimeout(() => {
      apiFetch("/api/compose/clothing", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          item: clothingName,
          colors: regionColors,
          decals: decals
        })
      })
      .then(async (response) => {
        if (!response.ok) throw new Error(await response.text());
        return response.blob();
      })
      .then(blob => {
        const url = URL.createObjectURL(blob);
        setComposedBaseColorUrl(url);
      })
      .catch(e => {
        setComposedBaseColorUrl(null);
        setError(`Composition failed: ${e}`);
      });
    }, 500); // 500ms debounce
    return () => clearTimeout(timer);
  }, [clothingName, regionColors, decals]);

  useEffect(() => {
    setAssetOffset(0);
  }, [assetQuery, assetCategory, assetStatus, assetSourceBuild]);

  useEffect(() => {
    const controller = new AbortController();
    const timer = window.setTimeout(() => {
      const params = new URLSearchParams({ offset: String(assetOffset), limit: "250" });
      if (assetQuery.trim()) params.set("query", assetQuery.trim());
      if (assetCategory) params.set("category", assetCategory);
      if (assetStatus) params.set("status", assetStatus);
      if (assetSourceBuild) params.set("source_build", assetSourceBuild);
      apiFetch(`/api/inventory/assets?${params.toString()}`, { signal: controller.signal })
        .then(async (response) => {
          if (!response.ok) throw new Error(`/api/inventory/assets: ${response.status}`);
          const text = await response.text();
          try {
            return JSON.parse(text);
          } catch {
            throw new Error(`/api/inventory/assets: backend returned non-JSON (${text.slice(0, 80) || "empty"})`);
          }
        })
        .then((data) => {
          // Static mode: the baked inventory.json is the full unfiltered list
          // (there is no server to paginate/filter), so apply the same
          // contract client-side.
          const resolved = isStaticMode()
            ? filterInventory(data, {
                query: assetQuery,
                category: assetCategory,
                status: assetStatus,
                source_build: assetSourceBuild,
                offset: assetOffset,
                limit: 250,
              })
            : data;
          const page = (resolved.assets ?? []).map((asset: Partial<Asset>) => normalizeAsset(asset));
          setAssets((current) => assetOffset === 0 ? page : [...current, ...page]);
          setAssetTotal(resolved.total ?? 0);
          setAssetMatchCount(resolved.count ?? 0);
          setAssetHasMore(Boolean(resolved.has_more));
          setAssetCategories(resolved.categories ?? {});
          setAssetStatuses(resolved.statuses ?? {});
          setAssetSourceBuilds(resolved.source_builds ?? []);
          setSelectedAsset((current: Asset | null) => current && (assetOffset !== 0 || page.some((asset: Asset) => asset.id === current.id))
            ? current
            : page[0] ?? null);
        })
        .catch((e: any) => { if (e.name !== "AbortError") setError(String(e)); });
    }, 150);
    return () => {
      window.clearTimeout(timer);
      controller.abort();
    };
  }, [assetQuery, assetCategory, assetStatus, assetSourceBuild, assetOffset]);

  useEffect(() => {
    const controller = new AbortController();
    const request = (url: string) => apiFetch(url, { signal: controller.signal }).then(async (response) => {
      if (!response.ok) throw new Error(`${url}: ${response.status}`);
      const text = await response.text();
      try {
        return JSON.parse(text);
      } catch {
        // A non-JSON body (proxy error page, stale backend HTML) must not
        // surface as a raw "Unexpected token" parse error — report it clearly.
        throw new Error(`${url}: backend returned non-JSON (${text.slice(0, 80) || "empty"})`);
      }
    });
    request("/api/catalog/weapons")
      .then((data) => {
        setWeapons(data.weapons ?? []);
        setSelectedWeapon((current) => current ?? data.weapons?.[0] ?? null);
      })
      .catch((e) => { if (e.name !== "AbortError") setError(String(e)); });
    request("/api/catalog/vehicles")
      .then((data) => {
        setVehicles(data.vehicles ?? []);
        setSelectedVehicle((current) => current ?? data.vehicles?.[0] ?? null);
      })
      .catch((e) => { if (e.name !== "AbortError") setError(String(e)); });
    request("/api/catalog/vehicle_parts")
      .then((data) => setVehicleParts(data.parts ?? []))
      .catch((e) => { if (e.name !== "AbortError") setError(String(e)); });
    request("/api/catalog/animations")
      .then((data) => {
        const animsets = (data.animsets ?? []) as Animset[];
        setAnimsets(animsets);
        const preferred = animsets.find((animset) =>
          animset.display.toLowerCase().includes("locomotion"))
          ?? animsets.find((animset) =>
            animset.display.toLowerCase().includes("contact"));
        const initial = preferred ?? animsets[0] ?? null;
        if (initial) {
          setSelectedAnimset(initial);
          setSelectedClip(initial.clips[0] ?? null);
        }
      })
      .catch((e) => { if (e.name !== "AbortError") setError(String(e)); });
    request("/api/catalog/characters")
      .then((data) => {
        const characters = (data.characters ?? []) as CharacterItem[];
        setCharacters(characters);
        setSelectedCharacter((current) => current
          ?? characters.find((character) => character.category === "body")
          ?? characters[0]
          ?? null);
      })
      .catch((e) => { if (e.name !== "AbortError") setError(String(e)); });
    return () => controller.abort();
  }, []);

  const selectWeapon = useCallback((weapon: Weapon) => {
    setSelectedWeapon(weapon);
    setSelectedSkin(null);
  }, []);

  const selectVehicle = useCallback((vehicle: Vehicle) => {
    setSelectedWheelVariant(null);
    setSelectedVehicle(vehicle);
  }, []);

  const selectAnimset = useCallback((animset: Animset) => {
    setSelectedAnimset(animset);
    setSelectedClip(animset.clips[0] ?? null);
    setAnimSeek(0);
    setAnimTime(0);
    setAnimPlaying(true);
  }, []);

  const selectAnimClip = useCallback((clip: AnimsetClip) => {
    setSelectedClip(clip);
    setAnimSeek(0);
    setAnimTime(0);
    setAnimPlaying(true);
  }, []);

  const selectCharacter = useCallback((character: CharacterItem) => {
    setSelectedCharacter(character);
    setAnimSeek(0);
    setAnimTime(0);
    setAnimPlaying(true);
  }, []);

  const wheelParts = useMemo(() => {
    const map: Record<string, VehiclePartVariant[]> = {};
    for (const part of vehicleParts) {
      if (part.slot !== "Wheel" && part.slot !== "Wheels") continue;
      const existing = map[part.base] ?? [];
      map[part.base] = [...existing, ...part.variants];
    }
    return map;
  }, [vehicleParts]);
  const wheelPartsForVehicle = selectedVehicle
    ? (wheelParts[selectedVehicle.wheel_base ?? selectedVehicle.id] ?? [])
    : [];

  const filteredCharacters = useMemo(() => {
    const needle = charQuery.trim().toLowerCase();
    return characters.filter((character) => {
      if (charCategory !== "all" && character.category !== charCategory) return false;
      if (needle && !character.name.toLowerCase().includes(needle)) return false;
      return true;
    });
  }, [characters, charCategory, charQuery]);

  const characterGroups = useMemo(() => {
    const categoryLabels: Record<CharacterCategory, string> = {
      body: "Bodies",
      clothing: "Clothing",
      crowd: "Crowd NPCs",
      character: "Full characters",
    };
    const categoryOrder: Record<CharacterCategory, number> = { body: 0, clothing: 1, crowd: 2, character: 3 };
    const byKey = new Map<string, { label: string; category: CharacterCategory; items: CharacterItem[] }>();
    for (const character of filteredCharacters) {
      const isClothing = character.category === "clothing";
      const label = isClothing ? (character.slot ?? "Clothing") : categoryLabels[character.category];
      const key = isClothing ? `clothing:${label}` : `category:${character.category}`;
      const group = byKey.get(key) ?? { label, category: character.category, items: [] };
      group.items.push(character);
      byKey.set(key, group);
    }
    return [...byKey.values()].sort((a, b) =>
      categoryOrder[a.category] - categoryOrder[b.category]
      || a.label.localeCompare(b.label)
      || a.items[0]!.name.localeCompare(b.items[0]!.name));
  }, [filteredCharacters]);

  useEffect(() => {
    setSelectedPart(selectedWeapon?.primary ?? null);
    setSelectedSkin(null);
  }, [selectedWeapon]);

  const weaponMeshUrl = useMemo(
    () => (selectedPart ? meshUrl(selectedPart, selectedSkin) : null),
    [selectedPart, selectedSkin],
  );
  const vehicleMeshUrl = selectedVehicle?.primary
    ? staticUrl(`/api/vehicle.glb?path=${encodeURIComponent(selectedVehicle.primary)}`)
    : null;
  const vehicleAnimUrl = selectedVehicle?.primary
    ? staticUrl(`/api/vehicle_animation.glb?path=${encodeURIComponent(selectedVehicle.primary)}`)
    : null;

  // Vehicles lane animated preview: a synthesized wheel-spin animset (the
  // server bakes a "Wheel Spin" clip rotating every wheel bone). Synthetic
  // because retail chassis animsets don't exist — Anim_LC_Vehicle_* are
  // driver character rigs.
  // Synthetic animset for the transport. The clip metadata mirrors the server
  // generator constants in vehicles.py (_WHEEL_SPIN_RATE=30, _WHEEL_SPIN_FRAMES
  // =60, one full axle revolution per loop); the scrubber max is overridden by
  // onDuration from the real GLB clip, so only the option label can drift.
  const vehicleAnimset = useMemo<Animset | null>(() => {
    if (!selectedVehicle) return null;
    return {
      id: `${selectedVehicle.id}:wheelspin`,
      display: "Wheel Spin",
      relpath: "",
      bone_count: 4,
      clips: [{ name: "Wheel Spin", frames: 60, rate: 30, duration: 2, tracks: 4 }],
    };
  }, [selectedVehicle]);

  const animationMeshRel = activeTab === "characters"
    ? (selectedCharacter?.relpath ?? `${animMesh}/${animMesh}/SkeletalMesh3/${animMesh}.psk`)
    : `${animMesh}/${animMesh}/SkeletalMesh3/${animMesh}.psk`;
  const animationUrl = selectedAnimset && selectedClip
    ? staticUrl(`/api/animation.glb?mesh=${encodeURIComponent(animationMeshRel)}&animset=${encodeURIComponent(selectedAnimset.relpath)}&clip=${encodeURIComponent(selectedClip.name)}`)
    : null;
  const previewWeaponUrl = previewWeapon ? meshUrl(previewWeapon.primary, null) : null;

  const inventoryPreviewUrl = selectedAsset?.preview_path
    ? (selectedAsset.preview_kind === "weapon_mesh"
        ? meshUrl(selectedAsset.preview_path, null)
        : selectedAsset.preview_kind === "vehicle_mesh"
          ? staticUrl(`/api/vehicle.glb?path=${encodeURIComponent(selectedAsset.preview_path)}`)
          : selectedAsset.preview_kind === "character_mesh"
            ? staticUrl(`/api/clothing/mesh.glb?item=${encodeURIComponent(selectedAsset.preview_path)}`)
            : selectedAsset.preview_kind === "static_mesh"
              ? staticUrl(`/api/static_mesh.glb?path=${encodeURIComponent(selectedAsset.preview_path)}`)
              : selectedAsset.preview_kind === "prop_animation"
                ? staticUrl(`/api/prop_animation.glb?path=${encodeURIComponent(selectedAsset.preview_path)}`)
                : selectedAsset.preview_kind === "texture"
              ? staticUrl(`/api/texture.png?path=${encodeURIComponent(selectedAsset.preview_path)}`)
              : selectedAsset.preview_kind === "video"
                ? staticUrl(`/api/media?path=${encodeURIComponent(selectedAsset.preview_path)}`)
                : null)
    : null;
  const flatPreviewKind = activeTab === "inventory"
    ? (selectedAsset?.preview_kind === "texture" || selectedAsset?.preview_kind === "video"
        ? selectedAsset.preview_kind
        : null)
    : null;
  const currentUrl = activeTab === "inventory" ? inventoryPreviewUrl
    : activeTab === "weapons" ? weaponMeshUrl
    : activeTab === "vehicles" ? (vehicleAnim ? vehicleAnimUrl : vehicleMeshUrl)
    : activeTab === "clothing" ? clothingMeshUrl
    : activeTab === "symbols" ? symbolTextureUrl
    : activeTab === "animations" ? animationUrl
    : activeTab === "characters" ? animationUrl
    : null;
  const currentName = activeTab === "inventory" ? (selectedAsset?.name ?? "")
    : activeTab === "weapons" ? (selectedWeapon?.display ?? "")
    : activeTab === "vehicles" ? (vehicleAnim && selectedVehicle ? `${selectedVehicle.display} · Wheel Spin` : (selectedVehicle?.display ?? ""))
    : activeTab === "clothing" ? clothingName
    : activeTab === "symbols" ? symbolName
    : activeTab === "animations" ? (selectedAnimset ? `${selectedAnimset.display} · ${selectedClip?.name ?? ""}` : "")
    : activeTab === "characters" ? (selectedCharacter ? `${selectedCharacter.name} · ${selectedClip?.name ?? ""}` : "")
    : "";
  const isSymbolTab = activeTab === "symbols";
  const isAssembledVehicle = activeTab === "vehicles" && !vehicleAnim && !!selectedVehicle && !!selectedWheelVariant;
  const assembledPartUrl = selectedWheelVariant
    ? staticUrl(`/api/vehicle_part.glb?path=${encodeURIComponent(selectedWheelVariant)}`)
    : null;
  const assembledSocketsUrl = selectedVehicle
    ? staticUrl(`/api/vehicle.sockets?path=${encodeURIComponent(retailVehiclePath(selectedVehicle.primary))}`)
    : null;
  const currentAssetKey = [currentUrl, assembledPartUrl, assembledSocketsUrl].filter(Boolean).join("|");

  // Switching weapons/parts is a viewport interaction: pause auto-rotate so the
  // freshly framed front view is actually visible. Also re-sync the orbit target
  // to the origin the framing effect assumes so the controls adopt the reset
  // instead of fighting it (remount already clears the target; this covers a
  // stale panned target within the same instance).
  useEffect(() => {
    if (!currentAssetKey) return;
    pauseAutoRotate();
    const timer = window.setTimeout(() => {
      const controls = controlsRef.current;
      if (controls) {
        controls.target.set(0, 0, 0);
        controls.update();
      }
    }, 0);
    return () => window.clearTimeout(timer);
  }, [currentAssetKey, pauseAutoRotate]);

  // Once the new asset is actually framed, let idle rotation resume after the
  // idle window, so the fresh front view stays visible for its full 2.5s.
  useEffect(() => {
    if (status !== "ready") return;
    scheduleAutoRotateResume();
  }, [status, scheduleAutoRotateResume]);

  const handleAssetError = useCallback((message: string) => {
    setError(message);
    setStatus("error");
  }, []);

  // The mixer reports every frame; writing 60fps state updates would re-render
  // the whole app. Throttle to ~20fps so the scrub slider stays live without
  // paying full-frame cost.
  const lastAnimTimeRef = useRef(0);
  const handleAnimTime = useCallback((time: number) => {
    const now = performance.now();
    if (now - lastAnimTimeRef.current < 50) return;
    lastAnimTimeRef.current = now;
    setAnimTime(time);
  }, []);

  const handleAnimDuration = useCallback((duration: number) => {
    setAnimDuration(duration);
    setAnimTime(0);
    setAnimSeek(0);
    lastAnimTimeRef.current = 0;
  }, []);
  const handleSceneStats = useCallback((tris: number) => {
    setTriangles(tris);
    if (tris > 0) {
      setStatus("ready");
      return;
    }
    setStatus("error");
    setError("Preview loaded without renderable geometry");
  }, []);

  useLayoutEffect(() => {
    // Flat (non-GLB) previews have no geometry to report; mark them ready so
    // the HUD and the model branch don't sit on "loading" forever.
    if (flatPreviewKind) {
      setStatus("ready");
      setError(null);
      return;
    }
    if (currentUrl) {
      setStatus("loading");
      setError(null);
      setTriangles(0);
      setAssembledWheelCount(0);
    }
  }, [currentAssetKey, currentUrl, flatPreviewKind]);

  return (
    <div className="app">
      <div className="tab-bar">
        {(["inventory","weapons","vehicles","clothing","symbols","animations","characters"] as Tab[]).map(t => (
          <button key={t} className={`tab ${activeTab===t?"active":""}`} onClick={() => setActiveTab(t)}>
            {t === "inventory" ? "All assets" : t.charAt(0).toUpperCase() + t.slice(1)}
          </button>
        ))}
      </div>
      <div className="sidebar">
        {activeTab === "inventory" && (
          <>
            <div className="inventory-head">
              <div>
                <span className="eyebrow">Project atlas</span>
                <h1>All assets <span>{assetMatchCount.toLocaleString()} / {assetTotal.toLocaleString()}</span></h1>
              </div>
              <span className="inventory-live">LIVE</span>
            </div>
            <input
              className="inventory-search"
              value={assetQuery}
              onChange={(event) => setAssetQuery(event.target.value)}
              placeholder="Search names, paths, provenance"
              aria-label="Search all assets"
            />
            <div className="inventory-filters">
              <select value={assetCategory} onChange={(event) => setAssetCategory(event.target.value)} aria-label="Filter by category">
                <option value="">Every lane</option>
                {Object.entries(assetCategories).map(([category, count]) => <option key={category} value={category}>{category} · {count}</option>)}
              </select>
              <select value={assetStatus} onChange={(event) => setAssetStatus(event.target.value)} aria-label="Filter by status">
                <option value="">Every status</option>
                {Object.entries(assetStatuses).map(([status, count]) => <option key={status} value={status}>{status} · {count}</option>)}
              </select>
              <select value={assetSourceBuild} onChange={(event) => setAssetSourceBuild(event.target.value)} aria-label="Filter by source build">
                <option value="">All source builds</option>
                {assetSourceBuilds.map((build) => <option key={build} value={build}>{build}</option>)}
              </select>
            </div>
            <div className="inventory-list">
              {assets.map((asset) => (
                <button key={asset.id} className={`asset-row ${selectedAsset?.id === asset.id ? "active" : ""}`} onClick={() => setSelectedAsset(asset)}>
                  <span className={`asset-dot ${asset.status}`} />
                  <span className="asset-row-main"><strong>{asset.name}</strong><small>{asset.destination || asset.source_locator || "No destination"}</small></span>
                  <span className="asset-row-meta"><b>{asset.category}</b><small>{asset.status}</small></span>
                </button>
              ))}
              {assets.length === 0 && <div className="inventory-empty">No assets match this filter.</div>}
              {assetHasMore && <button className="inventory-more" onClick={() => setAssetOffset((offset) => offset + 250)}>Load more assets</button>}
            </div>
          </>
        )}
        {activeTab === "weapons" && (
          <>
            {selectedWeapon && (
              <div className="viewer-controls">
                <div className="viewer-controls-title">{selectedWeapon.display}</div>
                {selectedWeapon.parts.length > 1 && (
                  <label className="viewer-control">
                    <span>Variant</span>
                    <select value={selectedPart ?? undefined} onChange={(e) => setSelectedPart(e.target.value)}>
                      {selectedWeapon.parts.map((part) => (
                        <option key={part.id} value={part.id}>{part.label}</option>
                      ))}
                    </select>
                  </label>
                )}
                {selectedWeapon.skins.length > 0 && (
                  <label className="viewer-control">
                    <span>Skin</span>
                    <select value={selectedSkin ?? ""} onChange={(e) => setSelectedSkin(e.target.value || null)}>
                      <option value="">Base</option>
                      {selectedWeapon.skins.map((skin) => (
                        <option key={skin.id} value={skin.id}>{skin.label}</option>
                      ))}
                    </select>
                  </label>
                )}
              </div>
            )}
            <h1>Weapons ({weapons.length})</h1>
            {weapons.map((w) => (                <button key={w.id} data-primary={w.primary} className={`weapon ${selectedWeapon?.id===w.id?"active":""}`} onClick={() => selectWeapon(w)}>

                <span>{w.display}</span>
                <span className={`conf ${w.name_confidence}`}>{w.name_confidence}</span>
              </button>
            ))}
          </>
        )}
        {activeTab === "vehicles" && (
          <>
            <h1>Vehicles ({vehicles.length})</h1>
            <label className="anim-check">
              <input
                type="checkbox"
                checked={vehicleAnim}
                onChange={(e) => {
                  setVehicleAnim(e.target.checked);
                  setSelectedWheelVariant(null);
                  if (e.target.checked) {
                    setAnimPlaying(true);
                    setAnimSeek(0);
                    setAnimTime(0);
                  }
                }}
              />
              <span>Animate (wheel spin)</span>
            </label>
            {selectedVehicle && vehicleAnim && vehicleAnimset && (
              <AnimTransportPanel
                animset={vehicleAnimset}
                clip={vehicleAnimset.clips[0] ?? null}
                playing={animPlaying}
                speed={animSpeed}
                loop={animLoop}
                duration={animDuration}
                time={animTime}
                title={`${selectedVehicle.display} · wheel spin`}
                onSelectClip={(clip) => {
                  setSelectedClip(clip);
                  setAnimSeek(0);
                  setAnimTime(0);
                  setAnimPlaying(true);
                }}
                onTogglePlay={() => setAnimPlaying((playing) => !playing)}
                onSetLoop={setAnimLoop}
                onSetSpeed={setAnimSpeed}
                onSeek={(value) => { setAnimSeek(value); setAnimTime(value); }}
              />
            )}
            {selectedVehicle && !vehicleAnim && selectedVehicle.parts.length > 1 && (
              <label className="viewer-control">
                <span>Part</span>
                <select
                  value={selectedVehicle.primary}
                  onChange={(event) => setSelectedVehicle({
                    ...selectedVehicle,
                    primary: event.target.value,
                  })}
                >
                  {selectedVehicle.parts.map((part) => (
                    <option key={part.id} value={part.id}>{part.label}</option>
                  ))}
                </select>
              </label>
            )}
            {!vehicleAnim && wheelPartsForVehicle.length > 0 && (
              <label className="viewer-control">
                <span>Wheel</span>
                <select
                  value={selectedWheelVariant ?? ""}
                  onChange={(event) => setSelectedWheelVariant(event.target.value || null)}
                >
                  <option value="">Stock</option>
                  {wheelPartsForVehicle.map((variant) => (
                    <option key={variant.id} value={variant.mesh}>{partLabel(variant.id)}</option>
                  ))}
                </select>
              </label>
            )}
            {vehicles.map((vehicle) => (
              <button
                key={vehicle.id}
                className={`weapon ${selectedVehicle?.id === vehicle.id ? "active" : ""}`}
                onClick={() => selectVehicle(vehicle)}
              >
                <span>{vehicle.display}</span>
                <span className="conf catalog">{vehicle.parts.length} parts</span>
              </button>
            ))}
          </>
        )}
        {activeTab === "clothing" && (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '1rem', height: '100%', overflowY: 'auto' }}>
            <ClothingSidebar
              onMeshChange={handleClothingMeshChange}
              onColorChange={handleClothingColorChange}
            />
            <hr style={{ borderColor: '#333' }} />
            <SymbolSidebar
              onSymbolChange={(symbol) => {
                setSelectedSymbolForDecal(symbol);
              }}
            />
            {decals.length > 0 && (
              <div className="decal-manager">
                <div className="decal-manager-head">
                  <h3>Placed decals ({decals.length})</h3>
                  <button className="decal-clear-all" onClick={() => setDecals([])}>Clear all</button>
                </div>
                {decals.map((decal, index) => (
                  <div key={decal.id} className="decal-row">
                    <img
                      className="decal-row-thumb"
                      src={staticUrl(`/api/symbol/texture?path=${encodeURIComponent(decal.path)}`)}
                      alt=""
                    />
                    <div className="decal-row-controls">
                      <label>
                        <span>Scale</span>
                        <input
                          type="range"
                          min={0.1}
                          max={3}
                          step={0.05}
                          value={decal.scale}
                          onChange={(event) => updateDecal(index, { scale: parseFloat(event.target.value) })}
                        />
                        <b>{decal.scale.toFixed(2)}×</b>
                      </label>
                      <label>
                        <span>Rotate</span>
                        <input
                          type="range"
                          min={-180}
                          max={180}
                          step={5}
                          value={decal.rotation}
                          onChange={(event) => updateDecal(index, { rotation: parseFloat(event.target.value) })}
                        />
                        <b>{Math.round(decal.rotation)}°</b>
                      </label>
                    </div>
                    <div className="decal-row-pos">
                      {Math.round(decal.u * 100)}% · {Math.round(decal.v * 100)}%
                    </div>
                    <button
                      className="decal-row-remove"
                      onClick={() => removeDecal(index)}
                      title="Remove decal"
                    >
                      ✕
                    </button>
                  </div>
                ))}
              </div>
            )}
          </div>
        )}
        {activeTab === "symbols" && (
          <SymbolSidebar onSymbolChange={(symbol) => {
            if (symbol) {
              setSymbolTextureUrl(staticUrl(`/api/symbol/texture?path=${encodeURIComponent(symbol.path)}`));
              setSymbolName(symbol.name);
            }
          }} />
        )}
        {activeTab === "animations" && (
          <>
            {selectedAnimset && (
              <AnimTransportPanel
                animset={selectedAnimset}
                clip={selectedClip}
                playing={animPlaying}
                speed={animSpeed}
                loop={animLoop}
                duration={animDuration}
                time={animTime}
                onSelectClip={selectAnimClip}
                onTogglePlay={() => setAnimPlaying((playing) => !playing)}
                onSetLoop={setAnimLoop}
                onSetSpeed={setAnimSpeed}
                onSeek={(value) => { setAnimSeek(value); setAnimTime(value); }}
                extra={
                  <>
                    <label className="viewer-control">
                      <span>Body mesh</span>
                      <select value={animMesh} onChange={(e) => setAnimMesh(e.target.value)}>
                        <option value="F_Body_Base">Female body</option>
                        <option value="M_Body_Base">Male body</option>
                      </select>
                    </label>
                    <label className="viewer-control">
                      <span>Weapon</span>
                      <select value={previewWeapon?.id ?? ""} onChange={(e) => {
                        setPreviewWeapon(weapons.find((w) => w.id === e.target.value) ?? null);
                      }}>
                        <option value="">No weapon</option>
                        {weapons.map((w) => <option key={w.id} value={w.id}>{w.display}</option>)}
                      </select>
                    </label>
                  </>
                }
              />
            )}
            <h1>Animations ({animsets.length} animsets)</h1>
            {animsets.map((animset) => (
              <button
                key={animset.relpath}
                data-animset={animset.relpath}
                className={`weapon ${selectedAnimset?.relpath === animset.relpath ? "active" : ""}`}
                onClick={() => selectAnimset(animset)}
              >
                <span>{animset.display}</span>
                <span className="conf catalog">{animset.clips.length} clips</span>
              </button>
            ))}
          </>
        )}
        {activeTab === "characters" && (
          <>
            <div className="inventory-head">
              <div>
                <span className="eyebrow">Character library</span>
                <h1>Characters <span>{characters.length.toLocaleString()} meshes</span></h1>
              </div>
            </div>
            <input
              className="inventory-search"
              value={charQuery}
              onChange={(event) => setCharQuery(event.target.value)}
              placeholder="Search bodies, clothing, NPCs"
              aria-label="Search characters"
            />
            <div className="char-filters">
              {(["all", "body", "clothing", "crowd", "character"] as const).map((category) => {
                const count = category === "all"
                  ? characters.length
                  : characters.filter((character) => character.category === category).length;
                return (
                  <button
                    key={category}
                    className={`char-filter ${charCategory === category ? "active" : ""}`}
                    onClick={() => setCharCategory(category)}
                  >
                    {category === "all" ? "All" : category.charAt(0).toUpperCase() + category.slice(1)}
                    <b>{count}</b>
                  </button>
                );
              })}
            </div>
            {selectedAnimset && (
              <AnimTransportPanel
                animset={selectedAnimset}
                clip={selectedClip}
                playing={animPlaying}
                speed={animSpeed}
                loop={animLoop}
                duration={animDuration}
                time={animTime}
                onSelectClip={selectAnimClip}
                onTogglePlay={() => setAnimPlaying((playing) => !playing)}
                onSetLoop={setAnimLoop}
                onSetSpeed={setAnimSpeed}
                onSeek={(value) => { setAnimSeek(value); setAnimTime(value); }}
                extra={
                  <label className="viewer-control">
                    <span>Weapon</span>
                    <select value={previewWeapon?.id ?? ""} onChange={(e) => {
                      setPreviewWeapon(weapons.find((w) => w.id === e.target.value) ?? null);
                    }}>
                      <option value="">No weapon</option>
                      {weapons.map((w) => <option key={w.id} value={w.id}>{w.display}</option>)}
                    </select>
                  </label>
                }
              />
            )}
            {characterGroups.map((group) => (
              <div key={group.label} className="char-group">
                <div className="char-group-head">{group.label} · {group.items.length}</div>
                {group.items.map((character) => (
                  <button
                    key={character.id}
                    data-character={character.relpath}
                    className={`weapon ${selectedCharacter?.id === character.id ? "active" : ""}`}
                    onClick={() => selectCharacter(character)}
                  >
                    <span>{character.name}</span>
                    <span className="conf catalog">{(character.bytes / 1024).toFixed(0)} KB</span>
                  </button>
                ))}
              </div>
            ))}
            {characterGroups.length === 0 && (
              <div className="inventory-empty">No character meshes match this filter.</div>
            )}
          </>
        )}
      </div>
      <div className="stage" data-status={status} data-triangles={triangles} data-assembled-wheels={assembledWheelCount} data-auto-rotate={autoRotate ? "1" : "0"} data-armed={armed ? "1" : "0"} data-decals={decals.length} data-anim-playing={animPlaying ? "1" : "0"} data-anim-loop={animLoop ? "1" : "0"} data-anim-speed={animSpeed} data-anim-duration={animDuration.toFixed(2)} data-anim-time={animTime.toFixed(2)} data-weapon={previewWeapon?.display ?? ""} data-vehicle-anim={vehicleAnim ? "1" : "0"} data-camera="">
        {armed && (
          <div className="decal-arm-banner">
            <span className="decal-arm-icon">◎</span>
            <span>
              Placing <strong>{selectedSymbolForDecal.name}</strong> — click the model to stamp. Press Esc or Stop to finish.
            </span>
            <button className="decal-arm-stop" onClick={() => setSelectedSymbolForDecal(null)}>Stop</button>
          </div>
        )}
        {activeTab === "inventory" && selectedAsset ? (
          <div className="asset-detail">
            <span className="eyebrow">{selectedAsset.category} / {selectedAsset.status}</span>
            <h2>{selectedAsset.name}</h2>
            <div className="asset-detail-grid">
              <span>Source build<b>{selectedAsset.source_build}</b></span>
              <span>Class<b>{selectedAsset.asset_class || "Not recorded"}</b></span>
              <span>Consumer<b>{selectedAsset.consumer_domain || "Not assigned"}</b></span>
              <span>Provenance<b>{selectedAsset.provenance}</b></span>
            </div>
            <code>{selectedAsset.destination || "Destination not recorded"}</code>
            {selectedAsset.source_locator && <code>{selectedAsset.source_locator}</code>}
            {selectedAsset.source_sha256 && <code>sha256:{selectedAsset.source_sha256}</code>}
            {selectedAsset.source_package && <code>{selectedAsset.source_package}{selectedAsset.source_object ? ` · ${selectedAsset.source_object}` : ""}</code>}
            <span className={`preview-badge ${selectedAsset.preview_kind === "none" ? "unavailable" : "available"}`}>
              {previewLabel(selectedAsset.preview_kind)}
            </span>
          </div>
        ) : null}
        {activeTab === "inventory" && selectedAsset?.preview_kind === "none" && (
          <div className="preview-empty">
            <span className="preview-empty-mark">◌</span>
            <strong>Preview unavailable</strong>
            <span>This record points to a compiled UE5 asset or metadata only.</span>
          </div>
        )}
        <div className="hud">
          {currentName ? (
            <>
              <div>{currentName}</div>
              <div>{flatPreviewKind ? (flatPreviewKind === "texture" ? "texture preview" : "video preview") : `${status} · ${triangles.toLocaleString()} tris`}</div>
              {error && <div className="err">{error}</div>}
            </>
          ) : (
            activeTab === "inventory"
              ? (selectedAsset?.preview_kind === "none" ? "This asset is indexed, but not previewable by the studio yet" : "Select an asset to inspect")
              : `No ${activeTab === "weapons" ? "weapon" : activeTab === "vehicles" ? "vehicle" : activeTab === "clothing" ? "clothing item" : activeTab === "symbols" ? "symbol" : activeTab === "characters" ? "character item" : "animation clip"} selected`
          )}
        </div>
        {currentUrl && flatPreviewKind === "texture" ? (
          <div className="flat-preview">
            <ZoomableTexture key={currentUrl} src={currentUrl} alt={currentName} />
          </div>
        ) : currentUrl && flatPreviewKind === "video" ? (
          <div className="flat-preview">
            <video className="flat-preview-media" src={currentUrl} controls autoPlay loop muted playsInline />
          </div>
        ) : (
        <Canvas camera={{ position: [0, 0, 3], fov: 45 }} dpr={[1, 2]} style={{ cursor: armed ? "crosshair" : undefined }}>
          <CameraProbe />
          <ambientLight intensity={1.1} />
          <directionalLight position={[5, 8, 5]} intensity={3.6} />
          <directionalLight position={[-5, -3, -5]} intensity={1.4} />
          {activeTab === "clothing" && (
            <DecalDropTarget
              rootRef={modelRootRef}
              onDropDecal={(symbol, uv, localPoint) => placeDecal(symbol.path, uv.x, uv.y, localPoint)}
            />
          )}
          {armed && selectedSymbolForDecal && (
            <ClickPicker
              rootRef={modelRootRef}
              active={armed}
              onPick={(uv, localPoint) => placeDecal(selectedSymbolForDecal.path, uv.x, uv.y, localPoint)}
            />
          )}
          {currentUrl && (
            <ModelErrorBoundary key={currentAssetKey} url={currentAssetKey} onError={handleAssetError}>
              <Suspense fallback={null}>
                {isSymbolTab ? (
                  <SymbolPlane textureUrl={currentUrl} onStats={handleSceneStats} />
                ) : activeTab === "vehicles" && vehicleAnim ? (
                  <AnimatedModel
                    url={currentUrl}
                    onStats={handleSceneStats}
                    onDuration={handleAnimDuration}
                    onTime={handleAnimTime}
                    playing={animPlaying}
                    speed={animSpeed}
                    loop={animLoop}
                    seek={animSeek}
                  />
                ) : isAssembledVehicle ? (
                  <Bounds fit observe margin={1.2}>
                    <Center>
                      <AssembledVehicle
                        baseUrl={currentUrl}
                        partUrl={assembledPartUrl!}
                        socketsUrl={assembledSocketsUrl!}
                        onError={handleAssetError}
                        onWheelCount={setAssembledWheelCount}
                        onStats={handleSceneStats}
                      />
                    </Center>
                  </Bounds>
                ) : activeTab === "animations" || activeTab === "characters" ? (
                  <AnimatedModel
                    url={currentUrl}
                    onStats={handleSceneStats}
                    onDuration={handleAnimDuration}
                    onTime={handleAnimTime}
                    playing={animPlaying}
                    speed={animSpeed}
                    loop={animLoop}
                    seek={animSeek}
                    weaponUrl={previewWeaponUrl}
                  />
                ) : activeTab === "inventory" && selectedAsset?.preview_kind === "prop_animation" ? (
                  <AnimatedModel
                    url={currentUrl}
                    onStats={handleSceneStats}
                    onDuration={handleAnimDuration}
                    onTime={handleAnimTime}
                    playing
                    speed={1}
                    loop
                    seek={animSeek}
                  />
                ) : (
                  <Model
                    url={currentUrl}
                    baseColorUrl={activeTab === "clothing" ? composedBaseColorUrl : null}
                    unlitPreview={activeTab === "weapons" || activeTab === "clothing"}
                    baseColorMaterialName={activeTab === "clothing"
                      ? (clothingName.toLowerCase().includes("armpad") ? "__clothing_layer" : "material_0")
                      : null}
                    onStats={handleSceneStats}
                    marker={pickMarker?.pos ?? null}
                    onRoot={handleModelRoot}
                  />
                )}
              </Suspense>
            </ModelErrorBoundary>
          )}
          <OrbitControls
            ref={controlsRef}
            key={`oc|${currentAssetKey}`}
            makeDefault
            autoRotate={autoRotate && (activeTab === "weapons" || activeTab === "characters")}
            autoRotateSpeed={1.4}
            onStart={pauseAutoRotate}
            onEnd={scheduleAutoRotateResume}
          />
        </Canvas>
        )}
      </div>
    </div>
  );
}
