# Inventario de ramas congelado antes del cierre — 2026-09-26

Se escribe **antes** de borrar cualquier rama. Cada fila lleva el SHA completo: mientras
este fichero exista, ninguna rama borrada es irrecuperable (`git branch <n> <sha>`).

Columna **contenida en**: si otra rama la contiene, su ref es redundante y borrarla no
pierde ni un commit. Columna **patch en main**: `git cherry main <rama>` — para las ramas
que entraron por squash-merge, el commit original no esta en main pero su contenido si.

## LFPowerGrid (mod compilable)

`main` = `e87e8872acc0f44d5480268f320b2e3cfee3502b`

Ramas vivas en `origin`: `chore/luna-reduce-loc-l02`, `chore/luna-reduce-loc-l05`, `chore/luna-reduce-loc-l10`, `main`

| rama | SHA | fecha | patch en main | contenida en | PR | ultimo commit |
|---|---|---|---|---|---|---|
| `chore/lfpg-b1-dead-guards-perfdiag` | `55037fc16f8011b5fb23988f24da538b4e7ab7a5` | 2026-09-25 | si (1/1) | **TIP** | #41 MERGED | chore(perfdiag): drop 9 impossible #ifndef SERVER blocks and 22 orphan |
| `chore/lfpg-b11-snapshot-combine` | `daf803ddfea7af00e96520383680988d8df4f229` | 2026-09-25 | si (1/1) | **TIP** | #40 MERGED | chore(luna): combine pending/deferred owner snapshot merge (B11) |
| `chore/lfpg-b4-lamps-base` | `caf36ea0381ba32b5616db83b22821ea6bfde5ff` | 2026-09-25 | si (1/1) | **TIP** | #44 MERGED | chore(luna): factor lamp lifecycle into LFPG_LampDeviceBase (B4) |
| `chore/lfpg-b7-btc-emitters` | `2babe1187cfebf9267c0d0bb532ad49abbe00f8f` | 2026-09-25 | si (1/1) | **TIP** | #43 MERGED | chore(btc): unify client BTC mutation RPC emitters (B7) |
| `chore/lfpg-b9-port-worldpos` | `0ce9fd1193da8fdd29482c54ff0ebe86191d15fb` | 2026-09-25 | si (1/1) | **TIP** | #42 MERGED | chore(luna): shared port world-pos helper on WireOwnerBase (B9) |
| `chore/lfpg-p0-preproc-balance` | `39387f97ce92864406f74a09aa7643dcf09e5084` | 2026-09-25 | si (1/1) | **TIP** | #39 MERGED | chore(ci): add PREPROC-BALANCE check to enforce_checks |
| `chore/luna-diferidas-go14` | `ca482ddd579adcfcf4ec7f3b2ec20d7bbd0d0348` | 2026-09-23 | si (1/1) | **TIP** | #34 MERGED | chore(luna): Diferidas Sol-GO reduce executable LOC (14) |
| `chore/luna-reduce-loc-l01` | `b6b371fe4158e337a1295ec4719f7e1bbe493553` | 2026-09-23 | si (1/1) | **TIP** | #14 MERGED | chore(luna): reduce LOC L01 (draft) |
| `chore/luna-reduce-loc-l02` | `70529c10d18ec3615e4bbc9744acfaffef284f93` | 2026-09-23 | no | **TIP** | #15 CLOSED | chore(luna): reduce LOC L02 (draft) |
| `chore/luna-reduce-loc-l03` | `daad11b21ab2094ff9fa5b07954cfcd33d5eeedf` | 2026-09-23 | si (1/1) | **TIP** | #16 MERGED | chore(luna): reduce LOC L03 (draft) |
| `chore/luna-reduce-loc-l04` | `89393b0bab662bff17da3508d90ee8eac991883c` | 2026-09-23 | si (1/1) | **TIP** | #17 MERGED | chore(luna): reduce LOC L04 (draft) |
| `chore/luna-reduce-loc-l05` | `176d316b1b309c087bbb86f510fc8742e65cbda0` | 2026-09-23 | no | **TIP** | #18 CLOSED | chore(luna): reduce LOC L05 (draft) |
| `chore/luna-reduce-loc-l06` | `491f3632bbb61419069adf626d47496263351be8` | 2026-09-23 | si (1/1) | **TIP** | #19 MERGED | chore(luna): reduce LOC L06 (draft) |
| `chore/luna-reduce-loc-l07` | `2fa96670340441108589556836ca50b80835e4f9` | 2026-09-23 | si (1/1) | **TIP** | #20 MERGED | chore(luna): reduce LOC L07 (draft) |
| `chore/luna-reduce-loc-l08` | `73c4795b61afbbb29ddf82f9d9d3fc7733e1805a` | 2026-09-23 | si (1/1) | **TIP** | #21 MERGED | chore(luna): reduce LOC L08 (draft) |
| `chore/luna-reduce-loc-l09` | `7837f229b1d7d44e67d5903fba443cc40489aba7` | 2026-09-23 | si (1/1) | **TIP** | #22 MERGED | chore(luna): reduce LOC L09 (draft) |
| `chore/luna-reduce-loc-l10` | `0c48b3e2a6aebdbc493952d93fc1277b8f304651` | 2026-09-23 | no | **TIP** | #23 CLOSED | chore(luna): reduce LOC L10 (draft) |
| `chore/luna-reduce-loc-l11` | `3038f52f2b0730d2db910472fe07a8007ef68f92` | 2026-09-23 | vacia | main | #24 MERGED | chore(luna): reduce LOC L11 (r2) |
| `chore/luna-reduce-loc-l12` | `4f20536b5e873578f3be13e2921568be52a81459` | 2026-09-23 | vacia | main | #25 MERGED | chore(luna): reduce LOC L12 (r2) |
| `chore/luna-reduce-loc-l13` | `9d3f08a5c6d48457051b90cfd924f4ec920247a3` | 2026-09-23 | vacia | main | #26 MERGED | chore(luna): reduce LOC L13 (r2) |
| `chore/luna-reduce-loc-l14` | `0ad66fb9438d119f1f8f7b87f206ad65d7ce7c92` | 2026-09-23 | vacia | main | #27 MERGED | chore(luna): reduce LOC L14 (r2) |
| `chore/luna-reduce-loc-l15` | `89bd3c55e6802574d03279c93ae8fe4591308a15` | 2026-09-23 | vacia | main | #28 MERGED | chore(luna): reduce LOC L15 (r2) |
| `chore/luna-reduce-loc-l16` | `d24691149b8c037032132f7069e45cbff4406814` | 2026-09-23 | vacia | main | #29 MERGED | chore(luna): reduce LOC L16 (r2) |
| `chore/luna-reduce-loc-l17` | `02fc87e63b3cbb5be7d6b65c3e90829df6eca418` | 2026-09-23 | vacia | main | #30 MERGED | chore(luna): reduce LOC L17 (r2) |
| `chore/luna-reduce-loc-l18` | `47e72500504030cd65b03615f9e29454b146effe` | 2026-09-23 | vacia | main | #31 MERGED | chore(luna): reduce LOC L18 (r2) |
| `chore/luna-reduce-loc-l19` | `2ff2e68fdc3f9c54e49439cd61598c851271da96` | 2026-09-23 | vacia | main | #32 MERGED | fix(luna): restore LFPG_GetSwitchState on Furnace (Sol #32) |
| `chore/luna-reduce-loc-l20` | `b7308288e92b61f7377d3cbca99ede72d13b868d` | 2026-09-23 | vacia | main | #33 MERGED | chore(luna): reduce LOC L20 (r2) |
| `feat/heater` | `726a48a5dc6ea95d817962e7095ee1c518355e36` | 2026-09-21 | no | **TIP** | — | Calefactor: el asset (modelo, texturas y materiales) |
| `fix/1.2.5-decididos` | `0a22f6159eae32847810ecbd97eb16a7e9923fd4` | 2026-09-15 | no | `feat/heater`, `integ/1.2.5`, `integ/1.2.5-eg1` (+32) | — | L0 1.2.5: tope de 16 cámaras, cargador a 1 u/s y salidas huérfanas de  |
| `integ/1.2.5` | `6d849acd4f1b4cd06dae212534fa28f642a4c7f2` | 2026-09-15 | no | `feat/heater`, `integ/1.2.5-eg1`, `integ/1.2.5-f07cur` (+31) | — | Merge lane/l0b-cctv-registry en la integración 1.2.5 |
| `integ/1.2.5-eg1` | `62a5eac24bb60d75fef310dacd71b13cc494b7f6` | 2026-09-15 | no | `feat/heater`, `integ/1.2.5-f07cur`, `integ/1.2.5-f8` (+24) | — | Merge lane/eg1-f04 en la integración de la 1.2.5 |
| `integ/1.2.5-f07cur` | `959c01d1d5820a881e281712dbd3e8c400dd1130` | 2026-09-16 | no | `feat/heater`, `integ/cut-lanes`, `lane/cut-a` (+15) | — | Merge lane/f07-cursor-code en la integración de la 1.2.5 |
| `integ/1.2.5-f8` | `a6e600f2e2e88ae7e9a1ddfedfca48961cf3a79a` | 2026-09-16 | no | `feat/heater`, `integ/1.2.5-f07cur`, `integ/1.2.5-nm2a` (+20) | — | Merge lane/f8-ambiguous en la integración de la 1.2.5 |
| `integ/1.2.5-nm1` | `68ee29dbd0a73a8afc8b9fa3a5015f1c4fc814c6` | 2026-09-16 | no | `feat/heater`, `integ/1.2.5-f07cur`, `integ/1.2.5-f8` (+22) | — | Merge lane/nm1 en la integración de la 1.2.5 |
| `integ/1.2.5-nm2a` | `15e9bd74364050423cbdfeafbffd76711b8a12ca` | 2026-09-16 | no | `feat/heater`, `integ/1.2.5-f07cur`, `integ/1.2.5-nm2b` (+19) | — | Merge lane/nm2a en la integración de la 1.2.5 |
| `integ/1.2.5-nm2b` | `5d84e80c499eaf928b784532327224713c54e7cc` | 2026-09-16 | no | `feat/heater`, `integ/1.2.5-f07cur`, `integ/cut-lanes` (+17) | — | Merge lane/nm2b en la integración de la 1.2.5 |
| `integ/1.2.5-r9f` | `946a4ff6b265f4d7fe3b352cecb94554d222aa1a` | 2026-09-15 | no | `feat/heater`, `integ/1.2.5-eg1`, `integ/1.2.5-f07cur` (+25) | — | Merge lane/r9f-b en la integración R9F de la 1.2.5 |
| `integ/cut-lanes` | `6d6ead8b496db895349acef10644ebc94fda23c8` | 2026-09-17 | no | `feat/heater`, `lane/fixes-20260918`, `lane/kits-burnable` (+2) | — | Integrar lane/cut-intercom (Codex gpt-5.6-sol, revision Anthropic) |
| `lane/cl1-inspector` | `c1913e337ff515a27c14de8ee0a87d0ef665990e` | 2026-09-15 | no | `feat/heater`, `integ/1.2.5`, `integ/1.2.5-eg1` (+32) | — | CL1 1.2.5 (C4): el inspector no resuelve el objetivo en cada frame con |
| `lane/cut-a` | `429c70d25404c7c1a620c353e53d93dcfe751e94` | 2026-09-17 | no | `feat/heater`, `integ/cut-lanes`, `lane/cut-acc` (+12) | — | Devolver a las fachadas los nueve metodos que llama la mision |
| `lane/cut-acc` | `e104507ad9421c40f137cff9e52904745e6d4b64` | 2026-09-17 | no | `feat/heater`, `integ/cut-lanes`, `lane/cut-door` (+11) | — | Document server action migration and offline gate evidence |
| `lane/cut-c` | `fd0676b0b60551ef93d2b098df20071b47ddda5b` | 2026-09-17 | no | `feat/heater`, `integ/cut-lanes`, `lane/cut-a` (+13) | — | Recorte C: mover a 5_Mission lo que World no usa (20,8 kB) |
| `lane/cut-door` | `c1392bbb1401946086084ca0c3e38c8eeed0a1ca` | 2026-09-17 | no | `feat/heater`, `integ/cut-lanes`, `lane/cut-door-codex` (+8) | — | Integrar el recorte pub: World 11.765 -> 11.752 kB, margen +80 -> +93 |
| `lane/cut-door-codex` | `0efa98c810b721d4db743d0aefbb9c2e3d5817bb` | 2026-09-17 | no | `feat/heater`, `integ/cut-lanes`, `lane/fixes-20260918` (+3) | — | docs: add door relocation report |
| `lane/cut-ent` | `fd9b1461b137be7ad1e9f28230af495c43056278` | 2026-09-17 | no | `feat/heater`, `integ/cut-lanes`, `lane/cut-door` (+10) | — | docs: record entity extraction budgets and validation gates |
| `lane/cut-furnace` | `f218b0f8890f69197f89784337ffa978e5a9fd69` | 2026-09-17 | no | `feat/heater`, `integ/cut-lanes`, `lane/cut-furnace-codex` (+4) | — | refactor: move LFPG_ToggleFurnace server body to Mission |
| `lane/cut-furnace-codex` | `958763cf59b1df784e993f8fb9341e178a6895b4` | 2026-09-17 | no | `feat/heater`, `integ/cut-lanes`, `lane/fixes-20260918` (+3) | — | Lane furnace: los nueve metodos restantes a 5_Mission |
| `lane/cut-intercom` | `9e53982b01e17cb56b36e9ad38a6fa6a9abc51be` | 2026-09-17 | no | `feat/heater`, `integ/cut-lanes`, `lane/cut-intercom-codex` (+4) | — | refactor: move LFPG_SpawnGhostRadio server body to Mission |
| `lane/cut-intercom-codex` | `9c6749187c06e775e206ec8a2fd49cacee4b736b` | 2026-09-17 | no | `feat/heater`, `integ/cut-lanes`, `lane/fixes-20260918` (+3) | — | Lane intercom: los quince metodos restantes a 5_Mission |
| `lane/cut-pub` | `98801db8fa6d5abb8551a8e31ffb7eab593b11ad` | 2026-09-17 | no | `feat/heater`, `integ/cut-lanes`, `lane/cut-door` (+9) | — | Revertir el guard de LFPG_InspectWireEntry: el servidor SI la usa |
| `lane/eg1-f04` | `45858715520cba87f29910459f818b131ddef09c` | 2026-09-15 | no | `feat/heater`, `integ/1.2.5-eg1`, `integ/1.2.5-f07cur` (+25) | — | EG1 1.2.5: F04 saca el crédito de los BatteryCharger del lote de valid |
| `lane/f07-cursor-code` | `f5d642e716330c19e1f7740e33b3480f8f55ff8c` | 2026-09-16 | no | `feat/heater`, `integ/1.2.5-f07cur`, `integ/cut-lanes` (+16) | — | F07: la pasada manual del sorter reanuda desde el cursor y ancla al to |
| `lane/f8-ambiguous` | `1d5d2cfa513fc6b8db1bd268c39b5f7cdbd225a3` | 2026-09-16 | no | `feat/heater`, `integ/1.2.5-f07cur`, `integ/1.2.5-f8` (+21) | — | F8: un deviceId ambiguo deja de contar como dispositivo inexistente al |
| `lane/fixes-20260918` | `65b1cdc304c1c2fe4c2feef77596b28703ef8da4` | 2026-09-18 | no | **TIP** | — | Horno: los kits se reconocen por su clase base, no por una lista de no |
| `lane/kits-burnable` | `44cfb86bb2006248894d8ef87c1df04ead162308` | 2026-09-18 | no | `feat/heater`, `lane/pr1-onliners`, `lane/pr4-estado` | — | Horno: todos los kits de LFPG se pueden quemar como combustible |
| `lane/l0b-cctv-registry` | `62677649d75d9f17246181bf30287cf7e205dbfd` | 2026-09-15 | no | `feat/heater`, `integ/1.2.5`, `integ/1.2.5-eg1` (+32) | — | L0b 1.2.5: la salida de CCTV del servidor no devuelve al jugador a un  |
| `lane/nm1` | `1860efbf9fcd2d051abadb6f3b5fc1817b70ec57` | 2026-09-16 | no | `feat/heater`, `integ/1.2.5-f07cur`, `integ/1.2.5-f8` (+23) | — | NM1: H10, F05(a) con la opción B de #20, S7 perezoso y U3, con interru |
| `lane/nm2a` | `424dceb21c4fc9ccb1d769789210a5d3253e7a2a` | 2026-09-16 | no | `feat/heater`, `integ/1.2.5-f07cur`, `integ/1.2.5-nm2a` (+20) | — | NM2a: índice de celdas por clave entera con consulta híbrida (F06+S6)  |
| `lane/nm2b` | `318bed89c7d5476100a11a35890eae44e27d509c` | 2026-09-16 | no | `feat/heater`, `integ/1.2.5-f07cur`, `integ/1.2.5-nm2b` (+18) | — | NM2b: orden manual del sorter con presupuesto (F07) y tick de disposit |
| `lane/pr1-onliners` | `2c151bf089504b20980434a8b0351b76bb3d54d6` | 2026-09-20 | no | `lane/pr4-estado` | — | PR-2 grupos B y C: 54 accesores de kit pasan a campos de sus dos bases |
| `lane/pr4-estado` | `f1ec832fa589f7bdb17eb09f2b74aaacd1811d01` | 2026-09-21 | no | **TIP** | — | Revision PR-3, P3: tres de las ocho aperturas de visibilidad sobraban |
| `lane/r9f-a` | `5b16a1eb22d1e747a4fe02d9e08970c66e69b504` | 2026-09-15 | no | `feat/heater`, `integ/1.2.5-eg1`, `integ/1.2.5-f07cur` (+26) | — | R9F-A 1.2.5: el libro Native de claims reembolsa en lote, retira compr |
| `lane/r9f-b` | `cf1cfa4e1e238c02fa04e26007c799c69b4dabda` | 2026-09-15 | no | `feat/heater`, `integ/1.2.5-eg1`, `integ/1.2.5-f07cur` (+26) | — | R9F-B 1.2.5 R2: las instrucciones al admin sobre el marcador de venta  |
| `lane/r9f-c` | `6b820283c5f23ab07bc716fe5e34192a6e611c4a` | 2026-09-15 | no | `feat/heater`, `integ/1.2.5-eg1`, `integ/1.2.5-f07cur` (+26) | — | R9F-C 1.2.5: ficheros atómicos verificados, marcador de venta con side |
| `rc/1.2.5-b` | `0fc8c018e78210ee8334dca8fecd9993430c12ad` | 2026-09-16 | no | **TIP** | — | RC-B: candidato de sentada (NO fusionar a main) |

