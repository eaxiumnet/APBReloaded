import { useEffect, useState } from "react";

import { apiFetch, staticUrl } from "./api";

export type SymbolData = {
  id: string;
  name: string;
  path: string;
};

type SymbolCategory = {
  category: string;
  symbol_count: number;
  symbols: SymbolData[];
};

type SymbolSidebarProps = {
  onSymbolChange: (symbol: SymbolData | null) => void;
};

export function SymbolSidebar({ onSymbolChange }: SymbolSidebarProps) {
  const [categories, setCategories] = useState<SymbolCategory[]>([]);
  const [selectedCategory, setSelectedCategory] = useState<SymbolCategory | null>(null);
  const [selectedSymbol, setSelectedSymbol] = useState<SymbolData | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    apiFetch("/api/symbols/list")
      .then((r) => r.json())
      .then((data) => {
        setCategories(data.categories ?? []);
        if (data.categories?.length) setSelectedCategory(data.categories[0]);
        setLoading(false);
      })
      .catch((e) => {
        setError(String(e));
        setLoading(false);
      });
  }, []);

  const handleSymbolClick = (symbol: SymbolData) => {
    setSelectedSymbol(symbol);
    onSymbolChange(symbol);
  };

  if (loading) return <div className="symbol-browser">Loading symbols...</div>;
  if (error) return <div className="symbol-browser error">Error: {error}</div>;

  const totalSymbols = categories.reduce((sum, cat) => sum + cat.symbol_count, 0);

  return (
    <div className="symbol-browser">
      <h2>Symbols ({totalSymbols})</h2>
      <p className="symbol-hint">Click a symbol, then click the model to stamp it — or drag it straight onto the model.</p>

      <div className="symbol-categories">
        {categories.map((cat) => (
          <button
            key={cat.category}
            className={`symbol-category ${selectedCategory?.category === cat.category ? "active" : ""}`}
            onClick={() => setSelectedCategory(cat)}
          >
            <span>{cat.category}</span>
            <span className="symbol-count">{cat.symbol_count}</span>
          </button>
        ))}
      </div>

      {selectedCategory && (
        <div className="symbol-grid">
          <h3>{selectedCategory.category} ({selectedCategory.symbol_count})</h3>
          <div className="symbol-thumbnails">
            {selectedCategory.symbols.map((symbol) => (
              <button
                key={symbol.id}
                className={`symbol-thumb ${selectedSymbol?.id === symbol.id ? "active" : ""}`}
                onClick={() => handleSymbolClick(symbol)}
                onDragStart={(event) => {
                  event.dataTransfer.setData("application/x-apb-symbol", JSON.stringify(symbol));
                  event.dataTransfer.effectAllowed = "copy";
                }}
                draggable
                title={`${symbol.name} — click to arm, or drag onto the model`}
              >
                <img
                  src={staticUrl(`/api/symbol/texture?path=${encodeURIComponent(symbol.path)}`)}
                  alt={symbol.name}
                  loading="lazy"
                  onError={(e) => {
                    (e.target as HTMLImageElement).style.display = "none";
                  }}
                />
              </button>
            ))}
          </div>
        </div>
      )}
    </div>
  );
}
