# ENCARGO — revisión adversarial de un parche de integridad monetaria

## 1. Qué es esto
Otro modelo, de otra familia, acaba de escribir **627 líneas** sobre el subsistema de dinero de un
mod de DayZ. Tu trabajo es **intentar romperlo**, no confirmarlo. El código va a un servidor que
**tiene jugadores conectados ahora mismo**, y **nadie lo ha compilado todavía**.

Un revisor que dice «me parece bien» no ha hecho el trabajo. Si de verdad no encuentras nada,
dilo, pero solo después de haber buscado en los sitios que se listan abajo.

## 2. Entorno — no heredas nada
- Repo en `P:\LFPowerGrid`. **Enforce Script de DayZ**, ficheros `.c`. **No es C ni C++.**
- Rama activa `fix/t1-integridad-monetaria`, con los cambios **sin commitear en el árbol**.
- Los scripts vanilla de DayZ, para comprobar firmas, están en `P:\scripts`.
- **PROHIBIDO:** editar cualquier fichero, ejecutar git, compilar, empaquetar, lanzar DayZ.
  **Solo lectura.** Tu entregable es texto.

## 3. Lo que tienes que leer
1. `reviews/2026-09-06-council-auditorias/IMPL-T1-INFORME.md` — lo que el implementador dice que hizo.
2. `reviews/2026-09-06-council-auditorias/T1-fichas.md` — las fichas de auditoría originales.
3. El diff real. Ficheros tocados:
   - `scripts/5_Mission/LFPG_BTCHelper.c`
   - `scripts/3_Game/LFPG_FileUtil.c`
   - `scripts/3_Game/LFPG_BTCConfig.c`
   - `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c`

**No te fíes del informe.** Está escrito por quien hizo el cambio. Contrástalo contra el código.

## 4. Dónde buscar, por orden de daño

### (a) ¿Compila? — lo primero, porque nadie lo ha comprobado
Enforce es estricto y un error tumba el mod entero al arrancar. Busca en lo **añadido**:
- variables usadas sin declarar, o declaradas dos veces en el mismo ámbito;
- `ref` mal puesto (Enforce lo exige en miembros que poseen, no en locales);
- tipos que no casan en asignación o en argumento;
- métodos llamados con aridad o tipo equivocados — **verifica cada firma con grep**;
- `#ifdef SERVER` / `#ifndef SERVER` desbalanceados, o código de servidor llamado desde cliente;
- literales de cadena con dos secuencias de escape juntas: rompe la compilación entera con
  `CParser: quoted string not closed`. La regla está documentada en
  `scripts/4_World/test/LFPG_SorterView_TEST.c:2073-2076`.
- Convenciones de la casa: cero ternarios, nada de `++`/`--`/`+=`/`-=`/`foreach`.

### (b) ¿Cada arreglo cierra de verdad su ficha?
Ficha por ficha (E04, SEC09, E16, E02, E03, E08, E05): sigue el camino de ejecución nuevo y
comprueba si el escenario que describe la ficha sigue siendo posible. Si sigue siéndolo, **dilo con
el camino concreto**.

### (c) ¿Ha creado una vía NUEVA de pérdida o de duplicación?
Es el riesgo mayor de reordenar operaciones de dinero. Para cada camino de error nuevo, pregunta:
- ¿queda algo destruido sin su contrapartida persistida?
- ¿queda algo acreditado dos veces?
- ¿hay un `return` de error que deja el estado a medias?
- si una **reversión** falla (`RemoveBalance` que no persiste, restitución que no cabe en el
  inventario), ¿qué queda? El implementador admite bordes aquí: **verifícalos y busca los que no admite.**

### (d) ¿Se ha debilitado la frontera de confianza?
El servidor es la autoridad. Marca cualquier sitio donde ahora se acepte una cantidad, un saldo o
una identidad que venga del cliente, o donde un error se haya convertido en éxito silencioso.

### (e) Regresiones de comportamiento sobre un servidor vivo
Cambios que dejen de funcionar cosas que hoy funcionan. Presta atención especial a la validación
nueva del catálogo de monedas en `LFPG_BTCConfig.c`: **una sola entrada mala invalida el catálogo
entero y todas las operaciones de efectivo fallan cerradas.** ¿Qué más tiene esa forma?

## 5. Cómo entregar
Escribe `reviews/2026-09-06-council-auditorias/REVIEW-T1.md`. Por hallazgo:

```
### [BLOQUEANTE | GRAVE | MENOR] <título de una línea>
- **Dónde:** path:line
- **Qué veo:** el fragmento
- **Por qué falla:** el camino concreto de ejecución, con entradas concretas
- **Confianza:** alta | media | baja, y qué te falta para subirla
```

`BLOQUEANTE` = no puede llegar a un servidor con jugadores (no compila, pierde dinero, tumba una
función). `GRAVE` = hay que arreglarlo pero no bloquea. `MENOR` = mejora.

Termina con:
- `## VEREDICTO` — una línea: ¿puede esto desplegarse a un servidor con jugadores, sí o no?
- `## LO QUE NO PUDE VERIFICAR` — sin esta sección la revisión no vale.

No propongas parches largos. Señala el defecto y su sitio; arreglarlo es de otro.
