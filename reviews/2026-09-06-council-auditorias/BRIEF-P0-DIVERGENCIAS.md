# ENCARGO — inventario de divergencias V3 ↔ V4 del sorter

## 1. Por qué existe este encargo
El proyecto va a **borrar la V3 del sorter**. Hoy conviven dos implementaciones de la misma UI y
se sabe que **ya han divergido** (ficha S10: difieren en la protección de preview). Lo que **no**
se sabe es en qué más, porque la única auditoría que miró el fork recibió los cuatro ficheros
grandes **truncados** y solo vio extractos.

Borrar la V3 destruye el oráculo: hoy cualquier duda de paridad se resuelve comparando con la V3
viva; mañana no. **Este inventario es la precaución previa a una acción irreversible.** No es
opcional y no es un refactor: es una lectura.

## 2. Entorno — no heredas nada
- Repo `P:\LFPowerGrid`. **Enforce Script de DayZ**, ficheros `.c`. **No es C ni C++.**
- **PROHIBIDO** editar cualquier fichero, ejecutar git, compilar o lanzar DayZ. **Solo lectura.**
  Tu entregable es un documento.

## 3. Los pares a comparar

| V3 (producción) | V4 (nueva) |
|---|---|
| `scripts/4_World/LFPG_SorterView.c` | `scripts/4_World/test/LFPG_SorterView_TEST.c` |
| `scripts/4_World/LFPG_SorterController.c` | `scripts/4_World/test/LFPG_SorterController_TEST.c` |
| `scripts/4_World/LFPG_SorterTagView.c` | `scripts/4_World/test/LFPG_SorterTagView_TEST.c` |
| `scripts/4_World/LFPG_SorterPreviewRow.c` | `scripts/4_World/test/LFPG_SorterPreviewRow_TEST.c` |
| `scripts/4_World/LFPG_ActionOpenSorterPanel.c` | `scripts/4_World/test/LFPG_ActionOpenSorterPanel_TEST.c` |

Y los layouts: `gui/layouts/LFPG_Sorter*.layout` frente a `gui/layouts/test/LFPG_*_TEST.layout`.

Al comparar, **normaliza mentalmente el sufijo `_TEST`**: `LFPG_ColorData_TEST` es el homólogo de
`LFPG_ColorData`, y así con todo. Una diferencia que sea solo el sufijo **no es una divergencia**.

## 4. Qué buscas, y qué NO

**Buscas divergencias de COMPORTAMIENTO.** Lo que importa es: si mañana borramos la V3 y la V4
ocupa su sitio, ¿qué hará distinto el mod?

Presta atención especial a:
- **Guardas y validaciones que existan en una y no en la otra.** Es el caso conocido (S10,
  protección de preview) y el más peligroso: si la guarda buena está en la V3, borrarla es una
  regresión silenciosa.
- **Condiciones de las acciones**: qué exige cada una para aparecer o ejecutarse
  (`powered`, `linked`, distancia, tipo exacto vs `IsKindOf`).
- **Manejo de errores y de nulos**: dónde una comprueba y la otra no.
- **Ciclo de vida**: `Init`/`Open`/`Close`/`Cleanup`, liberación de referencias, `SetUserData`.
- **Contrato de red**: qué sub-ids emite y atiende cada una.
- **Límites y constantes** con valores distintos entre las dos.
- **Correcciones que solo se aplicaron a un lado.** Busca comentarios de arreglo (`fix`, `v2.`,
  `F1-B`, `heap`, `Sprint`) presentes en una y ausentes en la otra: son la huella de una
  corrección que no se propagó.

**NO buscas** (no lo reportes): diferencias de nombre por el sufijo, orden de métodos, formato,
comentarios de cabecera histórica, ni el layout distinto por sí mismo.

## 5. Entregable
Escribe `reviews/2026-09-06-council-auditorias/INVENTARIO-DIVERGENCIAS-V3-V4.md`.

Por divergencia:

```
### D-NN — <título de una línea>
- **V3:** path:line + el fragmento
- **V4:** path:line + el fragmento, o «ausente»
- **Qué cambia si borramos la V3:** la consecuencia concreta, en una o dos frases
- **Dirección:** ¿cuál de las dos se comporta mejor? V3 | V4 | equivalentes | no decidible
- **Riesgo:** alto | medio | bajo
```

Ordena por riesgo, de mayor a menor.

Antes de la lista, una tabla resumen: `nº de divergencias | cuántas favorecen a V3 | cuántas a V4
| cuántas no decidibles`. **Si la mejor implementación está en la V3 en algún punto, ese es el
hallazgo más valioso de todo el encargo**, porque es exactamente lo que el borrado se llevaría por
delante.

Termina con `## LO QUE NO PUDE VERIFICAR`. Si algún fichero es demasiado grande para leerlo entero,
**dilo y di qué tramos leíste** — un inventario que se presenta como completo sin serlo es peor
que uno que declara sus huecos.
