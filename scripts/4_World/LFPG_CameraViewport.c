// =========================================================
// LF_PowerGrid - CCTV Viewport Manager (v0.9.0 - Etapa 3)
//
// Singleton CLIENT-ONLY. Gestiona la transicion al POV de una
// camara de seguridad vinculada a un LF_Monitor.
//
// Flujo de entrada (ActionLFPG_ViewCamera → Enter):
//   1. Valida: monitor encendido + camara enlazada + camara encendida
//   2. Resuelve LF_Camera via LFPG_DeviceRegistry (cliente tiene ref)
//   3. CreateObject("staticcamera", camPos, create_local=true)
//      → objeto LOCAL, no replicado al servidor ni a otros clientes
//   4. Camera.SetActive(true) → sustituye POV del jugador local
//   5. Activa overlay: scanlines + vignette + indicador REC
//
// Flujo de salida (SPACE, timeout o Exit() directo):
//   1. Camera.SetActive(false) → motor restaura POV del jugador
//   2. ObjectDelete del staticcamera local
//
// Overlay (DrawOverlay, llamado por MissionGameplay.OnUpdate):
//   - Lineas horizontales semi-transparentes (efecto CRT)
//   - Desplazamiento vertical lento para animacion de scanlines
//   - Vignette oscura en los 4 bordes (marco CRT)
//   - Indicador REC parpadeante en esquina superior derecha
//
// Integracion en MissionGameplay.OnUpdate:
//   ver CAMBIOS_ETAPA3.md para el parche exacto.
//
// ENFORCE SCRIPT NOTES:
//   - No foreach, no ++/--, no ternario, no multilinea en params
//   - cast implicito float→int via asignacion directa
//   - Hoisting de variables antes de if/else
//   - create_local=true en CreateObject (objeto solo en este cliente)
// =========================================================

static const float LFPG_CCTV_SCANLINE_SPACING = 5.0;   // px entre lineas
static const float LFPG_CCTV_SCANLINE_ALPHA   = 0.22;  // 0-1 opacidad lineas
static const float LFPG_CCTV_SCROLL_SPEED     = 20.0;  // px/s desplazamiento
static const float LFPG_CCTV_VIGNETTE_ALPHA   = 0.60;  // 0-1 opacidad vignette
static const float LFPG_CCTV_VIGNETTE_W       = 55.0;  // px grosor borde
static const float LFPG_CCTV_MAX_DURATION_S   = 120.0; // auto-exit tras 2 min

class LFPG_CameraViewport
{
    // ---- Singleton ----
    protected static ref LFPG_CameraViewport s_Instance;

    // ---- Estado ----
    protected Object    m_ViewCamObj;      // staticcamera local creado en Enter
    protected bool      m_Active;
    protected float     m_ScanlineOffset; // px: posicion actual del scroll
    protected string    m_CameraLabel;    // para logs / futura HUD text
    protected float     m_ActiveDuration; // segundos desde Enter
    // Cachear UAInput para evitar busqueda por string cada frame en Tick().
    // Se inicializa la primera vez que Tick() necesita el input (lazy init).
    protected UAInput   m_ExitInput;

    // Bug 4 fix: colores del overlay precalculados en el constructor.
    // ARGB() de constantes estaticas nunca cambia → calcularlo 60 veces/s es desperdicio.
    protected int       m_ScanColor;  // ARGB para las scanlines
    protected int       m_VigColor;   // ARGB para la vignette

    void LFPG_CameraViewport()
    {
        m_ViewCamObj     = null;
        m_Active         = false;
        m_ScanlineOffset = 0.0;
        m_CameraLabel    = "";
        m_ActiveDuration = 0.0;
        m_ExitInput      = null;

        // Precomputar colores — cast float→int implicito (Enforce Script).
        int scanAlphaI = LFPG_CCTV_SCANLINE_ALPHA * 255.0;
        int vigAlphaI  = LFPG_CCTV_VIGNETTE_ALPHA * 255.0;
        m_ScanColor    = ARGB(scanAlphaI, 0, 0, 0);
        m_VigColor     = ARGB(vigAlphaI,  0, 0, 0);
    }

    // =========================================================
    // Singleton
    // =========================================================
    static LFPG_CameraViewport Get()
    {
        // Nunca instanciar en servidor dedicado: este sistema es solo cliente.
        if (GetGame().IsDedicatedServer())
            return null;

        if (!s_Instance)
            s_Instance = new LFPG_CameraViewport();
        return s_Instance;
    }

    static void Reset()
    {
        if (s_Instance)
        {
            s_Instance.Exit();
            delete s_Instance;
            s_Instance = null;
        }
    }

    bool IsActive()
    {
        return m_Active;
    }

