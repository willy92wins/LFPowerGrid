# LFPowerGrid

**Las instrucciones de este repo estan en [`AGENTS.md`](AGENTS.md). Leelo antes de tocar codigo.**

Este fichero existe solo para que las dos familias de agentes encuentren lo mismo: Codex carga
`AGENTS.md` por su cuenta, Claude Code carga `CLAUDE.md`. El contenido vive en `AGENTS.md` y no se
duplica aqui, para que no puedan divergir.

Lo minimo, por si no abres el otro fichero:

1. **Todo cambio en `.c`, `.layout` o `config.cpp` pasa el linter offline antes de darse por
   bueno.** `python C:/Users/guill/DayZ-Modding-Knowledge-Pack/tools/dayz-script-validator/scripts/script_validator.py .`
   — gatea por `len(errors)`, **nunca por `status`**, que vale `WARN` con cero errores.
2. **Orden de compilacion `3_Game` → `4_World` → `5_Mission`**, y cada modulo solo ve los anteriores.
3. **Convenciones de la casa:** ni `? :`, ni `++`/`--`, ni `+=`, ni `foreach`, ni `Print(`; `ref`
   solo en miembros de clase. El repo es CRLF y no se reformatea.
4. **Hay jugadores reales en un servidor privado.** Borrar o renombrar un classname de
   `CfgVehicles` puede romper sus bases.
5. **No commitees**: deja los cambios en el arbol de trabajo.
