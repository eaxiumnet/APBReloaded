import React, { Suspense, useEffect, useMemo, useState } from "react";
import { Canvas } from "@react-three/fiber";
import { Bounds, Center, OrbitControls, useGLTF, useTexture as dreiUseTexture } from "@react-three/drei";
import * as THREE from "three";

import { ClothingSidebar } from "./ColMaskPanel";
import { SymbolSidebar } from "./SymbolBrowser";

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

type Tab = "weapons" | "clothing" | "symbols";

function meshUrl(path: string, skin: string | null): string {
  const skinParam = skin ? `&skin=${encodeURIComponent(skin)}` : "";
  return `/api/mesh.glb?path=${encodeURIComponent(path)}${skinParam}`;
}

function countTriangles(object: THREE.Object3D): number {
  let tris = 0;
  object.traverse((child) => {
    const mesh = child as THREE.Mesh;
    if (mesh.isMesh && mesh.geometry) {
      const geo = mesh.geometry;
      if (geo.index) tris += geo.index.count / 3;
      else if (geo.attributes.position) tris += geo.attributes.position.count / 3;
    }
  });
  return Math.round(tris);
}

function Model({ url, onStats }: { url: string; onStats: (tris: number) => void }) {
  const gltf = useGLTF(url);
  useEffect(() => { onStats(countTriangles(gltf.scene)); }, [gltf, onStats]);
  return <primitive object={gltf.scene} />;
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

class ModelErrorBoundary extends React.Component<
  { url: string; onError: (msg: string) => void; children: React.ReactNode },
  { failedUrl: string | null }
> {
  state = { failedUrl: null as string | null };
  static getDerivedStateFromError() { return {}; }
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
    if (this.state.failedUrl === this.props.url) return null;
    return this.props.children;
  }
}

export function App() {
  const [activeTab, setActiveTab] = useState<Tab>("weapons");
  const [weapons, setWeapons] = useState<Weapon[]>([]);
  const [selectedWeapon, setSelectedWeapon] = useState<Weapon | null>(null);
  const [selectedPart, setSelectedPart] = useState<string | null>(null);
  const [selectedSkin, setSelectedSkin] = useState<string | null>(null);
  const [clothingMeshUrl, setClothingMeshUrl] = useState<string | null>(null);
  const [clothingName, setClothingName] = useState("");
  const [symbolTextureUrl, setSymbolTextureUrl] = useState<string | null>(null);
  const [symbolName, setSymbolName] = useState("");
  const [status, setStatus] = useState<"idle" | "loading" | "ready" | "error">("idle");
  const [triangles, setTriangles] = useState(0);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    fetch("/api/catalog/weapons")
      .then((r) => r.json())
      .then((data) => {
        setWeapons(data.weapons ?? []);
        if (data.weapons?.length) setSelectedWeapon(data.weapons[0]);
      })
      .catch((e) => setError(String(e)));
  }, []);

  useEffect(() => {
    setSelectedPart(selectedWeapon?.primary ?? null);
    setSelectedSkin(null);
  }, [selectedWeapon]);

  const weaponMeshUrl = useMemo(
    () => (selectedPart ? meshUrl(selectedPart, selectedSkin) : null),
    [selectedPart, selectedSkin],
  );

  const currentUrl = activeTab === "weapons" ? weaponMeshUrl
    : activeTab === "clothing" ? clothingMeshUrl
    : symbolTextureUrl;
  const currentName = activeTab === "weapons" ? (selectedWeapon?.display ?? "")
    : activeTab === "clothing" ? clothingName
    : symbolName;
  const isSymbolTab = activeTab === "symbols";

  useEffect(() => {
    if (currentUrl) {
      setStatus("loading");
      setError(null);
      setTriangles(0);
    }
  }, [currentUrl]);

  return (
    <div className="app">
      <div className="tab-bar">
        {(["weapons","clothing","symbols"] as Tab[]).map(t => (
          <button key={t} className={`tab ${activeTab===t?"active":""}`} onClick={() => setActiveTab(t)}>
            {t.charAt(0).toUpperCase() + t.slice(1)}
          </button>
        ))}
      </div>
      <div className="sidebar">
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
            {weapons.map((w) => (
              <button key={w.id} className={`weapon ${selectedWeapon?.id===w.id?"active":""}`} onClick={() => setSelectedWeapon(w)}>
                <span>{w.display}</span>
                <span className={`conf ${w.name_confidence}`}>{w.name_confidence}</span>
              </button>
            ))}
          </>
        )}
        {activeTab === "clothing" && (
          <ClothingSidebar onMeshChange={(url, name) => { setClothingMeshUrl(url); setClothingName(name); }} />
        )}
        {activeTab === "symbols" && (
          <SymbolSidebar onSymbolChange={(url, name) => { setSymbolTextureUrl(url); setSymbolName(name); }} />
        )}
      </div>
      <div className="stage" data-status={status} data-triangles={triangles}>
        <div className="hud">
          {currentName ? (
            <>
              <div>{currentName}</div>
              <div>{status} · {triangles.toLocaleString()} tris</div>
              {error && <div className="err">{error}</div>}
            </>
          ) : (
            `No ${activeTab === "weapons" ? "weapon" : activeTab === "clothing" ? "clothing item" : "symbol"} selected`
          )}
        </div>
        <Canvas camera={{ position: [0, 0, 3], fov: 45 }} dpr={[1, 2]}>
          <ambientLight intensity={0.6} />
          <directionalLight position={[5, 8, 5]} intensity={1.2} />
          <directionalLight position={[-5, -3, -5]} intensity={0.4} />
          {currentUrl && (
            <ModelErrorBoundary url={currentUrl} onError={(msg) => { setError(msg); setStatus("error"); }}>
              <Suspense fallback={null}>
                <Bounds fit clip observe margin={1.2}>
                  <Center>
                    {isSymbolTab ? (
                      <SymbolPlane textureUrl={currentUrl} onStats={(tris) => { setTriangles(tris); setStatus("ready"); }} />
                    ) : (
                      <Model url={currentUrl} onStats={(tris) => { setTriangles(tris); setStatus("ready"); }} />
                    )}
                  </Center>
                </Bounds>
              </Suspense>
            </ModelErrorBoundary>
          )}
          <OrbitControls makeDefault />
        </Canvas>
      </div>
    </div>
  );
}