    // =========================================================
    // Enter — llamado desde ActionLFPG_ViewCamera.OnExecuteClient
    // =========================================================
    void Enter(LF_Monitor monitor)
    {
        // Hoist: una sola declaracion de PlayerBase p para todos los early-returns.
        PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());

        if (!monitor)
        {
            LFPG_Util.Warn("[CameraViewport] Enter: monitor null");
            return;
        }

        if (!monitor.LFPG_IsPowered())
        {
            if (p)
                p.MessageStatus("[LFPG] El monitor no tiene alimentacion.");
            return;
        }

        // v0.9.1: Resolver la primera camara powered desde el wire store del monitor.
        // TODO Sprint B: reemplazar por RPC-based camera list (CameraView + snapshot).
        array<ref LFPG_WireData> wires = monitor.LFPG_GetWires();
        if (!wires)
        {
            if (p)
                p.MessageStatus("[LFPG] No hay camaras conectadas al monitor.");
            return;
        }

        // Hoist variables fuera del loop (Enforce Script: no declarar en bloques hermanos).
        LF_Camera cam = null;
        EntityAI camEnt = null;
        string camDevId = "";
        int wi = 0;
        while (wi < wires.Count())
        {
            LFPG_WireData wd = wires[wi];
            wi = wi + 1;
            if (!wd)
                continue;

            camDevId = wd.m_TargetDeviceId;
            if (camDevId == "")
                continue;

            camEnt = LFPG_DeviceRegistry.Get().FindById(camDevId);
            if (!camEnt)
                continue;

            cam = LF_Camera.Cast(camEnt);
            if (!cam)
                continue;

            if (!cam.LFPG_IsPowered())
            {
                cam = null;
                continue;
            }

            // Encontramos una camara valida y powered.
            break;
        }

        if (!cam)
        {
            if (p)
                p.MessageStatus("[LFPG] No hay camaras activas conectadas.");
            return;
        }

        // Limpiar viewport previo si lo habia
        if (m_Active)
            Exit();

        vector camPos = cam.GetPosition();
        vector camOri = cam.GetOrientation();

        // Crear staticcamera LOCAL en la posicion de LF_Camera.
        // create_local=true → solo visible en ESTE cliente, no replicado.
        Object viewCam = GetGame().CreateObject("staticcamera", camPos, true, false, false);
        if (!viewCam)
        {
            LFPG_Util.Error("[CameraViewport] Fallo CreateObject staticcamera");
            if (p)
                p.MessageStatus("[LFPG] Error al activar camara. Intenta de nuevo.");
            return;
        }

        // SetPosition omitida: CreateObject ya ubica el objeto en camPos.
        viewCam.SetOrientation(camOri);

        Camera viewCamTyped = Camera.Cast(viewCam);
        if (!viewCamTyped)
        {
            LFPG_Util.Error("[CameraViewport] staticcamera no casteable a Camera");
            GetGame().ObjectDelete(viewCam);
            return;
        }

        viewCamTyped.SetActive(true);

        m_ViewCamObj     = viewCam;
        m_Active         = true;
        m_ScanlineOffset = 0.0;
        m_ActiveDuration = 0.0;

        // Etiqueta de camara (ultimos 6 chars del DeviceId para pantalla)
        int idLen = camDevId.Length();
        if (idLen > 6)
        {
            m_CameraLabel = "CAM-" + camDevId.Substring(idLen - 6, 6);
        }
        else
        {
            m_CameraLabel = "CAM-" + camDevId;
        }

        string logEntry = "[CameraViewport] Viewport activo: " + m_CameraLabel;
        logEntry = logEntry + " pos=" + camPos.ToString();
        LFPG_Util.Info(logEntry);

