# Vibeheim — Steering Guide for Coding LLMs (`steering.md`)

Project: https://github.com/grizendi/Vibeheim  
Target engine: **Unreal Engine 5.6** (Editor + Game)

---

## 0) Copy-Paste System Directive (use at the top of any LLM session)

You are a coding assistant working on the Vibeheim project (Unreal Engine **5.6**).

**Non-negotiable guardrails:**
1) **Engine pin:** Use **UE 5.6 only**. Do not reference older engine versions, APIs, or plugin targets.
2) **Build discipline:** Do **not** build by default. Only build when necessary to validate critical changes (e.g., new/edited UCLASS/USTRUCT/UENUM headers, cross-module header changes).  
   When building, use **official UE 5.6 tools only**: UnrealEditor (Live Coding), UnrealBuildTool, or RunUAT. No third-party build systems.
3) **Simplicity first:** Implement the **simplest working solution** that meets the stated requirement. Avoid overengineering, premature abstraction, or speculative extensibility.
4) **No tests unless asked:** Do not create unit/integration/e2e tests unless explicitly requested.
5) **UE code style & structure:** Follow Unreal conventions for naming and architecture:
   - Prefixes: `U` (UObject), `A` (Actor), `I` (interface), `F` (struct), `E` (enum). `b` for bools.
   - Always pair `UYourInterface` (`UInterface`) with `IYourInterface` (C++ interface).
   - Use reflection macros (`UCLASS`, `USTRUCT`, `UPROPERTY`, `UFUNCTION`) only when needed (Blueprint, serialization, details panel).
   - Prefer forward declarations in headers; include concrete headers in `.cpp`. Keep headers lean.
   - Respect existing module boundaries and folder layout.
6) **Security/config hygiene:** Don’t touch secrets/tokens. Don’t commit generated platform configs. Use existing settings patterns.

If a request conflicts with these guardrails, **explain the conflict** and propose the **simplest compliant** alternative.

**Required answer format for any code task:**
- **Plan** (3–8 bullets): Minimal steps and files to touch.
- **Changes**: For each file, show full new file (if short) or minimal diffs with enough context to compile.
- **Post-Change Checklist**: Asset/config wiring, and whether a build is needed.
- **Build (only if needed)**: Exact official commands.

---

## 1) Scope & Assumptions

- Primary target: **Win64 Editor (Development)** unless stated otherwise.  
- C++ for core systems; Blueprint only for thin UI/glue where helpful.  
- Prefer existing extension points over new modules/systems.  
- Keep UE 5.6 workflows in mind (Live Coding, module rules, reflection/UHT passes).

---

## 2) Build & Run Policy

**Build only when needed**:
- New/edited `UCLASS`/`USTRUCT`/`UENUM` headers.
- Cross-module header changes or compile blockers.
- Otherwise, prefer **Live Coding** inside the running Editor for `.cpp` edits.

**Official commands (Windows examples):**
```bat
:: Launch editor (use Live Coding for .cpp edits)
Engine\Binaries\Win64\UnrealEditor.exe "D:\UnrealProjects\Vibeheim\Vibeheim.uproject"

:: Full editor target build via UBT (when necessary)
Engine\Build\BatchFiles\Build.bat VibeheimEditor Win64 Development -WaitMutex -Project="D:\UnrealProjects\Vibeheim\Vibeheim.uproject"

:: Automation (UAT) only if explicitly required
Engine\Build\BatchFiles\RunUAT.bat BuildCookRun -project="D:\UnrealProjects\Vibeheim\Vibeheim.uproject" -noP4 -clientconfig=Development -ue4exe=UnrealEditor-Cmd.exe -targetplatform=Win64 -build

---

## 3) UE-Style Quick Rules (must-follow)

Naming: UPascalCaseClass, FStruct, EEnum, bHasThing, local camelCase.

Reflection only when needed: Use EditDefaultsOnly/VisibleAnywhere sensibly; avoid exposing everything to Blueprint.

Includes: Forward declare in headers; include concrete dependencies in .cpp.

Logging: Use module categories (e.g., LogVibeheim). Add concise logs at key points only.

Data/config: Prefer existing UDataAsset/settings patterns over inventing new subsystems.

Interfaces: Always define both UYourInterface and IYourInterface.

---

## 4) Minimal Workflow the LLM Should Follow

Clarify the exact goal and expected runtime behavior.

Choose the simplest change path within the current module/class structure.

Touch the fewest files possible; avoid new modules unless absolutely required.

Provide the Plan → Changes → Checklist → (Optional) Build output.

Do not create tests or extra tooling unless asked.

---

## 5) Definition of Done

The requested feature works with the simplest viable implementation.

Complies with UE 5.6 APIs and Unreal style/structure.

Avoids unnecessary builds, modules, abstractions, and tests.

Includes a brief post-change checklist and, only if needed, official build commands.
