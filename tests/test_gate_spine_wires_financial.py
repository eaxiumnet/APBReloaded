from pathlib import Path


SPINE = Path(r"D:\APBReloaded\tools\run_verification_gates.ps1")


def main() -> int:
    text = SPINE.read_text(encoding="utf-8")
    fails = 0

    if "test_financial_district_manifest_gate" not in text:
        print("FAIL missing test_financial_district_manifest_gate")
        fails += 1

    if "FINANCIAL_MANIFEST_OK" not in text:
        print("FAIL missing FINANCIAL_MANIFEST_OK")
        fails += 1

    print(f"FAILS={fails}")
    return 1 if fails else 0


if __name__ == "__main__":
    raise SystemExit(main())
