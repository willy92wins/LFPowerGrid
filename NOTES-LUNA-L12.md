# Notas de cambio L12

- Write-set: `scripts/4_World/LFPG_BTCAtmView.c` únicamente.
- Conteo físico: 1325 → 1245 líneas (80 LOC netas eliminadas).
- Eliminado: llaves redundantes de 39 condiciones multilínea de una sola sentencia (78 líneas), más `COL_STATUS_ERR_BG`, que no tenía referencias en `scripts/` (1 línea). También se sustituyó un temporal y su retorno por retorno directo (0 LOC netas).
- Validación offline: `script_validator.py .` terminó con `status: WARN`, `errors: []`, 253 archivos escaneados. Warnings del árbol y el chequeo de tipos vanilla omitido por no estar disponible el vanilla tree; no es compilación ni prueba in-game. No se obtuvo una ejecución base para calcular el delta.
- Riesgo residual: no se ejecutó en juego; la equivalencia se limita a quitar llaves alrededor de una única sentencia, sin cambiar condiciones ni orden.
- PR: no abierto. `gh auth status` confirmó que el token configurado es inválido. No se creó commit ni se publicó nada; el worktree queda en `chore/luna-reduce-loc-l12` con estos tres archivos.
