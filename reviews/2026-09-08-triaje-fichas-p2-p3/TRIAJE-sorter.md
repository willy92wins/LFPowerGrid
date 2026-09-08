# TRIAJE — Sorter P2/P3 (22 fichas)

| ID | Veredicto | Fichero principal | Motivo y supervivencia en V4 |
|---|---|---|---|
| S01 | VIVA | `scripts/3_Game/LFPG_SorterData.c:594` | Rechaza cero reglas/catch-all, incluido el JSON de Reset All. Compartida V3/V4. |
| S02 | VIVA | `scripts/3_Game/LFPG_SorterData.c:330` | Muta durante el parseo y busca claves fuera del objeto; Preview consume incluso el resultado fallido. Compartida. |
| S03 | VIVA | `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2668` | Save admite hasta 4096 caracteres; Preview rechaza más de 2048. Compartida. |
| S05 | VIVA | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:6077` | Copia todo el cargo antes de limitar evaluación/movimientos, también en sort manual. Compartida. |
| S06 | VIVA | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:6091` | Valida reanudación por ítem/config, sin comparar máscara ni generación de cables. Compartida. |
| S07 | VIVA | `scripts/5_Mission/LFPG_SorterLogic.c:576` | Hay cachés parciales, pero SLOT se parsea por comparación y las rutas se evalúan por ítem. Compartida; deuda de coste, sin medición de daño. |
| S09 | VIVA | `scripts/4_World/LFPG_RPCClientHandler.c:967` | Repack puede cambiar posiciones con movedCount=0: el ACK no refresca y el broadcast excluye al solicitante. También V4. |
| S10 | DUDOSA | `scripts/4_World/test/LFPG_SorterController_TEST.c:1718` | La duplicación desaparece al retirar V3; la supuesta divergencia de protección de preview no está localizada en HEAD. |
| S11 | VIVA | `scripts/4_World/test/LFPG_SorterController_TEST.c:1188` | Save/Sort quedan en vuelo ante retornos sin ACK; el temporizador de texto no libera el bloqueo. También V3. |
| S12 | VIVA | `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:3034` | Preview identifica sólo salida; ACK sólo resultado. El singleton abierto recibe respuestas sin entidad/revisión. También V4. |
| S13 | DUDOSA | `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2983` | Es preview de coincidencias de la salida seleccionada; no simula prioridades, cables ni capacidad. Falta fijar la promesa de producto. Compartida. |
| S14 | VIVA | `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2956` | Preview enumera cargo sin los guards de acceso usados para ordenar. Compartida; divulgación efectiva condicionada al contenedor. |
| S15 | VIVA | `scripts/5_Mission/LFPG_SorterLogic.c:1012` | Sólo el origen del tick comprueba distancia del vínculo; destino y origen manual no. Compartida. |
| S16 | VIVA | `scripts/4_World/LFPG_Sorter.c:334` | Persiste NetworkID y acepta cualquier contenedor resoluble con cargo al reiniciar, sin identidad persistente. V4 hereda este código. |
| S17 | VIVA | `scripts/4_World/LFPG_Sorter.c:407` | Dos búsquedas aceptan attachment-only; tick/manual/preview requieren cargo. V4 hereda ambas. |
| S18 | DUDOSA | `scripts/5_Mission/LFPG_SorterLogic.c:950` | Sigue usando ConfigGetInt con sufijos 0/1; la firma pública no demuestra si el motor admite ese acceso al array. Compartida. |
| S19 | VIVA | `scripts/5_Mission/LFPG_SorterLogic.c:748` | GhillieSuit_ColorBase retorna ATTACHMENT antes de comprobar ropa. Compartida. |
| S20 | VIVA | `scripts/4_World/test/LFPG_SorterController_TEST.c:1509` | Rehace chips/filas y re-resuelve botones incluso al seleccionar la misma salida o fallar AddRule. También V3. |
| S21 | VIVA | `scripts/4_World/test/LFPG_SorterController_TEST.c:1377` | El refresco granular omite RefreshRail_TEST; el contador del rail queda antiguo. Es propia de V4. |
| S22 | MUERTA | `scripts/4_World/test/LFPG_SorterView_TEST.c:93` | Ya murió el 28-08 al retirar el scaler; hoy se reincorpora aislando y restaurando su estado global. |
| S23 | VIVA | `scripts/4_World/test/LFPG_SorterTagView_TEST.c:17` | Índices, owner y color almacenados sin lectura funcional; el borrado usa UID. También V3. |
| S24 | VIVA | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:6066` | Persisten resolución duplicada de salidas, guards semejantes y arrays paralelos. Compartida; deuda de mantenimiento, no avería acreditada. |

## Alcance, evidencia y lectura del borrado de V3

Revisión estática realizada el **08-09-2026**, sobre **`d59cad892557d8ec8dcfed0bfca0ba1c5744db45`**. Resultado: **18 VIVA, 3 DUDOSA, 1 MUERTA, 0 NO-LOCALIZADA y 0 MUERE-CON-V3 como ficha completa**. La parte de duplicación de S10 sí desaparece con V3; su afirmación adicional sobre protección no se puede dar por comprobada.

Se leyeron implementación, consumidores, emisores/receptores RPC y caminos de invalidación. El historial se empleó para fechar S22, después de comprobar el mecanismo actual. No se infirió vigencia por archivo intacto. Los ejemplos son recorridos estáticos del código, no pruebas de Enforce ejecutadas.

**V4 no tiene una segunda implementación de la entidad:** `scripts/4_World/test/LFPG_Sorter_TEST.c:31` hereda de `LFPG_Sorter`; su único override es `SetActions`, en `:39`. Los pares de RPC V3/V4 convergen en los mismos handlers de servidor (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:132`, `:142`, `:162`). Por tanto, los defectos de `LFPG_Sorter.c`, SorterData, SorterLogic y NetworkManager **sobreviven en V4** mientras se conserve su comportamiento, aunque otra lane traslade o renombre la base.

