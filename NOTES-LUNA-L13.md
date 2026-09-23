# Notas de cambio — Luna L13

- Archivo fuente antes: 1.171 líneas (`033c08c`).
- Archivo fuente después: 1.165 líneas.
- Delta neto: -6 líneas.
- Cambio: eliminada la variable local `hasBackup` y sus asignaciones; la condición comprueba los mismos dos archivos con el mismo orden y cortocircuito (`.bak.new` y luego `.bak`). Sin cambio previsto de comportamiento.
- Inventario completo: [PROPOSALS-LUNA-L13.md](PROPOSALS-LUNA-L13.md).
- Linter: 253 ficheros; `errors: []`, `status: WARN`, con warnings de patrones preprocesador no soportados; proceso devolvió exit code 1 (el documento AGENTS del repo indica típicamente 2 para WARN). Sin compilación ni prueba in-game.
- Riesgo residual: equivalencia de evaluación directa razonada por preservar orden/short-circuit de `FileExist`; no hay compilador disponible. Los candidatos de recuperación y persistencia permanecen diferidos.
- PR: pendiente; `gh auth status` reporta token inválido para la cuenta activa.
