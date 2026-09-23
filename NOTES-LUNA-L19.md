# Notas de reducción LOC — Luna L19

- Write-set de código: `scripts/4_World/LFPG_Furnace.c`.
- LOC físicas (líneas según `splitlines()`): **899 antes → 888 después (−11)**.
- Se retiró el helper `LFPG_GetSwitchState()` sin call sites del horno en `scripts/` (6 LOC con separación), se redujo el retorno booleano de `LFPG_HasCargoItems()`, se devolvió directamente el cálculo final de `LFPG_CalcFuelWhitelist()` y se fusionaron bloques `#ifndef SERVER` adyacentes.
- `PROPOSALS-LUNA-L19.md` conserva el censo de candidatos aplicados y diferidos.
- Riesgo residual: no se ejecutó compilación Enforce ni prueba in-game; el getter eliminado solo se verificó contra referencias en `scripts/`. Linter offline: 253 archivos, `errors: []`, `status: WARN`; avisos preexistentes incluyen el analizador de `#if/#else` no compatible (incluida la zona del `#ifndef SERVER` ya presente).
- `git diff --check`: limpio. No hay cambios en otros archivos de código.
- No se pudo abrir el PR: `gh auth status` informa token inválido para `willy92wins`; API devuelve HTTP 401. El commit deja rama y archivos preparados para abrir el PR cuando se renueve la sesión de GitHub.
