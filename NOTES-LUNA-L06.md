# LUNA L06 — BalanceProvider reduce-LOC

- Base solicitada: `033c08c986987c50c5983d37811b0222f4cb3d05`.
- Archivo objetivo en base: 2190 líneas (conteo Git de líneas terminadas).
- Archivo objetivo en el worktree al comparar con esa base: 2190 líneas; delta real frente al tip: 0.
- Estado alternativo preexistente: el diff del worktree reporta 221 eliminaciones al compararlo con el índice/artefacto auxiliar de publicación, cuya copia tenía 2411 líneas. Ese artefacto no es la base solicitada y no demuestra reducción en este worktree.
- La comparación directa del contenido del archivo con `HEAD` confirma que no hay ningún cambio de bytes. No se pudo acreditar el umbral de 15% ni 80 líneas netas.

## Qué se revisó

Se inspeccionaron los helpers y consumidores para encontrar código muerto o duplicado. Los helpers con aparentes candidatos tienen consumidores reales. En concreto, `CountPendingPurchaseClaims` cuenta solo claims pendientes con `debit > 0`; `FindLastPendingDeviceClaimIndex` incluye también claims físicos (`debit == 0`), así que sustituir uno por otro alteraría el límite de compras pendientes. No se aplicó esa simplificación.

El archivo ya no tenía secuencias de líneas vacías adicionales que pudieran eliminarse con seguridad. No se cambió lógica ni formato.

## Riesgo residual / bloqueo

No hay un recorte semánticamente seguro demostrado que alcance el umbral en esta sesión. El candidato mecánico del artefacto auxiliar no produce delta frente a `HEAD` en este worktree. Para continuar haría falta una revisión más profunda de las invariantes del libro de claims y sus pruebas; no es seguro inferir código muerto por falta de consumidores locales, dada la superficie de persistencia/refunds. No se ejecutaron pruebas ni el linter porque el archivo no quedó modificado frente al tip.

No se creó commit, push ni PR: el done-when de LOC no está satisfecho y publicar un cambio vacío o uno cuyo beneficio depende de una base distinta ocultaría el estado real.
