# ProcessPatch & GamePatchManager Usage Guide

This document shows how to use every patching function, organized by what you're trying to accomplish at the assembly level. Each scenario shows the assembly before/after and the code to achieve it.

---

## Table of Contents

1. [Disable a function (patch return)](#scenario-1-disable-a-function-patch-return)
2. [Force a conditional branch to always jump](#scenario-2-force-a-conditional-branch-to-always-jump)
3. [Redirect a call to a different function](#scenario-3-redirect-a-call-to-a-different-function)
4. [Proxy a call (intercept at call site)](#scenario-4-proxy-a-call-intercept-at-call-site)
5. [Proxy a call as a jump (tail-call interception)](#scenario-5-proxy-a-call-as-a-jump-tail-call-interception)
6. [Replace a function at its entry point](#scenario-6-replace-a-function-at-its-entry-point)
7. [Two-step: capture original then replace entry](#scenario-7-two-step-capture-original-then-replace-entry)
8. [Resolve where an instruction points (data extraction)](#scenario-8-resolve-where-an-instruction-points-data-extraction)
9. [Search for a pattern](#scenario-9-search-for-a-pattern)
10. [Proxy a function for all callers (entry-point proxy)](#scenario-10-proxy-a-function-for-all-callers-entry-point-proxy)
11. [Proxy without capturing the original](#scenario-11-proxy-without-capturing-the-original)
12. [Direct function pointer from pattern address](#scenario-12-direct-function-pointer-from-pattern-address)

---

## Call Site vs Entry Point: Understanding the Difference

When you want to intercept a function, there are two places you can patch:

```asm
; CALL SITE (where the function is called FROM)
    ...
    E8 xx xx xx xx       ; call TargetFunc    <- patch HERE for call-site interception
    ...

; ENTRY POINT (where the function starts)
TargetFunc:
    48 89 5C 24 08       ; push rbp           <- patch HERE for entry-point replacement
    48 83 EC 20          ; sub rsp, 0x20
    ...
    ret
```

**Call-site interception** (Scenarios 4, 5) only affects one specific caller. Other callers of the same function are unaffected. You patch the `call` instruction to redirect to your proxy.

**Entry-point replacement** (Scenario 6) affects ALL callers. You patch the first instruction of the function itself, so anyone who calls it gets your replacement. The original function becomes unreachable.

**How returns work after entry-point replacement:**
When the game does `call TargetFunc`, the CPU pushes the return address (the instruction after the `call`) onto the stack. Your replacement at the entry point runs and when it `ret`s, it pops that return address and returns to the original caller automatically via the stacked return address. No extra work needed.

---

## Scenario 1: Disable a function (patch return)

Make a function return immediately without doing anything.

**Assembly before:**
```asm
SomeFunction:
    push rbp              ; 5 bytes of prologue
    sub rsp, 0x20
    ...
    ret
```

**Assembly after:**
```asm
SomeFunction:
    ret                   ; returns immediately
    nop                   ; 4 nops fill the remaining bytes
    nop
    nop
    nop
```

**Code:**
```cpp
uint64_t patAddr = GamePatcher->ResolvePattern(SettingsMgr->pSigCheck, "SigCheck");

// Pattern matched inside the function. Prologue is 0x14 bytes before.
// Overwrite 5 bytes with ret + nops.
GamePatcher->PatchReturnAt(patAddr - 0x14, 5);
```

**ProcessPatch equivalent:**
```cpp
ProcessPatch::PatchReturnAt(addr, 5);
```

**Common sizes:** 5 (call near / typical prologue), 6 (call far / jne near), 2 (short jmp)

**MK12 hooks using this:** `DisableSignatureCheck`

---

## Scenario 2: Force a conditional branch to always jump

Convert a conditional jump (je, jne, jg, etc.) into an unconditional jump. Auto-detects whether it's a short (2-byte) or near (6-byte) conditional.

### Near conditional (0F 84 / 0F 85 / etc, 6 bytes)

**Assembly before:**
```asm
    0F 84 xx xx xx xx    ; je some_address (only jumps if ZF=1)
```

**Assembly after:**
```asm
    E9 xx xx xx xx       ; jmp some_address (always jumps, displacement adjusted)
    90                   ; nop (fills the extra byte)
```

### Short conditional (74 / 75 / etc, 2 bytes)

**Assembly before:**
```asm
    74 xx                ; je short some_address
```

**Assembly after:**
```asm
    EB xx                ; jmp short some_address (always jumps)
```

**Code (auto-detects short vs near):**
```cpp
uint64_t patAddr = GamePatcher->ResolvePattern(SettingsMgr->pTocCheck, "TocSigCheck");

// The conditional jump is 0x12 bytes after the pattern match
GamePatcher->PatchConditionalToUnconditional(patAddr + 0x12);
```

**ProcessPatch equivalent:**
```cpp
ProcessPatch::PatchConditionalToUnconditional(addr);
```

**MK12 hooks using this:** `DisableSignatureWarn` (+0xA, near), `DisableTOCSigCheck` (+0x12, near), `DisablePakTOCCheck` (+0x12, short)

---

## Scenario 3: Redirect a call to a different function

Change which function a `call` instruction targets. The opcode byte stays the same; only the 4-byte displacement changes.

**Assembly before:**
```asm
    E8 xx xx xx xx       ; call OriginalFunc
```

**Assembly after:**
```asm
    E8 yy yy yy yy       ; call DifferentFunc (displacement patched)
```

**Code:**
```cpp
uint64_t patAddr = GamePatcher->ResolvePattern(SettingsMgr->pChunkSigCheck, "ChunkSigCheck");
uint64_t funcAddr = GamePatcher->ResolvePattern(SettingsMgr->pChunkSigCheckFunc, "ChunkSigCheckFunc");

// The call instruction is at patAddr + 0xE. Redirect it to funcAddr.
GamePatcher->RedirectCallTo(patAddr + 0xE, funcAddr);
```

**ProcessPatch equivalent:**
```cpp
// Auto-detects opcode size
ProcessPatch::RedirectCallTo(callAddr, newTarget);

// Manual opcode size override
ProcessPatch::RedirectCallTo(callAddr, newTarget, 1);
```

**MK12 hooks using this:** `DisableChunkSigCheck`

---

## Scenario 4: Proxy a call (intercept at call site)

Redirect a specific `call` instruction to your proxy function. Your proxy receives the same arguments, can inspect/modify them, then forwards to the original. Only this one call site is affected; other callers of the same function are not.

**Assembly before:**
```asm
    ...
    E8 xx xx xx xx       ; call OriginalFunc
    ...                  ; execution continues here after OriginalFunc returns
```

**Assembly after:**
```asm
    ...
    E8 yy yy yy yy       ; call YourProxy (via trampoline)
    ...                  ; execution continues here after YourProxy returns
```

**Code (when you already know the call address):**
```cpp
uint64_t patAddr = GamePatcher->ResolvePattern(SettingsMgr->pFPathLoadPat, "FPathLoad");

// The call instruction is at patAddr + 0xB
GamePatcher->ProxyCallAt(
    patAddr + 0xB,                            // address of the call instruction
    MK12Hook::Proxies::ReadFNameToWStrId,     // your proxy function
    &MK12::FNameToWStr,                       // stores the original function pointer
    PATCH_CALL);                              // keep as a call
```

**Code (one-liner: search + proxy combined):**
```cpp
GamePatcher->ProxyByPattern(
    SettingsMgr->pEndpointLoader,             // pattern string
    "EndpointLoader",                         // hook name (cache key + log label)
    0x00,                                     // offset from pattern to call instruction
    MK12Hook::Proxies::OverrideGameEndpoint,  // your proxy function
    &MK12::GetEndpointKeyValue,               // stores the original function pointer
    PATCH_CALL);                              // patch type
```

**Your proxy function:**
```cpp
wchar_t** OverrideGameEndpoint(MK12::JSONEndpointValue obj, wchar_t** EndpointAddress)
{
    // Inspect or modify arguments here
    // ...

    // Forward to the original function
    return MK12::GetEndpointKeyValue(obj, EndpointAddress);
}
```

**ProcessPatch equivalent:**
```cpp
// Typed (stores original into a function pointer)
ProcessPatch::ProxyCallAt(trampoline, callAddr, proxyFunc, &originalFuncPtr, PATCH_CALL);

// Untyped (returns raw address)
uint64_t* original = ProcessPatch::ProxyCallAt(trampoline, callAddr, proxyFunc, PATCH_CALL);
```

**MK12 hooks using this:** `FNameToStrWithIdLoader` (+0xB), `FNameToStrNoIdLoader` (+0xB), `FNameToStrCommonLoader` (+0x00), `OverrideGameEndpointsData` (0x00), `ProfileGetterHooks` (+30), `FloydTrackingHooks` (+14, +11, +11)

---

## Scenario 5: Proxy a call as a jump (tail-call interception)

Same as Scenario 4, but the `call` is converted to a `jmp`. Your proxy inherits the caller's return address directly from the stack. Use when the call is the last instruction before a return, or when you need tail-call behavior.

**Assembly before:**
```asm
    E8 xx xx xx xx       ; call OriginalFunc
    ; (caller expects OriginalFunc to return here)
```

**Assembly after:**
```asm
    E9 yy yy yy yy       ; jmp YourProxy (via trampoline)
    ; (YourProxy's ret goes directly back to the original caller,
    ;  because the call's return address is still on the stack)
```

**Code:**
```cpp
GamePatcher->ProxyCallAt(
    patAddr + 77,
    MK12Hook::Proxies::SetupSecretFightConditionsProxy,
    &MK12::SetupSecretFightConditions,
    PATCH_JUMP);    // <-- converts the call to a jmp
```

**MK12 hooks using this:** `ExtractFightMetadataFromSecretFightSetupStage` (+77)

---

## Scenario 6: Replace a function at its entry point

Overwrite the first instruction of a function with a jump to your replacement. ALL callers are redirected. The original function is unreachable after this.

**Assembly before:**
```asm
Caller1:
    call OriginalFunc    ; calls the function
    ...

Caller2:
    call OriginalFunc    ; also calls the function
    ...

OriginalFunc:
    48 89 5C 24 08       ; mov [rsp+8], rbx  <- first instruction
    ...
    ret                  ; returns to whoever called
```

**Assembly after:**
```asm
Caller1:
    call OriginalFunc    ; still points here...
    ...

Caller2:
    call OriginalFunc    ; still points here...
    ...

OriginalFunc:
    FF 25 xx xx xx xx    ; jmp YourReplacement (via trampoline)
    ...                  ; original code is unreachable
```

**How the return works:** When `Caller1` does `call OriginalFunc`, the CPU pushes the return address onto the stack. At `OriginalFunc`, the jump sends execution to your replacement. When your replacement does `ret`, it pops that return address and goes back to `Caller1`. The caller doesn't know anything changed.

**Code:**
```cpp
GamePatcher->ReplaceFunctionWith(
    (uint64_t)MK12::ReadFNameToWStrWithIdStart,       // function entry address
    MK12::Remake::FNameInfoToWStringWithId);            // your replacement function
```

**ProcessPatch equivalent:**
```cpp
ProcessPatch::ReplaceFunctionWith(trampoline, funcEntryAddr, replacementFunc);
```

**Important:** The original function is gone. If your replacement needs to call the original, you must capture the original function pointer BEFORE replacing (see Scenario 7).

**MK12 hooks using this:** `OverrideFNameToWStrFuncs` (x3: WithId, NoId, Common)

---

## Scenario 7: Two-step: capture original then replace entry

When you want to replace a function at its entry point BUT your replacement still needs to call the original. This requires two steps in order:

1. **Step 1:** Proxy a call site to capture the original function pointer
2. **Step 2:** Replace the function entry with your Remake that internally calls the captured original

**Step 1 assembly (at one call site):**
```asm
    ; Before:
    E8 xx xx xx xx       ; call OriginalFunc

    ; After:
    E8 yy yy yy yy       ; call Proxy
    ; Proxy captures OriginalFunc's address into MK12::FNameToWStr
```

**Step 2 assembly (at the function entry):**
```asm
    ; Before:
OriginalFunc:
    48 89 5C 24 08       ; original prologue

    ; After:
OriginalFunc:
    FF 25 xx xx xx xx    ; jmp RemakeFunc
```

**RemakeFunc internally calls the captured original:**
```cpp
uint64_t RemakeFunc(FNameInfoStruct* info, char* dest)
{
    FName* Name = NameTableIndexToFName(info);

    // Call the ORIGINAL function via the pointer captured in step 1
    MK12::FNameToWStr(*Name, dest);

    *(wchar_t*)&dest[2 * size] = L'\0';
    return size;
}
```

**Code:**
```cpp
// Step 1: Proxy a call site to capture the original function pointer
GamePatcher->ProxyCallAt(patAddr + 0xB,
    MK12Hook::Proxies::ReadFNameToWStrId,
    &MK12::FNameToWStr,         // <- original captured here
    PATCH_CALL);

// ... later, after UNameTableGetter resolves the function start ...

// Step 2: Replace the function entry with the Remake
GamePatcher->ReplaceFunctionWith(
    (uint64_t)MK12::ReadFNameToWStrWithIdStart,
    MK12::Remake::FNameInfoToWStringWithId);  // <- uses MK12::FNameToWStr internally
```

**Dependency:** Step 1 MUST run before Step 2. The Remake function calls `MK12::FNameToWStr` which is only valid after the proxy captures it.

**MK12 hooks using this:** The FName hooking chain: `FNameToStrWithIdLoader` (step 1) -> `OverrideFNameToWStrFuncs` (step 2)

---

## Scenario 8: Resolve where an instruction points (data extraction)

Not all hooks modify code. Sometimes you need to read where an instruction points to extract a game data pointer.

**Assembly:**
```asm
    80 3D xx xx xx xx 00    ; cmp byte ptr [rip+xxxxxxxx], 0
```

The `xx xx xx xx` is a RIP-relative displacement. The actual address is:
`instruction_address + displacement + instruction_length`

### Auto-detect

When `ParseInstruction` recognizes the encoding:

```cpp
uint64_t target = ProcessPatch::ResolveDestination(addr);
// Returns the absolute address, or 0 if not recognized
```

### Manual params

When auto-detect doesn't recognize the encoding, provide the layout:

```cpp
// Parameters: addr, dispOffset, instrSize, dispSize
uint64_t target = ProcessPatch::ResolveDestination(addr, 2, 7, 4);
```

### Real example (UNameTableGetter):
```cpp
uint64_t patAddr = GamePatcher->ResolvePattern(SettingsMgr->pUNameObjGetPat, "UNameObjGet");
uint64_t* lpPattern = (uint64_t*)patAddr;

// "80 3D xx xx xx xx 00" at patAddr: dispOffset=2, instrSize=7, dispSize=4
// Subtract 0x8 to get the struct base (the cmp points 8 bytes into the struct)
uint64_t UNameMain = ProcessPatch::ResolveDestination(patAddr, 2, 7, 4) - 0x8;

// "48 8B 0D xx xx xx xx" at patAddr + 0x12: dispOffset=3, instrSize=7, dispSize=4
uint64_t UNameSub = ProcessPatch::ResolveDestination(patAddr + 0x12, 3, 7, 4);

// "E8 xx xx xx xx" at patAddr + 0x22: auto-detect works for call instructions
MK12::InitializeNameTable = (MK12::InitializeNameTableType*)ProcessPatch::ResolveDestination(patAddr + 0x22);
```

### Common instruction layouts

| Instruction | Example bytes | dispOffset | instrSize | dispSize |
|-------------|--------------|------------|-----------|----------|
| call near | `E8 xx xx xx xx` | 1 | 5 | 4 |
| jmp near | `E9 xx xx xx xx` | 1 | 5 | 4 |
| jmp short | `EB xx` | 1 | 2 | 1 |
| je/jne near | `0F 84 xx xx xx xx` | 2 | 6 | 4 |
| je short | `74 xx` | 1 | 2 | 1 |
| cmp [rip+off] | `80 3D xx xx xx xx 00` | 2 | 7 | 4 |
| mov [rip+off] | `48 8B 0D xx xx xx xx` | 3 | 7 | 4 |
| lea [rip+off] | `48 8D 0D xx xx xx xx` | 3 | 7 | 4 |
| call indirect | `FF 15 xx xx xx xx` | 2 | 6 | 4 |

**MK12 hooks using this:** `UNameTableGetter`

---

## Scenario 9: Search for a pattern

### Raw search (no cache, no logging)

```cpp
// ProcessPatch level (any module)
uint64_t* result = ProcessPatch::SearchPattern(moduleHandle, "48 8B 0D ? ? ? ? E8");

// ProcessPatch level (main module)
uint64_t* result = ProcessPatch::SearchPattern("48 8B 0D ? ? ? ? E8");

// GamePatchManager level (game module)
uint64_t* result = GamePatcher->SearchPattern("48 8B 0D ? ? ? ? E8");
```

### Search with cache, validation, and error logging

```cpp
uint64_t addr = GamePatcher->ResolvePattern(
    SettingsMgr->pSigCheck,    // pattern string from INI
    "SigCheck",                // hook name (cache key + log label)
    true);                     // cache (default true, false for dynamic code)
// Returns the address, or 0 with printfError on failure
```

### PatternFinder class

```cpp
PatternFinder pat;
pat.Search("48 8B 0D ? ? ? ? E8");         // searches with cache
pat.Search("48 8B 0D ? ? ? ? E8", false);  // without cache (dynamic code)

// Arithmetic on the result
uint64_t callAddr = (pat + 0xB);

// Resolve where the instruction at the match points
uint64_t target = pat.Resolve();
```

---

## Scenario 10: Proxy a function for all callers (entry-point proxy)

Intercept a function at its entry point so ALL callers are redirected, while keeping the original callable. Combines the best of `ProxyCallAt` (original stays callable) and `ReplaceFunctionWith` (all callers intercepted).

Use this for statically linked functions (like curl) where there's no single call instruction to patch, or when you need to intercept every caller.

**Assembly before:**
```asm
Caller1:
    call TargetFunc      ; calls the function
    ...
Caller2:
    call TargetFunc      ; also calls the function
    ...

TargetFunc:
    push rbp             ; original prologue
    mov rbp, rsp
    sub rsp, 0x40
    ...
    ret
```

**Assembly after:**
```asm
Caller1:
    call TargetFunc      ; still points here...
    ...
Caller2:
    call TargetFunc      ; still points here...
    ...

TargetFunc:
    jmp YourProxy        ; redirected to your proxy

Trampoline:
    push rbp             ; saved prologue (original bytes)
    mov rbp, rsp
    jmp TargetFunc+N     ; continues into original body
```

**Your proxy can call the original through the trampoline:**
```cpp
// The proxy has the same signature as the target
__int64 __fastcall MySetOptProxy(__int64 handle, __int64 option, __int64 value)
{
    // Inspect or modify arguments before
    printf("option=%lld\n", option);

    // Call the original function (via trampoline)
    __int64 result = OriginalSetOpt(handle, option, value);

    // Inspect or modify the result after
    return result;
}
```

**Code:**
```cpp
uint64_t funcAddr = base + 0x5878250; // address of the function to proxy

GamePatcher->ProxyFunctionAt(funcAddr,
    MySetOptProxy,           // your proxy function
    &OriginalSetOpt,         // stores the callable original
    "curl_easy_setopt");     // name for logging
```

**ProcessPatch equivalent:**
```cpp
ProcessPatch::ProxyFunctionAt(funcAddr, (void*)MySetOptProxy, &OriginalSetOpt);
```

**To remove the proxy and restore the original:**
```cpp
GamePatcher->UnproxyFunctionAt(funcAddr, "curl_easy_setopt");
```

**Comparison with other proxy methods:**

| Method | Patches at | All callers? | Original callable? |
|--------|-----------|-------------|-------------------|
| `ProxyCallAt` | A `call` instruction | No, one caller only | Yes |
| `ReplaceFunctionWith` | Function entry | Yes | No, original is gone |
| `ProxyFunctionAt` | Function entry | Yes | Yes, via trampoline |

**MK12 hooks using this:** `CurlSetOptProxy` (statically linked `curl_easy_setopt`)

---

## Scenario 11: Proxy without capturing the original

Sometimes you want to redirect a call but don't need to call the original. For example, replacing a check function with a no-op.

**Assembly before:**
```asm
    E8 xx xx xx xx       ; call SignatureCheckFunc
```

**Assembly after:**
```asm
    E9 yy yy yy yy       ; jmp DummyVoidFunc (does nothing, returns)
```

**Code:**
```cpp
// Use ProxyCallAt but discard the original pointer
void* unused;
GamePatcher->ProxyCallAt(patAddr + 7, ProcessPatch::DummyVoidFunc, &unused, PATCH_JUMP);
```

Or use ProcessPatch directly:
```cpp
ProcessPatch::ProxyCallAt(trampoline, addr, ProcessPatch::DummyVoidFunc, PATCH_JUMP);
// Return value (original address) can be discarded
```

**DummyPtrFunc** is also available for functions that need to return a pointer:
```cpp
ProcessPatch::ProxyCallAt(trampoline, addr, ProcessPatch::DummyPtrFunc, PATCH_CALL);
// Replaces the call target with a function that returns nullptr
```

---

## Scenario 12: Direct function pointer from pattern address

Sometimes the pattern match IS the function entry. No opcode resolution needed, just cast the address.

**Code:**
```cpp
uint64_t patAddr = GamePatcher->ResolvePattern(pattern, "QuitGame");

// The pattern matched at the start of the function
SomeNamespace::QuitGame = (SomeNamespace::QuitGameType*)(uint64_t*)patAddr;
```

This is useful for functions you want to CALL (not hook), like callbacks, constructors, or utility functions the game exposes.

---

## Quick Reference

| I want to... | Use this |
|---|---|
| Make a function return immediately | `PatchReturnAt(addr, size)` |
| Force a conditional branch | `PatchConditionalToUnconditional(addr)` |
| Change which function a `call` targets | `RedirectCallTo(callAddr, newTarget)` |
| Intercept a call, inspect args, forward to original | `ProxyCallAt(callAddr, proxy, &original)` |
| Same but find the call by pattern | `ProxyByPattern(pattern, name, offset, proxy, &original)` |
| Same but convert call to jmp (tail-call) | `ProxyCallAt(..., PATCH_JUMP)` |
| Replace a function for ALL callers | `ReplaceFunctionWith(entryAddr, replacement)` |
| Intercept a function for ALL callers, keep original callable | `ProxyFunctionAt(funcAddr, proxy, &original)` |
| Remove a function proxy | `UnproxyFunctionAt(funcAddr)` |
| Replace entry but still call original (two-step) | Step 1: `ProxyCallAt` to capture, Step 2: `ReplaceFunctionWith` |
| Read where an instruction points | `ResolveDestination(addr)` or `ResolveDestination(addr, disp, size, ...)` |
| Search for bytes in memory | `SearchPattern(pattern)` |
| Search with cache + error handling | `ResolvePattern(pattern, name)` |
| Replace a call with a no-op | `ProxyCallAt(addr, DummyVoidFunc, &unused, PATCH_JUMP)` |
| Get a function pointer from a pattern match | Cast: `(FuncType*)(uint64_t*)patAddr` |
