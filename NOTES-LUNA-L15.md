# Notas de cambio — Luna L15

- Rama: `chore/luna-reduce-loc-l15`
- Base: `033c08c`
- Único archivo fuente cambiado: `scripts/4_World/LFPG_BTCAtmController.c`
- LOC del archivo: 1027 antes, 1017 después (10 menos; 8 líneas de guards duplicados y 2 líneas de variables `suffix`).
- Los cuatro emisores RPC siguen bajo el guard global `#ifndef SERVER` del archivo; sus cuerpos y escrituras RPC no cambiaron.
- `FormatEur` y `FormatEurInt` siguen concatenando exactamente `" E"`; se retiraron variables temporales que solo retenían ese literal.
- Linter `script_validator.py`: base y árbol actual, ambos `0 errores / 47 warnings / 253 archivos`; salida `WARN` y exit 2 por warnings, delta de errores 0. No es compilador. No se ejecutó prueba in-game.
- Riesgo residual: Los recortes que requieren unificar emisores, ramas de traducción o lecturas quedan documentados como diferidos en `PROPOSALS-LUNA-L15.md`.
- Estado del PR: no abierto. No se pudo crear commit porque Git no pudo escribir `.git/worktrees/wt-L15/index.lock` (Permission denied); además `gh auth status` indica token inválido.