        if (p)
        {
            string enterMsg = "[LFPG] Vista: " + m_CameraLabel + "  (SPACE para salir)";
            p.MessageStatus(enterMsg);
        }
    }

    // =========================================================
    // Exit — llamado por Tick (SPACE/timeout) o Reset
    // =========================================================
    void Exit()
    {
        if (!m_Active)
            return;

        m_Active = false;

        if (m_ViewCamObj)
        {
            Camera viewCamTyped = Camera.Cast(m_ViewCamObj);
            if (viewCamTyped)
            {
                // SetActive(false) restaura el POV del jugador local.
                viewCamTyped.SetActive(false);
            }
            GetGame().ObjectDelete(m_ViewCamObj);
            m_ViewCamObj = null;
        }

        m_CameraLabel    = "";
        m_ActiveDuration = 0.0;
        m_ScanlineOffset = 0.0; // OBS-03: simetria con Enter(). Sin efecto funcional.

        LFPG_Util.Info("[CameraViewport] Viewport cerrado.");
    }

    // =========================================================
    // Tick — llamado cada frame desde MissionGameplay.OnUpdate
    // Actualiza animacion y detecta input de salida.
    // NO debe llamarse si IsActive() == false (coste cero si inactivo).
    // =========================================================
    void Tick(float timeslice)
    {
        if (!m_Active)
            return;

        // Auto-exit por tiempo maximo (seguridad)
        m_ActiveDuration = m_ActiveDuration + timeslice;
        if (m_ActiveDuration >= LFPG_CCTV_MAX_DURATION_S)
        {
            LFPG_Util.Info("[CameraViewport] Auto-exit (timeout " + LFPG_CCTV_MAX_DURATION_S.ToString() + "s)");
            Exit();
            return;
        }

        // Deteccion de salida: tecla SPACE (UAJump).
        // m_ExitInput cacheado en el primer Tick para evitar busqueda
        // por string (GetInputByName) 60 veces/segundo.
        if (!m_ExitInput)
            m_ExitInput = GetUApi().GetInputByName("UAJump");

        if (m_ExitInput && m_ExitInput.LocalPress())
        {
            Exit();
            return;
        }

        // Avanzar offset de scanlines (mantener en [0, SPACING))
        m_ScanlineOffset = m_ScanlineOffset + (LFPG_CCTV_SCROLL_SPEED * timeslice);
        while (m_ScanlineOffset >= LFPG_CCTV_SCANLINE_SPACING)
        {
            m_ScanlineOffset = m_ScanlineOffset - LFPG_CCTV_SCANLINE_SPACING;
        }
    }

    // =========================================================
    // DrawOverlay — llamado dentro de la ventana BeginFrame/EndFrame
    // de LFPG_CableHUD (canvas ya limpio, listo para recibir lineas).
    // Dibuja: scanlines + vignette + indicador REC parpadeante.
    // =========================================================
    void DrawOverlay(LFPG_CableHUD hud)
    {
        if (!m_Active)
            return;

        if (!hud || !hud.IsReady())
            return;

        float sw = hud.GetScreenW();
        float sh = hud.GetScreenH();

        if (sw <= 0.0 || sh <= 0.0)
            return;

        // --------------------------------------------------
        // Scanlines horizontales semi-transparentes
        // --------------------------------------------------
        // m_ScanColor precalculado en el constructor — no recalcular cada frame.
        float lineY = m_ScanlineOffset;
        while (lineY < sh)
        {
            hud.DrawLineScreen(0.0, lineY, sw, lineY, 1.0, m_ScanColor);
            lineY = lineY + LFPG_CCTV_SCANLINE_SPACING;
        }

        // --------------------------------------------------
        // Vignette: 4 bordes oscuros (marco CRT)
        // m_VigColor precalculado en el constructor — no recalcular cada frame.
        // DrawLineScreen con grosor = vw actua como banda solida.
        // --------------------------------------------------
        // OBS-04: escalar vignette proporcional a la altura de pantalla.
        // 55px en 1080p = ~5.1% de sh. Mantener esa proporcion en 4K.
        float vwScale = sh / 1080.0;
        float vw    = LFPG_CCTV_VIGNETTE_W * vwScale;
        float vhalf = vw * 0.5;

        hud.DrawLineScreen(vhalf,      0.0,  vhalf,      sh,   vw, m_VigColor); // izq
        hud.DrawLineScreen(sw - vhalf, 0.0,  sw - vhalf, sh,   vw, m_VigColor); // der
        hud.DrawLineScreen(0.0,        vhalf, sw,         vhalf, vw, m_VigColor); // sup
        hud.DrawLineScreen(0.0,        sh - vhalf, sw, sh - vhalf, vw, m_VigColor); // inf

        // --------------------------------------------------
        // Indicador REC parpadeante (cuadrado rojo, esquina superior derecha)
        // Visible durante segundos pares, oculto durante impares (~1Hz).
        // cast implicito float→int trunca m_ActiveDuration a segundos enteros.
        // --------------------------------------------------
        int recTick = m_ActiveDuration;
        if ((recTick % 2) == 0)
        {
            int   recColor = ARGB(210, 190, 0, 0);
            float recX     = sw - 24.0;
            float recY     = 14.0;
            float recSz    = 7.0;
            // Bug 3 fix: precomputar el limite superior del while para no
            // recalcular recY + recSz en cada iteracion (8 iteraciones × 60fps).
            float recYEnd  = recY + recSz;

            // Cuadrado relleno: lineas horizontales de 1px
            float ry = recY;
            while (ry <= recYEnd)
            {
                hud.DrawLineScreen(recX, ry, recX + recSz, ry, 1.0, recColor);
                ry = ry + 1.0;
            }
        }
    }
}
