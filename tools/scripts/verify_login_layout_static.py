#!/usr/bin/env python3
"""Structural audit of shipped frontend login (no UE required)."""
from pathlib import Path
import re
import sys

ROOT = Path(r"D:\APBReloaded")
CPP = ROOT / "Source/APBReloaded/Systems/APBFrontendWidget.cpp"
H = ROOT / "Source/APBReloaded/Systems/APBFrontendWidget.h"
MATH = ROOT / "Source/APBReloaded/Systems/APBFrontendLayoutMath.h"
SCRATCH = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(
    r"C:\Users\Support\AppData\Local\Temp\grok-goal-4381756b8529\implementer"
)
SCRATCH.mkdir(parents=True, exist_ok=True)
out = []
fails = 0

def hit(label, cond, detail=""):
    global fails
    if cond:
        out.append(f"PASS: {label} {detail}")
    else:
        fails += 1
        out.append(f"FAIL: {label} {detail}")

cpp = CPP.read_text(encoding="utf-8", errors="replace")
h = H.read_text(encoding="utf-8", errors="replace")
math = MATH.read_text(encoding="utf-8", errors="replace")

# Login fixed non-scroll
hit("BeginStageContent(false) on Login",
    re.search(r"case EAPBFrontendStage::Login:[\s\S]{0,400}?BeginStageContent\(false\)", cpp) is not None)
hit("LoginAllowsScroll false in math", "LoginAllowsScroll" in math and "return false" in math)
hit("Login_APB_Logo.png load (no transparent)",
    'ImportUiTex(TEXT("Login_APB_Logo.png"))' in cpp
    and "Login_APB_Logo_transparent" not in cpp)
hit("LogoSizeFromPanelWidth used", "LogoSizeFromPanelWidth" in cpp)
hit("ScaledPanelSize used", "ScaledPanelSize" in cpp)
hit("Login shell logo above plate", "LogoSizeBox" in cpp and "PanelShell" in cpp)
hit("apb_layout math included", "APBFrontendLayoutMath.h" in cpp)

# Stage beds
for token in ["Character_Select_BG", "Faction_Criminal", "Faction_Enforcer", "Login_BG_AI_compat"]:
    hit(f"bed token {token}", token in cpp)

logo = ROOT / "Content/UI/Frontend/2011/Login_APB_Logo.png"
hit("logo file exists", logo.is_file(), str(logo))

lines = "\n".join(out) + f"\nFAILS={fails}\n"
(SCRATCH / "ui_structure.txt").write_text(lines, encoding="utf-8")
print(lines)
sys.exit(1 if fails else 0)
