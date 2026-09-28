#ifndef SERVER
// Client-only compilation boundary
// =========================================================
// LF_PowerGrid - 3D cable backend (client only)
//
// One client-local LFPG_CableSegment (HouseNoDestruct, model
// lfpg_cable_segment.p3d: 1 m open tube on +Z, centred, radius 0.01,
// no geometry LOD) per post-sag sub-segment built by LFPG_CableRenderer
// (LFPG_CableParticle). Placed with SetTransform: basis =
// (aside * thickness, up * thickness, dir * length), origin = midpoint.
// Colour by state through hiddenSelections "camo" (model.cfg sections[]).
//
// The renderer keeps its whole data pipeline (sync, decode, retry, cull,
// sag, 512 sub-segment budget); only the output changes. Object work is
// spread over frames (LFPG_C3D_WORK_PER_FRAME) with a bounded retry.
//
// Mode lives in $profile:LF_PowerGrid/LF_Cables3D.json ("Mode": 1 = 3D,
// 0 = 2D). F9 with a cable reel or pliers in hands toggles 2D / 3D
// (LFPG_MissionInit.c OnKeyPress). The 2D canvas path stays intact and
// keeps drawing the laser beams and the wiring preview.
// =========================================================

static const int    LFPG_C3D_MODE_2D          = 0;
static const int    LFPG_C3D_MODE_3D          = 1;
static const int    LFPG_C3D_LOOK_NONE        = -1;
static const int    LFPG_C3D_LOOK_HIDDEN      = 0;
static const int    LFPG_C3D_LOOK_DARK        = 1;
static const int    LFPG_C3D_LOOK_IDLE        = 2;
static const int    LFPG_C3D_LOOK_POWERED     = 3;
static const int    LFPG_C3D_LOOK_CRITICAL    = 4;
static const int    LFPG_C3D_WORK_PER_FRAME   = 48;    // object creations / recolours per frame
static const int    LFPG_C3D_MAX_LIVE         = 600;   // one per sub-segment, renderer cap is 512
static const int    LFPG_C3D_MAX_TRIES        = 3;
static const float  LFPG_C3D_THICKNESS        = 1.5;   // model radius 0.01 m -> 1.5 cm
static const float  LFPG_C3D_OVERLAP_M        = 0.01;  // added at each end, hides joints
static const float  LFPG_C3D_DEBUG_PERIOD_S   = 10.0;
static const string LFPG_C3D_CLASS            = "LFPG_CableSegment";
static const string LFPG_C3D_TEX_DARK         = "#(argb,8,8,3)color(0.05,0.05,0.05,1.0,CO)";
static const string LFPG_C3D_TEX_IDLE         = "#(argb,8,8,3)color(0.48,0.50,0.53,1.0,CO)";
static const string LFPG_C3D_TEX_POWERED      = "#(argb,8,8,3)color(0.18,0.61,0.35,1.0,CO)";
static const string LFPG_C3D_TEX_CRITICAL     = "#(argb,8,8,3)color(0.90,0.49,0.13,1.0,CO)";
static const string LFPG_C3D_SETTINGS_DIR     = "$profile:LF_PowerGrid";
static const string LFPG_C3D_SETTINGS_FILE    = "$profile:LF_PowerGrid/LF_Cables3D.json";

class LFPG_Cable3DSettings
{
	int Mode = 1;
	bool Debug = false;
};

class LFPG_Cable3D
{
	protected static ref LFPG_Cable3DSettings s_Settings;
	protected static ref array<ref LFPG_CableParticle> s_Queue;
	protected static bool s_ToolHeld;
	protected static bool s_LookDirty;
	protected static int s_Live;
	protected static int s_Created;
	protected static int s_Released;
	protected static int s_Failed;
	protected static int s_GaveUp;
	protected static float s_DebugAccS;

	protected static void EnsureInit()
	{
		if (s_Settings)
			return;
		LoadSettings();
		if (!s_Queue)
			s_Queue = new array<ref LFPG_CableParticle>;
	}

	protected static void LoadSettings()
	{
		s_Settings = new LFPG_Cable3DSettings();
		string settingsDir = LFPG_C3D_SETTINGS_DIR;
		if (!FileExist(settingsDir))
			MakeDirectory(settingsDir);

		string settingsFile = LFPG_C3D_SETTINGS_FILE;
		if (FileExist(settingsFile))
		{
			string err;
			if (!JsonFileLoader<LFPG_Cable3DSettings>.LoadFile(settingsFile, s_Settings, err))
			{
				string loadMsg = "[Cable3D] settings load failed, using defaults: ";
				loadMsg = loadMsg + err;
				LFPG_Util.Warn(loadMsg);
				s_Settings = new LFPG_Cable3DSettings();
			}
		}
		else
		{
			SaveSettings();
		}

		if (s_Settings.Mode != LFPG_C3D_MODE_2D)
			s_Settings.Mode = LFPG_C3D_MODE_3D;

		string modeMsg = "[Cable3D] mode=";
		modeMsg = modeMsg + ModeName(s_Settings.Mode);
		LFPG_Util.Info(modeMsg);
	}

