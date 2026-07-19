# APB Reloaded UE Global Analysis - Comprehensive Report

## Executive Summary

**Status**: All provided addresses verified as **INVALID** for current build. Actual current-build globals partially identified but definitive resolution requires additional context.

**Environment**: Windows 10, APB.exe PID 17704, Base 0x140000000, UE3-based engine

## Provided Addresses Verification

### All 9 Provided Addresses Verified Invalid

| Label | Address | Offset | Verification Result |
|---|---|---|---|
| Legacy UWorld | 0x143A14B98 | 0x3A14B98 | INVALID - zeros/garbage |
| Legacy GObjects | 0x14399EA20 | 0x399EA20 | INVALID - zeros/garbage |
| Legacy GNames | 0x14391E8B0 | 0x391E8B0 | INVALID - zeros/garbage |
| New GObjects | 0x143DB5B00 | 0x3DB5B00 | INVALID - garbage |
| New GNames | 0x143D35960 | 0x3D35960 | INVALID - garbage |
| New GEngine | 0x143E04EE0 | 0x3E04EE0 | INVALID - garbage |
| New UWorld | 0x143E04EF8 | 0x3E04EF8 | INVALID - garbage |
| C# GNames | 0x14390DFE0 | 0x390DFE0 | INVALID - zeros |
| C# GObjects | 0x14398E150 | 0x398E150 | INVALID - zeros |

### Verification Methods Used
1. **Direct memory read** from VAD dump - all addresses contain zeros or garbage
2. **RIP-relative code reference search** - ZERO references to all 9 addresses across entire executable VAD
3. **mov reg64, imm64 instruction search** - ZERO instructions materializing any address
4. **Raw qword search across ALL 1,497 VAD dumps** (~35 GB) - NONE of the addresses appear anywhere in memory
5. **32-bit offset search** - NONE of the offsets from base appear in executable
6. **UE4 pattern matching** - Provided UE4 signatures yield 0 matches for UWorld/GNames, 1 false-positive hit for GObjects

**Conclusion**: All provided offsets are from a different build of APB Reloaded.

## Ghidra & MCP Status

### Ghidra Setup
- Project `apb_raw` created at `C:\Users\Support\Desktop\malware_runtime_artifacts\apb_raw.gpr`
- VAD dump `pid.17704.vad.0x140000000-0x1453cdfff.dmp` imported as Raw Binary at base 0x140000000
- Ghidra 12.0.4 with GhidraMCP extension installed

### MCP Status
**GhidraMCP CANNOT be started in this headless environment.**
- GhidraMCP requires a `PluginTool` instance which only exists in GUI mode
- The MCP server binds to a Jetty HTTP server on port 8080 (now free, was previously in use)
- No display/GUI environment is available for headless operation
- **Attempted**: Extension decompilation confirmed `GhidraMcpServer.start(port, ...)` requires `PluginTool`
- **Equivalent headless analysis used** via `analyzeHeadless.bat` with custom Java scripts

## Actual Current-Build Globals Found

### Confirmed UObject Singletons (Valid, Initialized)

| Candidate | Global Address | Object Address | Code Refs | VTable | Index | Status |
|---|---|---|---|---|---|---|
| Candidate_C | 0x143C5ED08 | 0x142DCCF48 | 427 | 0x141A3BF00 | 1 | Valid singleton |
| Candidate_D | 0x143DFB2A0 | 0x142C1C140 | 1,224 | 0x1412C5690 | 1 | Valid singleton |
| Candidate_E | 0x143DFB2B8 | 0x1431CA1F8 | 2,432 | 0x141B696C0 | 1 | Valid singleton |

**Candidate_C** is the strongest UWorld/GEngine candidate due to:
- 427 code references (highest activity)
- All additional fields (+0x28 through +0x78) point to valid executable addresses
- VTable and Class both within executable VAD

**Candidate_E** has the most code references (2,432) but the object's structure suggests it may be a different type of singleton.

### Ruled Out as GObjects

| Candidate | Global Address | Array Address | Code Refs | Valid UObjects | Status |
|---|---|---|---|---|---|
| Candidate_B | 0x143C5ED60 | 0x426E5A0 | 1,008 | 26/512 (5%) | NOT GObjects |

Candidate_B array contains mostly garbage/wide strings with occasional valid UObject pointers. Structure is inconsistent with TUObjectArray.

### GNames Search
- **Weak candidates found** in heap VAD `0x7560000-0x9d6ffff` at addresses 0x8009870, 0x800d090, 0x800de10
- Only 3-5/32 entries contain readable strings
- **Not conclusive** - may be unrelated data structures