## LFPowerGrid_dev

`main` = `bcad0105b9622a60b937bed0f3f05c6ca1ed015b`

| rama | SHA | fecha | patch en main | contenida en | PR | ultimo commit |
|---|---|---|---|---|---|---|
| `lane/f07-cursor-model` | `9bb3167c71e48bf155cd9e0bf0716514887ac736` | 2026-09-16 | vacia | main | — | Modelo de F07: la pasada manual reanuda desde el cursor guardado y rev |
| `lane/f07cap-model` | `c50140c0c08d8f25f3d9d2e907b9e36ed9a0c531` | 2026-09-16 | vacia | main | — | Modelo de F07: ancla al agotar el tope, y la ventana se cuenta por ite |
| `lane/hx1-checkers` | `631fa2d182d1c0f6dd8d9b3e219e407a57df8632` | 2026-09-15 | vacia | main | — | HX1: comprobadores estáticos para los gates de la 1.2.5 (barrido de mi |
| `lane/hx2-models` | `7f6ca7e38213508ae433859b55bcd335da9e42d9` | 2026-09-15 | vacia | main | — | HX2: modelos offline de paridad para F04, F05(a), S7 y F06/S6 |
| `lane/hx2b-s7-lazy` | `14d5796610925cd51b2aa7e997e47400860218da` | 2026-09-16 | vacia | main | — | HX2b: instantánea perezosa de jugadores en el modelo de S7 (decisión # |
| `lane/hx3-fixture` | `c29fa20635ff67a326c8a0bf2677949d1822c0c9` | 2026-09-15 | vacia | main | — | HX3a: fixture_grid del arnés GS-02 con limpieza garantizada y oráculo  |
| `lane/hx3b-fix` | `5ca2ba17e8051ab522fd3a82c85cb66abfa1d314` | 2026-09-16 | vacia | main | — | HX3b-fix: con el modelo de F14 apagado, el alta y la baja dejan de man |
| `lane/hx3b-models` | `5e4b5ebf3888fd09bd9715a862836bb5835c8b2c` | 2026-09-16 | vacia | main | — | HX3b: modelos de paridad del sorter manual (F07) y del tick por fases  |
| `lane/hx3c-smoke-power` | `34472f70f137250e6c6205c1c6891afbb195b372` | 2026-09-16 | vacia | main | — | HX3c: perfil de potencia viable para el smoke de fixture_grid |