	protected static void SaveSettings()
	{
		string settingsFile = LFPG_C3D_SETTINGS_FILE;
		string err;
		if (!JsonFileLoader<LFPG_Cable3DSettings>.SaveFile(settingsFile, s_Settings, err))
		{
			string saveMsg = "[Cable3D] settings save failed: ";
			saveMsg = saveMsg + err;
			LFPG_Util.Warn(saveMsg);
		}
	}

	static bool Is3D()
	{
		EnsureInit();
		return s_Settings.Mode == LFPG_C3D_MODE_3D;
	}

	static string ModeName(int mode)
	{
		if (mode == LFPG_C3D_MODE_3D)
			return "3D";
		return "2D";
	}

	// F9 handler (LFPG_MissionInit.c). Releases every segment and rebuilds
	// the wires in the new mode through LFPG_CableRenderer.ForceGlobalRefresh.
	static int ToggleMode()
	{
		EnsureInit();
		if (s_Settings.Mode == LFPG_C3D_MODE_3D)
		{
			s_Settings.Mode = LFPG_C3D_MODE_2D;
		}
		else
		{
			s_Settings.Mode = LFPG_C3D_MODE_3D;
		}
		SaveSettings();
		s_Queue.Clear();

		string toggleMsg = "[Cable3D] mode toggled to ";
		toggleMsg = toggleMsg + ModeName(s_Settings.Mode);
		LFPG_Util.Info(toggleMsg);

		LFPG_CableRenderer renderer = LFPG_CableRenderer.Get();
		if (renderer)
			renderer.ForceGlobalRefresh();
		s_LookDirty = true;
		return s_Settings.Mode;
	}

	static void MarkLookDirty()
	{
		s_LookDirty = true;
	}

	static int LookFor(int cableState, bool toolHeld, bool visible)
	{
		if (!visible)
			return LFPG_C3D_LOOK_HIDDEN;
		if (!toolHeld)
			return LFPG_C3D_LOOK_DARK;
		if (cableState == LFPG_CableState.POWERED)
			return LFPG_C3D_LOOK_POWERED;
		if (cableState == LFPG_CableState.CRITICAL_LOAD)
			return LFPG_C3D_LOOK_CRITICAL;
		return LFPG_C3D_LOOK_IDLE;
	}

	// Called by LFPG_CableRenderer.Refresh3DLook for every live sub-segment.
	static void Want(LFPG_CableParticle p, int look)
	{
		if (!p)
			return;
		if (!p.IsValid())
			return;
		if (p.m_Cable3DWant == look)
			return;
		p.m_Cable3DWant = look;
		p.m_Cable3DTries = 0;
		EnsureInit();
		s_Queue.Insert(p);
	}

	// Called once per frame from LFPG_CableRenderer.MaintenanceTick.
	static void Tick(LFPG_CableRenderer renderer, float timeslice)
	{
		if (!Is3D())
			return;

		PlayerBase pb = PlayerBase.Cast(g_Game.GetPlayer());
		bool tool = false;
		if (pb)
		{
			if (LFPG_WorldUtil.PlayerHasCableReelInHands(pb))
				tool = true;
			else if (LFPG_WorldUtil.PlayerHasPliersInHands(pb))
				tool = true;
		}
		if (tool != s_ToolHeld)
		{
			s_ToolHeld = tool;
			s_LookDirty = true;
		}

		if (s_LookDirty && renderer)
		{
			s_LookDirty = false;
			renderer.Refresh3DLook(s_ToolHeld);
		}

		// Pops from the back. A failed entry is re-inserted at the front BEFORE
		// the examined entry is removed, so the queue never drops its last
		// strong reference to a particle that is still being retried.
		int budget = LFPG_C3D_WORK_PER_FRAME;
		int examined = 0;
		int queued = s_Queue.Count();
		while (budget > 0 && examined < queued && s_Queue.Count() > 0)
		{
			examined = examined + 1;
			int last = s_Queue.Count() - 1;
			LFPG_CableParticle p = s_Queue[last];
			bool retry = false;
			if (p && p.IsValid() && p.m_Cable3DLook != p.m_Cable3DWant)
			{
				budget = budget - 1;
				if (!Apply(p))
				{
					p.m_Cable3DTries = p.m_Cable3DTries + 1;
					if (p.m_Cable3DTries < LFPG_C3D_MAX_TRIES)
						retry = true;
					else
						s_GaveUp = s_GaveUp + 1;
				}
			}
			if (retry)
				s_Queue.InsertAt(p, 0);
			s_Queue.Remove(s_Queue.Count() - 1);
		}

		if (s_Settings.Debug)
		{
			s_DebugAccS = s_DebugAccS + timeslice;
			if (s_DebugAccS >= LFPG_C3D_DEBUG_PERIOD_S)
			{
				s_DebugAccS = 0.0;
				LogCounters();
			}
		}
	}

