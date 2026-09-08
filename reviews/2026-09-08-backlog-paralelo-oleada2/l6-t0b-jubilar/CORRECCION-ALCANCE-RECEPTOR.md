MODO NO INTERACTIVO. EJECUTA DE PRINCIPIO A FIN. No preguntes y no pares a confirmar.

**El criterio era mio y estaba mal. Tenias razon en las dos cosas y las he verificado yo abriendo
el codigo:**

- `scripts/4_World/test/LFPG_Sorter_TEST.c:31` -> `class LFPG_Sorter_TEST : LFPG_Sorter`, y
  `config.cpp:1086` -> lo mismo. La entidad V4 hereda de la V3 en script Y en config.
- `scripts/4_World/test/LFPG_ActionOpenSorterPanel_TEST.c:69` sigue llamando a
  `LFPG_SorterView.IsOpen()`.

Y el fallo de encuadre: **puse esos dos ficheros en solo lectura**, asi que te pedi romper una
dependencia y a la vez te quite los ficheros para romperla. No era un limite tuyo. Lo cierro yo
ahora ampliando la lista blanca.

Tu censo se acepta entero: **6 ficheros, 9 clases, 3 layouts.** Conservalo en el informe.

## ALCANCE CORREGIDO — lo que de verdad hay que jubilar

**Jubilar la V3 significa matar su INTERFAZ, no su entidad.** La entidad y su kit se quedan,
porque son la base de la V4 y porque hay jugadores reales con sorters ya colocados en sus bases:
borrar la entidad romperia esos mundos.

### SE BORRA (la UI V3 y su accion)
- `scripts/4_World/LFPG_SorterView.c`
- `scripts/4_World/LFPG_SorterController.c`
- `scripts/4_World/LFPG_SorterTagView.c` (sus DOS clases)
- `scripts/4_World/LFPG_SorterPreviewRow.c` (sus DOS clases)
- `scripts/4_World/LFPG_ActionOpenSorterPanel.c`
- `gui/layouts/LFPG_Sorter.layout`, `LFPG_SorterPreviewRow.layout`, `LFPG_SorterTag.layout`

### SE QUEDA, y esto es lo que cambia respecto a mi brief anterior
- **`scripts/4_World/LFPG_Sorter.c` ENTERO** — sus dos clases, `LFPG_Sorter_Kit` (`:23`) y
  `LFPG_Sorter` (`:35`). Es la base de la V4.
- **`config.cpp:1059` `class LFPG_Sorter`** — no la toques. Es el padre de `LFPG_Sorter_TEST`
  (`config.cpp:1086`) y esta ligada a persistencia.

## LISTA BLANCA AMPLIADA (ahora si puedes escribir en estos)
Todo lo de antes, **mas**:
- `scripts/4_World/test/LFPG_Sorter_TEST.c`
- `scripts/4_World/test/LFPG_ActionOpenSorterPanel_TEST.c`

## Lo que hay que hacer, ahora que si se puede
1. Borrar la UI V3 y sus 3 layouts.
2. Quitar la referencia a `LFPG_SorterView.IsOpen()` de
   `test/LFPG_ActionOpenSorterPanel_TEST.c:69` y el mutex anti dual-open de
   `test/LFPG_SorterView_TEST.c:1398`: ya no hay dos paneles que arbitrar.
3. Retirar de `LFPG_MissionInit.c` las 9 referencias a la V3, el registro de la accion V3 en
   `LFPG_ActionRegistration.c:66` (la de `:69`, la V4, se queda) y las consultas a `IsOpen()` de
   la V3 en `LFPG_Actions.c` y `LFPG_ActionSyncSorter.c`.
4. **Que ningun sorter se quede sin panel.** Con la accion V3 retirada, comprueba si la accion V4
   se ofrece sobre una entidad `LFPG_Sorter` colocada (no `_TEST`). Si no, **haz que la sirva**:
   un jugador con un sorter V3 en su base tiene que poder abrirlo. Es lo mas importante de este
   punto y lo que decide si esto es seguro en un servidor vivo.
5. **Criterio de exito:** grep cero de `LFPG_SorterView`, `LFPG_SorterController`,
   `LFPG_SorterTagView`, `LFPG_SorterPreviewRow` y `LFPG_ActionOpenSorterPanel` (con limite de
   palabra, sin confundir con `_TEST`) fuera de comentarios historicos. `LFPG_Sorter` y
   `LFPG_Sorter_Kit` **SI pueden aparecer**: se quedan.

## Si vuelves a encontrar un bloqueo del mismo tipo
Parar y decirlo fue la respuesta correcta y me ahorraste romper la carga del mundo. Hazlo otra vez
si hace falta. Pero **si el bloqueo es solo que un fichero esta fuera de tu lista blanca, y
tocarlo es la unica forma coherente de cumplir el encargo, dilo nombrando el fichero exacto** en
`LO QUE NO PUDE VERIFICAR`: es lo que me deja cerrarlo en un turno en vez de en dos.

Actualiza `INFORME.md` entero con lo que hagas ahora. No hagas `git commit` ni `git add`.
