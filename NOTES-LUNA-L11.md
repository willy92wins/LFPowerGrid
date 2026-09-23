# Notas de reducción LOC — Luna L11

- Rama: `chore/luna-reduce-loc-l11`
- Write-set de código: `scripts/4_World/LFPG_TestDevices.c`
- Líneas físicas del fuente: 1.447 antes, 1.438 después (−9 netas).
- Eliminado: guard `#ifndef SERVER` redundante dentro del guard exterior de `LF_TestLamp.OnVariablesSynchronized`; variable local constante `bEnable`; comentarios redundantes sobre estado derivado/persistencia y el no-op de `LFPG_SetPowered`.
- Riesgo residual: bajo para comportamiento. Los cambios ejecutables quitan un guard duplicado y pasan `true` literal a la misma llamada; los comentarios retirados no eran ejecutables. No hubo compilador Enforce ni prueba in-game.
- Verificación: linter `script_validator.py .`: `status=WARN`, 0 errores, 47 warnings, 253 archivos escaneados, exit 2 (warning aprobado según el gate del repo). Igual número de warnings que la línea base conocida; falta el árbol vanilla, por lo que `ES-UNDEFINED-CLASS-REF` fue omitido. `git diff --check` pasa.
- PR/commit: bloqueados por permisos del worktree. `git commit` no pudo crear `.git/worktrees/wt-L11/index.lock` (Permission denied), y `gh auth status` informa token inválido para `willy92wins`. No se modificó el índice ni se creó commit; por tanto no se lanzó `gh pr create`.
