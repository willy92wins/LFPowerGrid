# NOTES — LUNA EXEC L10

MODEL: grok-4.7

Base: `b0c94386e60044dadd24edf179839fcb29337331` (main no se ha movido).
Write-set: `scripts/4_World/LFPG_CameraViewport.c`, `scripts/4_World/LFPG_WiringClient.c`.
`LFPG_WiringClient.c` no se toca: no hizo falta para pasar el umbral y no hay un recorte honesto ahí que merezca mezclarse con este diff.

Heurística por línea: vacía = blanco; `//` o dentro de `/* */` = comentario; el resto = exec (`#if*` / `#endif` cuentan exec). El fichero que termina en newline no cuenta una línea vacía extra.

## Conteo

| fichero | | total | exec | coment | blanco | #if* | #endif |
|---|---|---:|---:|---:|---:|---:|---:|
| CameraViewport | antes | 1497 | 1080 | 237 | 180 | 2 | 2 |
| CameraViewport | después | 1443 | 1030 | 236 | 177 | 2 | 2 |
| WiringClient | antes = después | 1096 | 790 | 173 | 133 | 1 | 1 |

Exec del write-set: 1870 → 1820 (−50).
De esas −50, −20 son la compactación de formato (abajo) y **−30 son exec honestas**.
Comentario eliminado: 1. Blanco eliminado: 3. No se venden como exec.

## Compactación de formato (no cuenta para el umbral)

compactación de métodos vacíos: +10/−30

Reaplicado a mano el cambio de `0c48b3e` (PR #23), sin cherry-pick. Sigue siendo correcto sobre `b0c9438`: el stub `#ifdef SERVER` tiene los mismos 10 métodos de cuerpo vacío (`LFPG_CameraViewport`, `Reset`, `SafeAbort`, `EnterFromList`, `HandleKeyUp`, `CycleNext`, `CyclePrev`, `DoExitCleanup`, `Tick`, `DrawOverlay`). Firmas, orden y valores de retorno de los métodos no vacíos no cambian. Cada cuerpo

```
void Foo()
{
}
```

pasa a `void Foo() {}` (−3/+1 por método).

## Reducciones honestas (CameraViewport, dentro de `#ifndef SERVER`)

1. **dead** — `m_ScanlineOffset` + `LFPG_CCTV_SCROLL_SPEED` + el avance en `Tick`. Delta exec **−12**. DrawOverlay dibuja las scanlines desde `lineY = 0` y no lee el offset; el campo solo se escribía. `LFPG_CCTV_SCANLINE_SPACING` se queda: lo usa `DrawOverlay`.
   Evidencia (0 lecturas; solo escrituras en este fichero):

```
git grep -n -e m_ScanlineOffset -e LFPG_CCTV_SCROLL_SPEED b0c94386e60044dadd24edf179839fcb29337331 -- '*.c' '*.cpp' '*.layout' '*.xml'
b0c94386e60044dadd24edf179839fcb29337331:scripts/4_World/LFPG_CameraViewport.c:49:static const float LFPG_CCTV_SCROLL_SPEED     = 20.0;
b0c94386e60044dadd24edf179839fcb29337331:scripts/4_World/LFPG_CameraViewport.c:86:    protected float     m_ScanlineOffset;
b0c94386e60044dadd24edf179839fcb29337331:scripts/4_World/LFPG_CameraViewport.c:159:        m_ScanlineOffset = 0.0;
b0c94386e60044dadd24edf179839fcb29337331:scripts/4_World/LFPG_CameraViewport.c:460:        m_ScanlineOffset = 0.0;
b0c94386e60044dadd24edf179839fcb29337331:scripts/4_World/LFPG_CameraViewport.c:877:        m_ScanlineOffset = 0.0;
b0c94386e60044dadd24edf179839fcb29337331:scripts/4_World/LFPG_CameraViewport.c:973:        m_ScanlineOffset = 0.0;
b0c94386e60044dadd24edf179839fcb29337331:scripts/4_World/LFPG_CameraViewport.c:1072:            m_ScanlineOffset = 0.0;
b0c94386e60044dadd24edf179839fcb29337331:scripts/4_World/LFPG_CameraViewport.c:1268:        m_ScanlineOffset = m_ScanlineOffset + (LFPG_CCTV_SCROLL_SPEED * timeslice);
b0c94386e60044dadd24edf179839fcb29337331:scripts/4_World/LFPG_CameraViewport.c:1269:        while (m_ScanlineOffset >= LFPG_CCTV_SCANLINE_SPACING)
b0c94386e60044dadd24edf179839fcb29337331:scripts/4_World/LFPG_CameraViewport.c:1271:            m_ScanlineOffset = m_ScanlineOffset - LFPG_CCTV_SCANLINE_SPACING;
```

2. **branch** — el parpadeo REC (`if (m_RecVisible) false; else true`) pasa a `m_RecVisible = !m_RecVisible`. Delta exec **−7**. El `Show` siguiente no cambia.
3. **branch** — `anyPan` se asigna directo con `(m_KeyA || m_KeyD || m_KeyW || m_KeyS)`, mismo orden de teclas. Delta exec **−4**.
4. **dedupe** — `CycleNext` / `CyclePrev` comparten `CycleBy(int step)`. Firmas públicas iguales. Orden: guardas, `CommitCurrentAim(true, true)`, índice, `EnterCamera`, `UpdateOverlayLabel`, `ShowCycleMessage`. `step = 1` envuelve por arriba; `step = -1` envuelve por abajo; el otro límite no dispara con el índice en rango. Delta exec **−7**.

Suma honesta: −12 −7 −4 −7 = **−30**.

## Preprocesador

CameraViewport `#if*`/`#endif` 2=2 antes y después. WiringClient 1=1, sin editar.
No se movió código entre ramas. `#ifndef SERVER` sigue siendo la clase cliente; `#ifdef SERVER` sigue siendo el stub (mismos miembros, mismas firmas).
Con `SERVER` definido: entra el stub; los 10 métodos vacíos siguen vacíos. Sin `SERVER`: entra la clase cliente y el stub queda fuera.

## Riesgo residual / no verificado

- No hay compilador Enforce en este entorno y no se arrancó el juego. El linter offline del Knowledge Pack (`script_validator.py`) no está en esta máquina.
- Quitar `m_ScanlineOffset` cambia el layout del objeto cliente. No es classname de `CfgVehicles` y no se serializa. Un mod externo que leyera el campo protected se rompería; en este repo no hay lector.
- `CycleBy` añade un segundo clamp que las dos llamadas no alcanzan. El índice resultante coincide con el de cada método original.
- `AGENTS.md` §5 pide no commitear. El brief de este hop manda commitear la rama y el PR draft; se sigue el brief.
