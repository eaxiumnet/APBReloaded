import json
L=json.load(open(r"C:\Users\Support\AppData\Local\Temp\opencode\layout.json"))

def is_pct(s): return bool(s) and "Percentage" in s
def face(v): return None if v is None else float(v)

def resolve_fg(f):
    l=face(f.get("Left")); t=face(f.get("Top")); r=face(f.get("Right")); b=face(f.get("Bottom"))
    x = 0.0 if l is None else l
    y = 0.0 if t is None else t
    if r is None: w=0.0
    elif l is None: w=r
    else: w = (r-l) if r>=l else r
    if b is None: h=0.0
    elif t is None: h=b
    else: h = (b-t) if b>=t else b
    return round(x,2),round(y,2),round(w,2),round(h,2)

SCENES={"Login_Scene":("Login",1152,720),"Lobby_Scene":("Lobby",800,600)}
lines=[]
lines.append("#pragma once")
lines.append("// GENERATED from APBMenus_GameFlowScenes.upk UIScene docking data. Do not hand-edit.")
lines.append("// Regenerate: tools/gen_scene_data.py from Content/Extracted/2011/.../login_lobby_layout.json")
lines.append("namespace apb_scene {")
lines.append("struct FRectDef { const char* Name; float X, Y, W, H; };")
lines.append("struct FBgDef { const char* Name; float L, T, R, B; bool LPct, TPct, RPct, BPct; };")
for scene,(tag,DW,DH) in SCENES.items():
    rows=L[scene]
    fg=[r for r in rows if not any(is_pct(v) for v in r["scales"].values())]
    bg=[r for r in rows if any(is_pct(v) for v in r["scales"].values())]
    lines.append(f"// ---- {tag} (design {DW}x{DH}) ----")
    lines.append(f"static constexpr float {tag}DesignW = {DW}.f;")
    lines.append(f"static constexpr float {tag}DesignH = {DH}.f;")
    lines.append(f"static const FRectDef {tag}Rects[] = {{")
    for r in fg:
        x,y,w,h=resolve_fg(r["faces"])
        lines.append(f'    {{ "{r["name"]}", {x}f, {y}f, {w}f, {h}f }},')
    lines.append("};")
    lines.append(f"static constexpr int {tag}RectCount = sizeof({tag}Rects)/sizeof(FRectDef);")
    lines.append(f"static const FBgDef {tag}Bg[] = {{")
    for r in bg:
        f=r["faces"]; s=r["scales"]
        def v(k): return 0.0 if f.get(k) is None else round(float(f[k]),4)
        def p(k): return "true" if is_pct(s.get(k)) else "false"
        lines.append(f'    {{ "{r["name"]}", {v("Left")}f, {v("Top")}f, {v("Right")}f, {v("Bottom")}f, {p("Left")}, {p("Top")}, {p("Right")}, {p("Bottom")} }},')
    lines.append("};")
    lines.append(f"static constexpr int {tag}BgCount = sizeof({tag}Bg)/sizeof(FBgDef);")
lines.append("inline const FRectDef* FindRect(const FRectDef* Arr, int N, const char* Name) {")
lines.append("    for (int i=0;i<N;++i){ const char*a=Arr[i].Name;const char*b=Name;while(*a&&*a==*b){++a;++b;} if(*a==0&&*b==0) return &Arr[i]; }")
lines.append("    return nullptr;")
lines.append("}")
lines.append("}")
open(r"D:\APBReloaded\Source\APBReloaded\Systems\Frontend\APBFrontendSceneData.h","w",encoding="utf-8").write("\n".join(lines)+"\n")
print("wrote APBFrontendSceneData.h")
print("Login FG:",len([r for r in L['Login_Scene'] if not any(is_pct(v) for v in r['scales'].values())]),
      "BG:",len([r for r in L['Login_Scene'] if any(is_pct(v) for v in r['scales'].values())]))
print("Lobby FG:",len([r for r in L['Lobby_Scene'] if not any(is_pct(v) for v in r['scales'].values())]),
      "BG:",len([r for r in L['Lobby_Scene'] if any(is_pct(v) for v in r['scales'].values())]))