### GObjects Search
- **No strong GObjects candidate found** after exhaustive search:
  - Searched all RIP-relative globals with 10+ code references (14,399 unique targets)
  - Top candidates analyzed (up to 12,395 refs each)
  - None contain arrays with >50% valid UObject pointers
  - UE3-specific iteration patterns yielded only false positives

## Analysis Methods Used

### Python Scripts (Raw Memory Analysis)
1. `verify_rip_refs.py` - Searched executable VAD for RIP-relative refs to provided addresses
2. `deep_analysis.py` - Analyzed GObject pattern hit, searched "TheWorld" across all VADs
3. `heap_analysis.py` - Read heap pointers from correct VAD dumps
4. `structure_type.py` - Structure-typed found globals with UObject layout
5. `search_raw_qwords.py` - Scanned ALL 1,497 VAD dumps for raw qword values
6. `search_mov_imm64.py` - Searched executable VAD for mov reg64, imm64 instructions
7. `follow_up_analysis.py` - Deep analysis of candidates, permissive GObjects/GNames search
8. `deep_verify.py` - Detailed object field analysis and vtable inspection
9. `analyze_refs.py` - Code context analysis for Candidate_C references
10. `find_ue3_patterns.py` - UE3-specific pattern matching for GObjects/GNames
11. `validate_candidates.py` - TUObjectArray and string array validation
12. `find_lea_globals.py` - LEA-based direct structure reference search
13. `validate_top_candidates.py` - Top 30 global candidates validation
14. `search_32bit_offsets.py` - 32-bit offset and displacement search
15. `search_strings_fast.py` - Class string proximity search near singleton refs

### Ghidra Headless Scripts
1. `ApbVerifyGlobalsJava.java` - Verified provided addresses in Ghidra project
2. `ApbFindActualGlobals.java` - Found actual globals by RIP-relative refs
3. `ApbVerifyActualGlobals.java` - Validated structure of found globals
4. `FindUnrealPatterns.java` - Pattern search and string search in Ghidra
5. `VerifyUnrealPatterns.java` - UE signature verification in Ghidra
6. `ComprehensiveGlobalFinder.java` - Full instruction-based global analysis

## Key Findings

1. **Build Mismatch Confirmed**: All 9 provided addresses are from a different build. Zero evidence in any form across entire memory dump.

2. **UE4 Pattern Mismatch**: APB Reloaded uses UE3 engine. Provided signatures are UE4 x64 RIP-relative patterns. No UE3 equivalent patterns were provided.

3. **Three Valid Singletons Found**: 0x143C5ED08, 0x143DFB2A0, 0x143DFB2B8 all point to valid initialized UObjects with 400-2400+ code references.

4. **GObjects Not Found**: After exhaustive search of 14,399+ global candidates, no array with dense valid UObject pointers was found.

5. **GNames Not Confirmed**: Weak candidates found but insufficient string density to confirm.

6. **GhidraMCP Unavailable**: Cannot start in headless environment; equivalent analysis achieved via headless Java scripts.

## Limitations

1. **No GUI/GhidraMCP**: Cannot use interactive Ghidra analysis or MCP tools
2. **Raw Binary Import**: Ghidra auto-analysis on 83.8 MB raw binary is extremely slow and was aborted
3. **No UE3 Signatures**: Without UE3-specific patterns for this build, finding exact globals is very difficult
4. **No GNames Resolution**: Cannot resolve Name indices to strings without confirmed GNames
5. **No Reference Build**: Cannot compare against a known-good build to identify patterns

## Recommended Next Steps

1. **Obtain correct UE3 signatures** for the current APB Reloaded build
2. **Run live debugger** (if anti-cheat allows) to observe actual global values at runtime
3. **Use on-disk unpacked binary** (if available) for better static analysis with proper PE sections
4. **Try different memory acquisition timing** - dump may have been taken during loading when globals weren't fully initialized

## Files Generated

All analysis scripts and outputs located at:
`C:\Users\Support\Desktop\malware_runtime_artifacts\`

Primary outputs:
- `find_ue3_patterns_output.txt` - UE3 pattern search results
- `validate_top_candidates_output.txt` - Top 30 global candidate validation
- `deep_verify_output.txt` - Detailed object structure analysis
- `search_mov_imm64_output.txt` - mov imm64 search results
- `search_raw_qwords_output.txt` - Raw qword search across all VADs
