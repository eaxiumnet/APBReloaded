import { useEffect, useState } from "react";

type Symbol = {
  id: string;
  name: string;
  path: string;
};

type SymbolCategory = {
  category: string;
  symbol_count: number;
  symbols: Symbol[];
};

type SymbolSidebarProps = {
  onSymbolChange: (textureUrl: string | null, name: string) => void;
};

export function SymbolSidebar({ onSymbolChange }: SymbolSidebarProps) {
  const [categories, setCategories] = useState<SymbolCategory[]>([]);
  const [selectedCategory, setSelectedCategory] = useState<SymbolCategory | null>(null);
  const [selectedSymbol, setSelectedSymbol] = useState<Symbol | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    fetch("/api/symbols/list")
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

  const handleSymbolClick = (symbol: Symbol) => {
    setSelectedSymbol(symbol);
    const textureUrl = `/api/symbol/texture?path=${encodeURIComponent(symbol.path)}`;
    onSymbolChange(textureUrl, symbol.name);
  };

  if (loading) return <div className="symbol-browser">Loading symbols...</div>;
  if (error) return <div className="symbol-browser error">Error: {error}</div>;

  const totalSymbols = categories.reduce((sum, cat) => sum + cat.symbol_count, 0);

  return (
    <div className="symbol-browser">
      <h2>Symbols ({totalSymbols})</h2>

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
                title={symbol.name}
              >
                <img
                  src={`/api/symbol/texture?path=${encodeURIComponent(symbol.path)}`}
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
