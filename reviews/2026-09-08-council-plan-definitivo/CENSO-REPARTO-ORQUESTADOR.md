# CENSO DEL ORQUESTADOR - reparto de las 92 fichas VIVA por fichero

Universo: columna "fichero principal" de las tablas de veredicto de los cinco
TRIAJE-*.md. Regex: ^\|\s*([A-Z]{1,4}\d{2})\s*\|\s*VIVA\s*\|\s*`([^`|]*)`

Sirve para adjudicar la disputa de la lane glm, que atribuyo 23 fichas a
LFPG_NetworkManagerImpl.c metiendo en el saco fichas de otros cuatro ficheros.

| n | fichero | fichas |
|---|---|---|
| 13 | `scripts/4_World/LFPG_CableRenderer.c` | R01 R03 R05 R06 R07 R08 R09 R10 R11 R20 R21 R22 R24 |
| 13 | `scripts/5_Mission/LFPG_NetworkManagerImpl.c` | G03 G05 G06 G07 G12 G13 G21 G22 G23 S05 S06 S24 SEC05 |
| 8 | `scripts/5_Mission/LFPG_ElecGraphImpl.c` | G08 G09 G10 G11 G14 G15 G19 G20 |
| 7 | `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c` | S03 S12 S14 SEC04 SEC06 SEC07 SEC17 |
| 3 | `scripts/3_Game/LFPG_FileUtil.c` | SEC10 SEC16 SEC19 |
| 3 | `scripts/4_World/LFPG_DeviceInspector.c` | R12 R13 R28 |
| 3 | `scripts/4_World/LFPG_RemoteController.c` | D08 D09 D23 |
| 3 | `scripts/4_World/test/LFPG_SorterController_TEST.c` | S11 S20 S21 |
| 3 | `scripts/5_Mission/LFPG_BTCSessionRegistry.c` | SEC12 SEC13 SEC14 |
| 3 | `scripts/5_Mission/LFPG_SorterLogic.c` | S07 S15 S19 |
| 2 | `scripts/3_Game/LFPG_SorterData.c` | S01 S02 |
| 2 | `scripts/4_World/LFPG_Furnace.c` | D14 D22 |
| 2 | `scripts/4_World/LFPG_IDevice.c` | D10 D21 |
| 2 | `scripts/4_World/LFPG_Intercom.c` | D11 D12 |
| 2 | `scripts/4_World/LFPG_LaserBeamRenderer.c` | R18 R19 |
| 2 | `scripts/4_World/LFPG_Sorter.c` | S16 S17 |
| 1 | `scripts/3_Game/LFPG_Migrators.c` | SEC18 |
| 1 | `scripts/3_Game/LFPG_Settings.c` | SEC11 |
| 1 | `scripts/4_World/LFPG_ActionDismantleDevice.c` | D06 |
| 1 | `scripts/4_World/LFPG_ActionUpgradeWaterPump.c` | D07 |
| 1 | `scripts/4_World/LFPG_BatteryAdapter.c` | D24 |
| 1 | `scripts/4_World/LFPG_CableHUD.c` | R27 |
| 1 | `scripts/4_World/LFPG_CableParticle.c` | R23 |
| 1 | `scripts/4_World/LFPG_Camera.c` | R14 |
| 1 | `scripts/4_World/LFPG_CameraViewport.c` | R29 |
| 1 | `scripts/4_World/LFPG_DoorController.c` | D13 |
| 1 | `scripts/4_World/LFPG_MotionSensor.c` | D17 |
| 1 | `scripts/4_World/LFPG_RPCClientHandler.c` | S09 |
| 1 | `scripts/4_World/LFPG_RPCGuard.c` | SEC15 |
| 1 | `scripts/4_World/LFPG_SearchlightController.c` | R17 |
| 1 | `scripts/4_World/LFPG_SwitchV2Remote.c` | D18 |
| 1 | `scripts/4_World/LFPG_TestDevices.c` | D19 |
| 1 | `scripts/4_World/lfpg_devicebase.c` | D15 |
| 1 | `scripts/4_World/test/LFPG_SorterTagView_TEST.c` | S23 |
| 1 | `scripts/5_Mission/LFPG_ControlSessionRegistry.c` | SEC08 |
| 1 | `scripts/5_Mission/LFPG_MissionInit.c` | R30 |
| 1 | `scripts/5_Mission/LFPG_TankHUD.c` | R16 |

**92 VIVA en 37 ficheros.** Los cuatro mayores suman 41; el resto van de 3 o menos.