El borrado anunciado ocurre en otra copia: este informe no certifica su resultado ni anticipa sus líneas finales. Ninguna ficha se marca MUERE-CON-V3 sólo porque su evidencia esté en un fichero incluido en la lista de borrado.

Costes estimados para corregir el comportamiento que permanece en V4: **TRIVIAL** = una línea; **ACOTADO** = un fichero; **AMPLIO** = varios ficheros o cambio de contrato. No son presupuestos ni autorizan cambios; aquí no se ha modificado código.

## Fichas vivas y dudosas

### S01 — VIVA — Reset All no admite guardado

**Evidencia:** `scripts/4_World/test/LFPG_SorterController_TEST.c:1174` ejecuta ResetAll y `:1192` serializa lo que queda; `scripts/3_Game/LFPG_SorterData.c:268` vacía salidas y `:594` devuelve false cuando no se ha contado ninguna regla ni catch-all. `scripts/4_World/LFPG_Sorter.c:655` rechaza esa configuración sin sustituir la anterior.

Una configuración vacía pero estructuralmente válida no se puede guardar: al reabrir reaparecen las reglas antiguas. También sucede al eliminar la última regla de la última salida, sin usar Reset All.

**V3/V4:** V3 hace el mismo reset en `scripts/4_World/LFPG_SorterController.c:1078`; ambas usan el parser y setter compartidos. **Coste: ACOTADO**, aceptar el estado vacío válido en el parser. **Agrupación:** misma función `FromJSON` que S02; mismo fichero que S03/S07 y mismo flujo de Save que S11/S12.

### S02 — VIVA — Parser parcial y delimitadores sin límite de objeto

**Evidencia:** `scripts/3_Game/LFPG_SorterData.c:332` borra el estado inicial, `:466` inserta reglas durante el parseo, `:492` busca `ca` hacia delante sin frontera de objeto y `:579` devuelve false sin revertir las inserciones. `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2937` sólo registra el fallo: `:2942` sigue usando esa configuración para Preview.

El fallo deja reglas parciales utilizables, y una clave de la salida siguiente puede atribuirse a la anterior. Por ejemplo, el recorrido de `{"o":[{"r":[]},{"r":[],"ca":true}]}` encuentra el `ca` del segundo objeto desde el primero, marca catch-all en salida 0 y salta el segundo objeto al avanzar hasta su cierre: la validación final no detecta ese desplazamiento.

**Matiz:** Save ya es transaccional respecto a la configuración activa: `scripts/4_World/LFPG_Sorter.c:654` parsea en un objeto temporal y sólo sustituye en `:663`. No afirmo que un Save rechazado destruya las reglas activas. Sí quedan consumidores no transaccionales: Preview, carga persistida en `scripts/4_World/LFPG_Sorter.c:364` e inicialización UI en `scripts/4_World/test/LFPG_SorterController_TEST.c:556`.

**V3/V4:** parser/servidor compartidos; inicialización V3 también llama a FromJSON (`scripts/4_World/LFPG_SorterController.c:570`). **Coste: ACOTADO**, corregir las fronteras y la publicación del resultado en el parser común. **Agrupación:** S01 en `FromJSON`; S13/S14 en `HandleSorterPreviewRequest`. No contar estas manifestaciones como parsers distintos.

### S03 — VIVA — Save y Preview tienen límites distintos

**Evidencia:** `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2668` limita Save a 4096; `:2899` limita Preview a `LFPG_SORT_MAX_JSON_BYTES`, definido como 2048 en `scripts/3_Game/LFPG_SorterData.c:45`. El setter `scripts/4_World/LFPG_Sorter.c:651` no impone el límite menor.

No es una diferencia inalcanzable: seis salidas con ocho reglas PREFIX distintas por salida y valores ASCII de 40 caracteres generan **2761 caracteres** con el formato de `ToJSON`. Cumplen ocho reglas por salida y el máximo de 64 caracteres por valor (`scripts/3_Game/LFPG_SorterData.c:130`, `:175`), pero Preview retorna sin respuesta por tamaño.

**V3/V4:** Save V4 (`scripts/4_World/test/LFPG_SorterController_TEST.c:1192`) y Preview V4 (`:1757`) envían el mismo JSON completo; V3 hace lo equivalente. **Coste: TRIVIAL** para unificar el umbral servidor en una línea; hacer visible el rechazo y evitar el bloqueo es además S11. **Agrupación:** handlers compartidos con S01/S02/S11/S12/S13/S14.

### S05 — VIVA — Copia completa antes del presupuesto

**Evidencia:** `scripts/5_Mission/LFPG_NetworkManagerImpl.c:6077` copia todos los ítems del cargo; los límites de movimientos/evaluaciones llegan en `:6118` y el presupuesto de reglas en `:6150`. La operación manual también copia todo en `:6511` antes de comprobar su tope de 200 evaluaciones en `:6533`.

