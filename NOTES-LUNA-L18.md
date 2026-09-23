# Notas de cambio — Luna L18

- Archivo: `scripts/4_World/LFPG_DoorController.c`.
- Base: `033c08c986987c50c5983d37811b0222f4cb3d05`.
- LOC fisicas: 899 antes, 861 despues; delta neto **-38**.
- Cambio: se quitaron 26 lineas de dos bloques de perfdiag condicionados por `#ifndef SERVER` desde `LFPG_DoSearchAndPair`, que esta bajo `#ifdef SERVER`; tambien sus dos campos y las escrituras/contador que solo les daban datos. Se aplanaron las condiciones Fence de abrir/cerrar, conservando cast, guardas de estado, llamadas y mensajes.
- Riesgo residual: no hay verificacion in-game ni compilador Enforce disponible en este worktree. El linter offline del proyecto es el gate aplicado aqui.
- Alcance: solo el archivo autorizado mas `PROPOSALS-LUNA-L18.md` y estas notas solicitadas para el PR.
