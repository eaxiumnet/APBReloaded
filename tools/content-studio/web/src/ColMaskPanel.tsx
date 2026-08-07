import { useEffect, useState } from "react";

import { apiFetch, staticUrl } from "./api";

type ClothingItem = {
  id: string;
  name: string;
  region_count: number;
  regions: string[];
};

type RegionData = {
  name: string;
  path: string;
  color: string;
};

type ClothingSidebarProps = {
  onMeshChange: (meshUrl: string | null, name: string) => void;
  onColorChange?: (itemId: string, regionName: string, color: string) => void;
};

export function ClothingSidebar({ onMeshChange, onColorChange }: ClothingSidebarProps) {
  const [items, setItems] = useState<ClothingItem[]>([]);
  const [selected, setSelected] = useState<ClothingItem | null>(null);
  const [regions, setRegions] = useState<RegionData[]>([]);
  const [loading, setLoading] = useState(true);
  const [loadingRegions, setLoadingRegions] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const controller = new AbortController();
    apiFetch("/api/catalog/clothing", { signal: controller.signal })
      .then((r) => {
        if (!r.ok) throw new Error(`clothing catalog failed: ${r.status}`);
        return r.json();
      })
      .then((data) => {
        setItems(data.items ?? []);
        setSelected((current) => current ?? data.items?.[0] ?? null);
        setLoading(false);
      })
      .catch((e) => {
        if (e.name === "AbortError") return;
        setError(String(e));
        setLoading(false);
      });
    return () => controller.abort();
  }, []);

  useEffect(() => {
    const selectedId = selected?.id;
    const selectedName = selected?.name;
    if (!selectedId || !selectedName) {
      setRegions([]);
      onMeshChange(null, "");
      return;
    }

    // Construct mesh URL: /api/clothing/mesh.glb?item=<name>
    const meshUrl = staticUrl(`/api/clothing/mesh.glb?item=${encodeURIComponent(selectedName)}`);
    onMeshChange(meshUrl, selectedName);

    setLoadingRegions(true);
    const controller = new AbortController();
    apiFetch(`/api/colmask?item=${encodeURIComponent(selectedId)}`, { signal: controller.signal })
      .then(async (r) => {
        if (!r.ok) throw new Error(await r.text());
        return r.json();
      })
      .then((data) => {
        const regionList: RegionData[] = Object.entries(data.regions || {}).map(
          ([name, path]) => ({
            name,
            path: path as string,
            color: "#808080",
          })
        );
        setRegions(regionList);
        setLoadingRegions(false);
      })
      .catch((e) => {
        if (e.name === "AbortError") return;
        console.error("Failed to load regions:", e);
        setRegions([]);
        setLoadingRegions(false);
      });
    return () => controller.abort();
  }, [selected?.id, selected?.name, onMeshChange]);

  const handleColorChange = (regionName: string, color: string) => {
    setRegions((prev) =>
      prev.map((r) => (r.name === regionName ? { ...r, color } : r))
    );
    if (selected && onColorChange) {
      onColorChange(selected.id, regionName, color);
    }
  };

  if (loading) return <div className="colmask-panel">Loading clothing items...</div>;
  if (error) return <div className="colmask-panel error">Error: {error}</div>;

  return (
    <div className="colmask-panel">
      <h2>Clothing ({items.length})</h2>
      <div className="colmask-list">
        {items.map((item) => (
          <button
            key={item.id}
            className={`colmask-item ${selected?.id === item.id ? "active" : ""}`}
            onClick={() => setSelected(item)}
          >
            <span>{item.name}</span>
            <span className="region-count">{item.region_count} regions</span>
          </button>
        ))}
      </div>

      {selected && (
        <div className="colmask-regions">
          <h3>{selected.name} Regions</h3>
          {loadingRegions ? (
            <div className="loading">Loading regions...</div>
          ) : (
            <div className="region-grid">
              {regions.map((region) => (
                <div key={region.name} className="region-card">
                  <div className="region-thumb">
                    <img
                      src={staticUrl(`/api/colmask/texture?path=${encodeURIComponent(region.path)}`)}
                      alt={region.name}
                      onError={(e) => {
                        (e.target as HTMLImageElement).style.display = "none";
                      }}
                    />
                  </div>
                  <div className="region-name">{region.name}</div>
                  <div className="region-color-picker">
                    <input
                      type="color"
                      value={region.color}
                      onChange={(e) => handleColorChange(region.name, e.target.value)}
                      className="color-input"
                    />
                    <span className="color-value">{region.color}</span>
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>
      )}
    </div>
  );
}