El presupuesto no acota el trabajo de enumerar y copiar un cargo grande, incluido el que sólo podrá procesarse parcialmente en esa visita. El cambio S04 de hoy adelanta el turno del siguiente sorter (`:6216`), pero no limita esta preparación.

**V3/V4:** ambas usan el mismo manager. **Coste: ACOTADO**, acotar preparación y cursor dentro de ese fichero preservando la estabilidad frente a movimientos de inventario. **Agrupación:** `LFPG_TickSorters` con S06/S07/S15/S24; `HandleSorterRequestSort` con S09/S15/S24. El repack sí recibió un límite previo hoy (`scripts/5_Mission/LFPG_SorterLogic.c:1133`); ese arreglo S08 no mata esta ficha.

### S06 — VIVA — Reanudación ajena a la topología

**Evidencia:** `scripts/5_Mission/LFPG_NetworkManagerImpl.c:6085` recupera ítem, índice, salida, regla y configuración; `:6091` valida sólo identidad del ítem/config y su presencia. `:6151` pasa ese cursor al evaluador con la máscara de cables recién calculada en `:6062`. `scripts/5_Mission/LFPG_SorterLogic.c:388` empieza por `startOutput`, saltando las salidas anteriores.

Si una evaluación queda diferida en salida 4 y se conecta una salida 1 que ahora debe ganar por prioridad, la reanudación puede saltársela y enviar el ítem a una salida posterior. El caso requiere mantener alguna salida conectada: una máscara cero sí limpia la reanudación en `scripts/5_Mission/LFPG_NetworkManagerImpl.c:6073`.

**Comprobación fuera del fichero:** existe generación de cables en `scripts/4_World/LFPG_WireOwnerBase.c:161`; AddWire y ClearWires la incrementan en `:199` y `:212`. No está incluida en el estado de reanudación del sorter; tampoco se encontró un llamador externo que lo invalide al cablear. **V3/V4:** compartida. **Coste: ACOTADO**, asociar la reanudación a la topología relevante y comprobar cambios de máscara, dentro del manager. **Agrupación:** misma función y arrays que S05/S24; debe coordinarse con cualquier caché de rutas de S07.

### S07 — VIVA — Optimización parcial, no desaparición completa

**Evidencia:** `scripts/5_Mission/LFPG_SorterLogic.c:576` vuelve a separar y convertir los extremos SLOT en cada MatchRule; `:409` recorre las reglas para cada ítem. `scripts/5_Mission/LFPG_NetworkManagerImpl.c:6151` y `:6547` evalúan cada ítem aunque comparta tipo, reglas y conectividad con el anterior.

Persisten cálculos repetibles por regla/tipo. **Ya existen mejoras:** normalización PREFIX/CONTAINS al crear regla (`scripts/3_Game/LFPG_SorterData.c:201`) y cachés de categoría, nombre y dimensiones (`scripts/5_Mission/LFPG_SorterLogic.c:602`); no debe presentarse la ficha como ausencia total de caché ni cuantificarse el ahorro sin medir.

**V3/V4:** lógica compartida. **Coste: AMPLIO** para cubrir toda la ficha: compilar la representación de reglas y cachear resultados con invalidación por reglas/conectividad; no cachear permisos o capacidad mutable como si fueran propiedades del tipo. **Agrupación:** S02 en representación de reglas; S06 en invalidación; S18/S19/S24 en SorterLogic; S05 en el tick. Es una oportunidad de rendimiento presente, no un bloqueo del jugador demostrado.

### S09 — VIVA — Repack sin transferencias no refresca al solicitante

**Evidencia:** `scripts/5_Mission/LFPG_NetworkManagerImpl.c:6499` ejecuta repack sin salidas, `:6503` difunde excluyendo al solicitante y `:6504` retorna cero. La exclusión se aplica realmente en `:6357`. En V4, `scripts/4_World/LFPG_RPCClientHandler.c:967` sólo llama a UpdateInventoryMenu y solicita la señal de refresco si `success && movedCount > 0`; V3 conserva el mismo guard en `:598`.