	protected static void LogCounters()
	{
		string msg = "[Cable3D] live=";
		msg = msg + s_Live.ToString();
		msg = msg + " queued=";
		msg = msg + s_Queue.Count().ToString();
		msg = msg + " created=";
		msg = msg + s_Created.ToString();
		msg = msg + " released=";
		msg = msg + s_Released.ToString();
		msg = msg + " failed=";
		msg = msg + s_Failed.ToString();
		msg = msg + " gaveUp=";
		msg = msg + s_GaveUp.ToString();
		LFPG_Util.Info(msg);
	}

	// Called from LFPG_CableRenderer.CleanupInstance after its DestroyAll().
	static void Reset()
	{
		if (s_Queue)
			s_Queue.Clear();
		s_Live = 0;
		s_Created = 0;
		s_Released = 0;
		s_Failed = 0;
		s_GaveUp = 0;
		s_ToolHeld = false;
		s_LookDirty = false;
		s_DebugAccS = 0.0;
		s_Settings = null;
	}

	// Deletes one client-local segment object. Called by
	// LFPG_CableParticle.Destroy with its own object (never with the particle).
	static void DeleteObject(Object obj)
	{
		if (!obj)
			return;
		if (g_Game)
			g_Game.ObjectDelete(obj);
		s_Live = s_Live - 1;
		s_Released = s_Released + 1;
	}

	protected static string TextureFor(int look)
	{
		if (look == LFPG_C3D_LOOK_IDLE)
			return LFPG_C3D_TEX_IDLE;
		if (look == LFPG_C3D_LOOK_POWERED)
			return LFPG_C3D_TEX_POWERED;
		if (look == LFPG_C3D_LOOK_CRITICAL)
			return LFPG_C3D_TEX_CRITICAL;
		return LFPG_C3D_TEX_DARK;
	}

	// Brings the particle to its wanted look. Returns false only when the
	// object could not be created.
	protected static bool Apply(LFPG_CableParticle p)
	{
		int want = p.m_Cable3DWant;
		if (want == LFPG_C3D_LOOK_HIDDEN || want == LFPG_C3D_LOOK_NONE)
		{
			if (p.m_Cable3DObj)
			{
				DeleteObject(p.m_Cable3DObj);
				p.m_Cable3DObj = null;
			}
			p.m_Cable3DLook = want;
			return true;
		}

		if (!p.m_Cable3DObj)
		{
			if (s_Live >= LFPG_C3D_MAX_LIVE)
				return false;
			p.m_Cable3DObj = SpawnSegment(p.m_From, p.m_To);
			if (!p.m_Cable3DObj)
				return false;
		}

		EntityAI e = EntityAI.Cast(p.m_Cable3DObj);
		if (e)
			e.SetObjectTexture(0, TextureFor(want));
		p.m_Cable3DLook = want;
		return true;
	}

	protected static Object SpawnSegment(vector a, vector b)
	{
		vector d = b - a;
		float len = d.Length();
		if (len < 0.01)
			return null;

		vector dir = d * (1.0 / len);
		vector up = "0 1 0";
		if (Math.AbsFloat(dir[1]) > 0.99)
			up = "1 0 0";

		// Rows: aside, up, dir (enmath3d.c DirectionAndUpMatrix). The scaled
		// basis stretches the 1 m model along the sub-segment only.
		vector mat[4];
		Math3D.DirectionAndUpMatrix(dir, up, mat);
		float stretch = len + 2.0 * LFPG_C3D_OVERLAP_M;
		mat[0] = mat[0] * LFPG_C3D_THICKNESS;
		mat[1] = mat[1] * LFPG_C3D_THICKNESS;
		mat[2] = mat[2] * stretch;
		vector half = dir * (len * 0.5);
		mat[3] = a + half;

		Object o = g_Game.CreateObjectEx(LFPG_C3D_CLASS, mat[3], ECE_LOCAL);
		if (!o)
		{
			s_Failed = s_Failed + 1;
			if (s_Failed == 1)
			{
				string failMsg = "[Cable3D] CreateObjectEx returned null for ";
				failMsg = failMsg + LFPG_C3D_CLASS;
				LFPG_Util.Warn(failMsg);
			}
			return null;
		}
		o.SetTransform(mat);
		o.Update();
		s_Live = s_Live + 1;
		s_Created = s_Created + 1;
		if (s_Created == 1)
			LFPG_Util.Info("[Cable3D] first segment object created");
		return o;
	}
};
#endif