Reordenar posiciones dentro del cargo puede haber cambiado el inventario con cero transferencias entre contenedores. El resultado del repack se descarta, por lo que el solicitante carece del refresco explícito que sí reciben sus vecinos; también afecta a la rama con cables cuando ningún ítem se transfiere (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:6589`).

**Cambio de hoy comprobado:** la suscripción V4 a la señal (`scripts/4_World/test/LFPG_SorterView_TEST.c:1520`) no cura una señal que no se emite para ese jugador. **V3/V4: sobrevive en ambas. Coste: TRIVIAL** para refrescar V4 ante cualquier ACK satisfactorio cambiando su guard; **ACOTADO** si se mantiene la paridad de ambos handlers del mismo fichero. **Agrupación:** S11/S12 en ACK; S05/S15/S24 en sort manual. El retraso visual exacto queda pendiente de motor; la omisión del aviso está demostrada.

### S10 — DUDOSA — Duplicación real, divergencia de preview no localizada

**Lectura 1, mantenimiento:** existen dos controllers/views, dos rutas cliente y dos clases publicadas (`config.cpp:1059`, `:1086`; `scripts/4_World/LFPG_RPCClientHandler.c:568`, `:937`). Ese coste de duplicación **MUERE-CON-V3** si se retira completamente la UI/acción/ruta antigua. No hace falta reimplementar la lógica común dos veces.

**Lectura 2, protección del preview:** V3 exige pairing en `scripts/4_World/LFPG_SorterController.c:1781`, usa debounce/timeout en `:881` y descarta otra salida en `:1873`; V4 hace lo mismo en `scripts/4_World/test/LFPG_SorterController_TEST.c:1720`, `:881`, `:1774`. Ambos envíos comprueban juego y jugador. No se localizó una protección de preview presente sólo en una versión. La guarda V4 de energía `CanEdit` (`:934`) protege mutaciones, no esos envíos.

Se buscaron S10 y preview en `reviews/`, ambos controllers/views, parsers de respuesta, acciones y despacho servidor. El informe previo T0b ya advertía esta falta de localización; aquí se releyeron los métodos. Si la ficha quería decir correlación de respuestas, ese defecto está en **las dos** y es S12, que sobrevive.

**Coste: AMPLIO** para retirar la duplicación; no estimable como fix de una protección sin concretar. **Agrupación:** S11/S12/S20/S21/S23 en el conjunto UI; no contar el borrado como cura de esas fichas. Se conserva DUDOSA para la ficha completa, sin inventar un guard histórico ni una fecha de arreglo.

### S11 — VIVA — Rechazos sin ACK dejan Save/Sort bloqueados

**Evidencia:** V4 pone `m_SaveInFlight=true` en `scripts/4_World/test/LFPG_SorterController_TEST.c:1190` y `m_SortInFlight=true` en `:1234`; los libera en ACK (`:783`, `:807`) o nueva inicialización (`:486`). El servidor retorna sin ACK por rate-limit en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2654` y `:2733`, por distancia en `:2694`/`:2763`, y por pérdida de energía en Save en `:2701`.

Un rechazo normal deja el botón inutilizable hasta reabrir. `m_FeedbackTimer` sólo cambia el texto; el timeout de `TickTimers` que libera una petición en vuelo es exclusivamente de Preview (`scripts/4_World/test/LFPG_SorterController_TEST.c:881`). Un ACK negativo recibido sí desbloquea: no todos los rechazos provocan el defecto.

**V3/V4:** V3 mantiene el mismo esquema (`scripts/4_World/LFPG_SorterController.c:1092`, `:1144`, `:800`, `:824`). **Coste: AMPLIO**, completar respuestas terminales y recuperación cliente, coordinando su identificación con S12. **Agrupación:** Save con S01/S03; ACK y estado cliente con S09/S12/S24. No basta con reducir el debounce de Preview.

### S12 — VIVA — Respuestas sin identidad de petición/configuración

**Evidencia:** `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2711` sólo escribe el bool de Save; `:2780` escribe resultado/conteo de Sort; `:3034` inicia el payload de Preview con salida, total y cantidad, sin entidad ni revisión. `scripts/4_World/test/LFPG_SorterView_TEST.c:1531`, `:1545`, `:1560` delegan en el singleton abierto. `scripts/4_World/test/LFPG_SorterController_TEST.c:1774` sólo comprueba índice de salida.

Una respuesta tardía de A puede llegar tras abrir B; editar reglas o volver a la misma salida tampoco permite detectar una respuesta de una revisión anterior. Un ACK puede liberar el estado de una operación distinta o mostrar éxito en el panel equivocado. Es una carencia del contrato, aunque aquí no se ha medido el orden de entrega real.

**V3/V4:** los SubId distintos separan versiones, no entidades ni generaciones de panel; V3 también delega al singleton (`scripts/4_World/LFPG_SorterView.c:1295`). **Coste: AMPLIO**, cambiar petición/respuesta y validación cliente de forma coordinada. **Agrupación:** S11 en ciclo de petición; S09 en ACK; S13/S14 en Preview. No confundir los NetworkID de CONFIG_RESPONSE con estos payloads.

### S13 — DUDOSA — Coincidencias de filtros frente a simulación de routing

**Evidencia:** `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2942` toma sólo la salida seleccionada; `:2983` acepta todo para catch-all y `:2989` usa MatchesAnyRule. El routing real busca primero una salida con reglas y cable (`scripts/5_Mission/LFPG_SorterLogic.c:290`, `:317`) y sólo después el primer catch-all conectado (`:325`, `:340`).

**Lectura 1:** si Preview promete «qué saldría por esta salida al ordenar», es incorrecto: dos salidas con la misma regla muestran el mismo ítem, pero sólo gana la primera; catch-all muestra también ítems que una regla anterior absorberá. Tampoco se comprueba destino ni hueco disponible.

**Lectura 2:** si promete «qué ítems del origen cumplen los filtros de esta salida», el algoritmo realiza esa función y no es un defecto de routing; hace falta aclarar la presentación. La descripción «una regla» es inexacta: evalúa el OR de todas las reglas de esa salida (`scripts/5_Mission/LFPG_SorterLogic.c:470`).

**V3/V4:** endpoint común, misma diferencia semántica. **Coste: AMPLIO** para una simulación fiel; el alcance cambia si sólo se precisa el significado en UI. **Agrupación:** S02/S14 en el handler Preview y S07/S15 en routing. Se elige no imponer una promesa de producto ausente del brief.

### S14 — VIVA — Preview evita la política de acceso del sorter

**Evidencia:** `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2923` comprueba distancia jugador-sorter; `:2956` sólo exige contenedor y reglas antes de obtener cargo y enumerarlo (`:2976`). No llama a `CanTakeFromContainer`, que sí bloquea VSM/CodeLock/puertas en `scripts/5_Mission/LFPG_SorterLogic.c:56` y que usa el sort manual en `scripts/5_Mission/LFPG_NetworkManagerImpl.c:6440`.

El servidor puede enviar tipos/categorías/dimensiones/cantidades de un cargo que el propio sorter no permite extraer. La divulgación requiere que el contenedor bloqueado conserve cargo enumerable: un VSM cerrado que lo vacía no prueba esa explotación. No se afirma acceso global remoto; hay proximidad al sorter.

**Llamadores revisados:** el dispatcher reata el jugador al sender (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:54`) y enruta ambos previews a esta función (`:162`); eso no añade control de acceso al contenedor. La clasificación de políticas de hoy tampoco introduce aquí el guard omitido.

**V3/V4: compartida. Coste: ACOTADO**, aplicar a la lectura la política pertinente en el handler servidor, incluyendo el alcance por ítem si se necesita. **Agrupación:** S13/S02/S03/S12 en `HandleSorterPreviewRequest`; S15/S24 en guards de contenedor. Impacto potencial de información privada; no hay explotación in-game acreditada.

### S15 — VIVA — La validez espacial del vínculo no es simétrica

**Evidencia:** el tick compara sorter-origen y desvincula si supera el radio (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:6016`). El manual obtiene el origen sin esa comparación (`:6431`). `ResolveOutputContainer` obtiene el sorter destino y devuelve su vínculo sin validar distancia (`scripts/5_Mission/LFPG_SorterLogic.c:1007`, `:1012`); `LFPG_GetLinkedContainer` sólo exige que el NetworkID resuelva (`scripts/4_World/LFPG_Sorter.c:577`).

Mover un contenedor después de enlazarlo permite que las rutas manual/destino lo sigan usando fuera del radio, al menos hasta que otro camino lo invalide. Si el sorter destino no tiene energía, su propio tick se salta antes de comprobar la distancia (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:5995`): no hay garantía de que el tick lo cure a tiempo.

**Matiz:** destino sí comprueba acceso de escritura al mover (`scripts/5_Mission/LFPG_SorterLogic.c:1031`, `:1068`); no afirmo que falten todos los guards ni que recibir deba exigir energía. El defecto verificado es la distinta vigencia del vínculo según operación.

**V3/V4:** compartida/heredada. **Coste: AMPLIO**, establecer una validación consistente en origen, destino y operación manual. **Agrupación:** S06/S24 en resolución, S16/S17 en vínculo de entidad, S14 en acceso. Puede producir movimientos que contradicen el alcance físico del sorter.

### S16 — VIVA — NetworkID guardado como vínculo persistente

**Evidencia:** `scripts/4_World/LFPG_Sorter.c:334` guarda dos mitades de NetworkID y `:341` las restaura. `:228` las resuelve al iniciar; si obtiene una entidad no humana/no eléctrica con cargo (`:233`), la acepta y reclama en `:264`. No compara identidad persistente, tipo anterior, distancia ni propietario anterior. Si no resuelve un candidato válido, se enlaza al más cercano (`:251`), que tampoco garantiza ser el anterior.

El enlace carece de una prueba de identidad entre reinicios. La fuente pública del motor define NetworkID para la sesión y distingue un identificador persistente que permanece tras reiniciar: [Object, GetNetworkID, líneas 773–775](https://github.com/BohemiaInteractive/DayZ-Script-Diff/blob/main/scripts/3_game/entities/object.c#L773) y [EntityAI, GetPersistentID, líneas 3256–3258](https://github.com/BohemiaInteractive/DayZ-Script-Diff/blob/main/scripts/3_game/entities/entityai.c#L3256). La posibilidad de reutilización concreta y el orden efectivo de carga no se han reproducido; **VIVA se refiere al contrato inseguro, no a un desvío observado**.

**V3/V4:** `scripts/4_World/test/LFPG_Sorter_TEST.c:31` hereda exactamente ese almacenamiento y reenganche. **Coste: AMPLIO**, revisar contrato de identidad/reenganche y compatibilidad de datos guardados. Una alternativa conservadora sin ampliar formato sería no confiar automáticamente en el ID restaurado y requerir un vínculo verificable; cualquier solución debe preservar lectura legacy. **Agrupación:** mismo fichero y métodos de vínculo que S15/S17; setters de configuración de S01/S02 en la misma entidad. No resolverla borrando la clase base.

### S17 — VIVA — Búsqueda duplicada y attachment-only sin consumidor

**Evidencia:** `scripts/4_World/LFPG_Sorter.c:373` y `:480` implementan dos búsquedas cercanas; las condiciones `:409` y `:515` aceptan candidatos sin cargo si tienen attachments. La primera se usa en resync (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2832`) y la segunda en el reenganche (`scripts/4_World/LFPG_Sorter.c:251`).

Un soporte sólo de attachments puede ganar frente a un cargo algo más lejano y quedar anunciado como vinculado, pero el tick lo descarta (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:6050`), el manual devuelve error (`:6464`) y Preview no lo recorre (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2962`). El validador de reenganche además sólo reconoce cargo (`scripts/4_World/LFPG_Sorter.c:238`).

**V3/V4:** entidad común heredada. **Coste: ACOTADO**, unificar búsqueda y exigir cargo bajo el contrato actual; implementar movimiento de attachments sería AMPLIO y un cambio de alcance. **Agrupación:** S15/S16 en vínculo; S24 en duplicación. La corrección conservadora no debe inventar soporte attachment-only a partir de esa condición.

### S18 — DUDOSA — Dimensiones mediante rutas «itemSize 0/1»

**Evidencia local:** `scripts/5_Mission/LFPG_SorterLogic.c:938` fija raíz CfgVehicles; `:948` forma rutas con sufijos ` 0`/` 1` y `:950` llama a ConfigGetInt. Valores no positivos se convierten en 1 (`:954`) y se cachean por tipo (`:959`). Afecta SLOT (`:574`), repack (`:1157`) y Preview (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:3003`).

**Lectura 1, fallo de lectura nativa:** si esos sufijos se interpretan como subclases y no como índices, se acaba usando 1×1 para ítems mayores, alterando filtros y el plan de repack. La firma documentada lee un entero por ruta, mientras existe otra para arrays: [CGame, ConfigGetInt y ConfigGetIntArray, líneas 514 y 557](https://github.com/BohemiaInteractive/DayZ-Script-Diff/blob/main/scripts/3_game/global/game.c#L514). Es una hipótesis fundada, no una ejecución del método nativo.

**Lectura 2, deuda de validación:** si el motor admite indexación en esas rutas, ese defecto concreto no existe y la ficha sólo pide una prueba que falta. La declaración `proto native` no revela el parser interno. Hay un oráculo de dimensiones declarado en [CGame.GetInventoryItemSize, línea 867](https://github.com/BohemiaInteractive/DayZ-Script-Diff/blob/main/scripts/3_game/global/game.c#L867), pero no se ejecutó ni se verificó contra la versión local del juego.

Se buscaron ConfigGetIntArray, GetInventoryItemSize/GetItemSize e itemSize en el repo y las declaraciones oficiales; `P:/scripts` no estaba disponible. No se convierte «no encontré documentación de indexación» en «el motor devuelve cero». **V3/V4:** compartida. **Coste: ACOTADO** si la prueba confirma el problema, concentrado en GetItemSlotDimensions. **Agrupación:** S07 en caché/evaluación, S19 en SorterLogic, S13 en datos de Preview. La raíz fija CfgVehicles también debe incluirse en la comprobación con armas/cargadores; no se ha censado su configuración efectiva.

### S19 — VIVA — El traje ghillie se clasifica como accesorio

**Evidencia:** `scripts/5_Mission/LFPG_SorterLogic.c:748` comprueba GhillieSuit_ColorBase y retorna ATTACHMENT en `:750`; la comprobación Clothing_Base retorna CLOTHING después, en `:762`.

Los descendientes del traje ghillie toman la categoría de accesorio antes de llegar a ropa, afectando a la ruta por categoría y al texto del preview. El comentario «Wraps/lights» no cambia que se está comprobando el traje.

**V3/V4:** función compartida. **Coste: TRIVIAL**, corregir ese retorno de categoría. **Agrupación:** S07 en caché de categoría y S18/S24 en SorterLogic. Las cachés conservarían la clasificación durante una sesión ya iniciada; una prueba debe partir de una caché renovada.

### S20 — VIVA — Reconstrucción incluso sin cambio útil

**Evidencia V4:** `scripts/4_World/test/LFPG_SorterController_TEST.c:907` acepta seleccionar de nuevo la salida actual y llama a RefreshAll (`:920`); `:1360` re-resuelve botones y `:1381` reconstruye chips. `:1509` borra toda la colección y `:1538` crea nuevas vistas. Añadir un prefijo duplicado ignora el false de AddRule (`:1061`) y refresca igualmente (`:1066`). Preview también borra filas (`:1777`) y las crea de nuevo (`:1833`) sin comparar datos.

Esto acredita trabajo evitable ante entradas o respuestas iguales, pero no acredita cuánto cuesta ni que el rebind pueda suprimirse sin más. El comentario de `ReBindButtons` (`:1316`) documenta un workaround de Dabs; el guard de caché de widgets V4 (`:2180`) ya evita parte de las búsquedas y no invalida los ejemplos anteriores.

**V3/V4:** V3 conserva SelectOutput/RefreshAll (`scripts/4_World/LFPG_SorterController.c:907`, `:1273`), reconstrucción de tags (`:1596`) y filas (`:1932`). **Coste: ACOTADO** para evitar las actualizaciones sin cambios en el controller V4, conservando los rebind necesarios. **Agrupación:** S21 en granularidad, S23 en chips y S11/S12 en estado/respuestas. No reinstalar el antiguo pool sólo porque existan comentarios sobre él.

### S21 — VIVA — El rail V4 no se actualiza con las ediciones granulares

**Evidencia:** `scripts/4_World/test/LFPG_SorterController_TEST.c:1025`, `:1049`, `:1067`, `:1156` y `:1296` terminan en RefreshRulesDisplay. Ese método (`:1377`) refresca tags y contadores, pero no llama a RefreshRail_TEST. El único llamador de éste es RefreshAll (`:1355`); el número visible del rail se escribe en `:2036`.

Añadir o quitar reglas cambia el modelo y el contador principal, dejando el rail con el número anterior hasta un refresco completo, por ejemplo al seleccionar salida. Reset All sí pasa por RefreshAll; no está afectado por esa omisión concreta.

**V3/V4:** el rail es específico de V4 y permanece tras retirar V3; la UI V3 usa RefreshOutputTabs (`scripts/4_World/LFPG_SorterController.c:1276`). **Coste: TRIVIAL**, incorporar la actualización del rail en el camino granular común. **Agrupación:** S20 en RefreshRulesDisplay y handlers de edición. No confundir RefreshRuleCount (`:1595`) con la escritura del rail.

### S23 — VIVA — Estado y scaffolding de chips sin uso funcional

**Evidencia:** `scripts/4_World/test/LFPG_SorterTagView_TEST.c:17`/`:18` guardan índices, `:21` owner y `:30` color; se asignan en `:62`/`:68`/`:70` pero no se leen para ejecutar el borrado. La operación real codifica índices en el UID (`:106`) y los decodifica directamente en `scripts/4_World/test/LFPG_SorterView_TEST.c:1126`, llamando al controller actual en `:1132`.

La búsqueda de esos símbolos en scripts y layouts encontró declaraciones, escrituras, comentarios y la puesta de owner a null en el destructor, sin consumidor funcional. No se afirma una fuga por el comentario de «circular reference»: eso exigiría verificar ownership de Dabs. Los comentarios sobre reutilización/pool siguen contradiciendo la creación de vistas nuevas (`scripts/4_World/test/LFPG_SorterController_TEST.c:1501`, `:1538`).

**V3/V4:** V3 repite campos/asignaciones (`scripts/4_World/LFPG_SorterTagView.c:20`, `:64`, `:70`). **Coste: AMPLIO** para retirar también el parámetro owner del contrato SetData y limpiar su llamador; sólo campos/escrituras redundantes se concentran en el chip. **Agrupación:** S20 en creación de tags y S24 en estado redundante. Sigue siendo P3 de mantenimiento, sin daño al jugador demostrado.

### S24 — VIVA — Resoluciones y estado paralelos, con matices

**Evidencia:** `scripts/5_Mission/LFPG_NetworkManagerImpl.c:6066` resuelve las seis salidas para construir máscara y `:6174` vuelve a resolver la elegida por ítem; el manual repite el patrón (`:6486`, `:6552`). El estado de reanudación está repartido en cinco arrays (`:157`), que se insertan y eliminan en paralelo (`:5878`, `:5894`). Los guards CanTake/CanPut repiten VSM/lock/puerta (`scripts/5_Mission/LFPG_SorterLogic.c:56`, `:87`) y repack vuelve a comprobar acceso (`:1111`) después de hacerlo el manual (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:6440`).

La duplicación descrita existe, pero no prueba una avería: el estado paralelo se mantiene alineado en los caminos leídos, y leer/escribir necesitan checks distintos de CanReleaseCargo/CanReceiveItemIntoCargo. El riesgo verificable es trabajo repetido y mayor coste de mantener políticas coherentes; no se propone borrar guards por semejanza textual.

**V3/V4:** servidor compartido. La parte UI también conserva estado derivado, como pairing obtenido del nombre (`scripts/4_World/test/LFPG_SorterController_TEST.c:497`), con energía repollada aparte (`:960`); no se presupone que todos esos campos puedan fusionarse. **Coste: AMPLIO** para toda la ficha. **Agrupación:** S05/S06/S07 en tick, S09/S15 en sort manual, S14/S17 en política de acceso/vínculo, S23 en estado UI. Conviene separar esos alcances antes de un tramo de implementación.

## Ficha muerta y fecha

### S22 — MUERTA — Ya estaba muerta antes de hoy

La V4 actual crea su contexto propio en `scripts/4_World/test/LFPG_SorterView_TEST.c:1383` y captura a través de él (`:1384`). `CapturePanel` intercambia estado, llama al scaler y lo restaura (`:93`); `SwapState` cubre los cinco arrays y ambos flags (`:114`). Apply y Reset siguen el mismo patrón (`:100`, `:107`). Por ello el Clear de los arrays globales en `scripts/3_Game/LFPG_UIScaler.c:79` no borra la captura V3 durante una captura V4.

**Cronología comprobada en contenido:** `7e86ca80493ca5c7aebfa86912464a0e2b0c77aa`, del **28-08-2026**, retiró las llamadas V4 directas a Capture/Apply/Reset. El padre de `f247f5a` seguía sin ellas. **`f247f5a35fe384797b22e9f15d61f32e264740a4`, del 08-09-2026, las reincorpora mediante el contexto aislado.** El defecto original ya había muerto; hoy se evita reintroducirlo. No depende del borrado futuro de V3.

No se ha probado en el motor el comportamiento de referencias/herencia del contexto ni DPI y cambios de resolución. Eso limita la validación del port, pero no hace aparecer en el código actual la sobrescritura global que describía la ficha.

## LAS QUE RECOMIENDO ATACAR PRIMERO

1. **S15 — vínculo válido en todas las rutas.** Puede mover pertenencias a través de un enlace que ya debería estar fuera de alcance; el código demuestra la asimetría sin depender de un parser nativo desconocido. Revisar S16 en el mismo dominio antes de cerrar el tramo.
2. **S14 — lectura de contenedores bloqueados.** La omisión es de servidor y expone datos si el cargo sigue materializado. Priorizar un caso real de contenedor cerrado/bloqueado y aplicar el mismo límite de acceso que necesita ordenar.
3. **S11 — recuperación de Save/Sort rechazados.** Rechazos normales inutilizan operaciones básicas hasta reabrir. Diseñar la respuesta junto a S12 evita sustituir un bloqueo por aceptar ACK de otro panel.
4. **S01 — guardar el vaciado de reglas.** Impide al jugador conservar una configuración vacía y deja activas las reglas anteriores. Coordinar con S02 porque comparten exactamente el parser.
5. **S02 — parseo estructural y publicación atómica.** Evita interpretar mal salidas y consumir configuraciones parciales; Save ya protege la configuración activa, por lo que no se presupone pérdida de datos en cada fallo de parseo.

S16 merece validación temprana con reinicio y cambio de orden de carga por su posible impacto en identidad; no se coloca por delante de los efectos más directamente trazables sin haber observado reutilización de IDs. S05/S07/S20 requieren perfil para ordenar inversión por coste real.

## LO QUE NO PUDE VERIFICAR

- Compilación Enforce, carga de mundo ni ejecución de ningún ejemplo: no hay compilador/juego en el encargo.
- El diff final de la lane que retira V3, su reubicación de la entidad común, ni las líneas que resulten después de integrarlo.
- Qué protección concreta pretendía S10; no se localizó diferencia en pairing, debounce, timeout, envío o descarte por salida de los previews actuales.
- La promesa de producto de S13: coincidencias locales frente a simulación de routing y capacidad.
- S18: interpretación nativa de los sufijos 0/1 y dimensiones efectivas en la versión instalada; la fuente oficial consultada declara APIs, no expone su implementación nativa.
- S16: reutilización efectiva de NetworkID, orden de restauración de entidades y un caso reproducido de reenganche equivocado.
- S14: una divulgación efectiva con los mods concretos de cerraduras/almacenamiento; sí se verificó la omisión del guard en el emisor.
- Orden/latencia reales de respuestas, duración visible del inventario obsoleto tras repack y recuperación de UI en cliente multijugador.
- Rendimiento medido de S05/S07/S20/S24, ownership y rebind reales de Dabs, y escalado V3/V4 a distintas resoluciones/DPI.
- `CLAUDE.md` no existe en este checkout; tampoco estaba disponible `P:/scripts`. No se asumieron instrucciones de proyecto ni fuentes vanilla locales ausentes.

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

- **La lista de borrado mezcla UI V3 con la base viva de V4.** `scripts/4_World/test/LFPG_Sorter_TEST.c:31`, los casts del servidor y `config.cpp:1086` dependen de LFPG_Sorter. Borrar su fichero literalmente sin conservar/trasladar la clase dejaría dependencias sin definición. Ésta es una advertencia sobre el recorte anunciado, no una incidencia observada en el código actual.
- **S10 junta dos afirmaciones distintas.** La duplicación se elimina retirando V3; no se halló la divergencia específica de preview. No se declara MUERE-CON-V3 toda la ficha ocultando esa duda, ni se usa su duplicación para justificar que S11/S12/S20/S23 desaparecen.
- **S22 no murió esta madrugada:** el historial de código sitúa su eliminación original el 28 de agosto; el port de hoy incorpora protección para no revivirla. Las etiquetas de la auditoría no describen necesariamente el corte actual.
- **S02 no significa que todo Save inválido corrompa las reglas guardadas.** El setter ya usa candidato temporal. El defecto residual está dentro del parser y en los consumidores que siguen usando su estado tras false.
- **S07/S24 describen oportunidades de diseño/mantenimiento, no un fallo funcional acreditado.** Mantener S07 como P2 exige evidencia de coste; por ahora tiene más fundamento tratar ese trabajo como P3. S20 también debe distinguir trabajo redundante de los rebind necesarios por Dabs.
- **S13 mezcla preview de filtros con resultado de ejecución.** Para declarar bug de routing falta una promesa; tampoco evalúa literalmente una sola regla, sino todas las de la salida seleccionada.
- **S14/S16 no equivalen a exploits o desvíos reproducidos.** La ausencia de validación está viva; la realización del daño depende de almacenamiento/identidad/motor. Si S14 demuestra lectura de inventario privado con un contenedor usado en producción, habría base para revisar su P2 hacia P1; no se eleva sólo por la hipótesis.
- **S17 permite una solución menor que implementar attachments:** alinear la selección con el consumidor de cargo. Admitir un candidato no demuestra que el producto deba mover attachments.
- **El lote no forma un único cambio seguro.** Parser/ACK, política de vínculos, rendimiento y UI tienen contratos distintos; S07/S24 se solapan con S05/S06/S20/S23. La agrupación indicada por función evita contabilizar varias veces el mismo trabajo.

## Comprobaciones y cierre

- Inspección estática de las 22 fichas con contraste V3/V4 y búsquedas de consumidores, guards, refrescos y estado de reanudación. Las auditorías previas se usaron como pistas, no como prueba de vigencia.
- Cálculo independiente de longitud del JSON ASCII de S03: 2761 caracteres, entre ambos umbrales; no equivale a ejecutar FromJSON ni un RPC.
- Para S22 se leyó tanto el aislamiento actual como las eliminaciones/reintroducción reales en los commits citados.
- Único producto escrito por esta lane: **TRIAJE.md**. Sin modificaciones de código ni de índice, sin commit, conforme al brief.
- No se escribe memoria ni handoff fuera del workspace: el encargo limita expresamente las escrituras a este informe, que concentra evidencia, decisiones y pendientes.
