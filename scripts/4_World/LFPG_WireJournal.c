// Dirty-shutdown journal for native wire owners (WireOwnerBase + Generator).

static bool LFPG_WireJournalMarkAllowed(bool active, bool hasEntry, int entryCount, int maxEntries)
{
	if (!active)
		return false;
	if (hasEntry)
		return true;
	if (entryCount >= maxEntries)
		return false;
	return true;
}

static int LFPG_WireJournalCaptureKind(bool usable, bool liveEmpty, bool savedEmpty, bool liveEqualsSaved, bool prevEmpty, bool loadedEmpty)
{
	if (!usable)
		return LFPG_WJ_CAPTURE_REMOVE;
	if (liveEmpty)
		return LFPG_WJ_CAPTURE_KEEP;
	if (savedEmpty && prevEmpty && loadedEmpty)
		return LFPG_WJ_CAPTURE_REMOVE;
	if (liveEqualsSaved && prevEmpty)
		return LFPG_WJ_CAPTURE_REMOVE;
	return LFPG_WJ_CAPTURE_STORE;
}

static int LFPG_WireJournalSaveVerdict(bool hasEntry, bool isDirty, bool savedEqualsNewest, bool seenOnce)
{
	if (!hasEntry)
		return LFPG_WJ_SAVE_IGNORE;
	if (isDirty)
		return LFPG_WJ_SAVE_IGNORE;
	if (!savedEqualsNewest)
		return LFPG_WJ_SAVE_IGNORE;
	if (!seenOnce)
		return LFPG_WJ_SAVE_FIRST_EMIT;
	return LFPG_WJ_SAVE_DURABLE;
}

static bool LFPG_WireJournalTtlExpired(int capturedMs, int nowMs, int ttlMs)
{
	if (capturedMs < 0)
		return false;
	if (nowMs - capturedMs >= ttlMs)
		return true;
	return false;
}

static int LFPG_WireJournalReplayVerdict(bool ownerFound, bool usable, bool storedEqualsNewest, bool storedEqualsSaved, bool storedEqualsPrev, bool storedEqualsLoaded)
{
	if (!ownerFound)
		return LFPG_WJ_REPLAY_MISSING;
	if (!usable)
		return LFPG_WJ_REPLAY_UNUSABLE;
	if (storedEqualsNewest)
		return LFPG_WJ_REPLAY_UNCHANGED;
	if (storedEqualsSaved)
		return LFPG_WJ_REPLAY_RESTORE;
	if (storedEqualsPrev)
		return LFPG_WJ_REPLAY_RESTORE;
	if (storedEqualsLoaded)
		return LFPG_WJ_REPLAY_RESTORE;
	return LFPG_WJ_REPLAY_STALE;
}

static int LFPG_WireJournalFileVerdict(bool exists, bool versionRead, int version, int currentVer)
{
	if (!exists)
		return LFPG_WJ_FILE_ABSENT;
	if (versionRead && version > currentVer)
		return LFPG_WJ_FILE_FUTURE;
	return LFPG_WJ_FILE_CANDIDATE;
}

class LFPG_WireJournal
{
	static ref LFPG_WireJournal s_Instance;
	ref map<string, ref LFPG_WireJournalEntry> m_Entries;
	ref map<string, bool> m_Dirty;
	ref map<string, bool> m_SeenEmit;
	ref map<string, int> m_CapturedMs;
	ref array<string> m_Restored;
	bool m_Loaded;
	bool m_Enabled;
	bool m_ReadOnly;
	bool m_FileDirty;
	bool m_CapWarned;
	int m_LastSaveWarnMs;
	int m_LastKeepWarnMs;

	void LFPG_WireJournal()
	{
		m_Entries = new map<string, ref LFPG_WireJournalEntry>;
		m_Dirty = new map<string, bool>;
		m_SeenEmit = new map<string, bool>;
		m_CapturedMs = new map<string, int>;
		m_Restored = new array<string>;
		m_Loaded = false;
		m_Enabled = false;
		m_ReadOnly = false;
		m_FileDirty = false;
		m_CapWarned = false;
		m_LastSaveWarnMs = 0;
		m_LastKeepWarnMs = 0;
	}

	static LFPG_WireJournal Get()
	{
		if (!s_Instance)
			s_Instance = new LFPG_WireJournal();
		return s_Instance;
	}

	bool IsActive()
	{
		if (!m_Loaded)
			return false;
		if (!m_Enabled)
			return false;
		if (m_ReadOnly)
			return false;
		return true;
	}

	protected bool OwnerUsable(EntityAI obj)
	{
		if (!obj)
			return false;
		if (!LFPG_DeviceAPI.HasWireStore(obj))
			return false;
		string id = LFPG_DeviceAPI.GetDeviceId(obj);
		if (LFPG_DeviceRegistry.Get().IsAmbiguous(id))
			return false;
		LFPG_WireOwnerBase wo = LFPG_WireOwnerBase.Cast(obj);
		if (wo)
		{
			if (wo.LFPG_IsWireStoreFrozen())
				return false;
		}
		return true;
	}

	protected string OwnerLoadedJSON(EntityAI obj)
	{
		string empty = "";
		if (!obj)
			return empty;
		LFPG_WireOwnerBase wo = LFPG_WireOwnerBase.Cast(obj);
		if (wo)
			return wo.LFPG_GetLoadedWiresJSON();
		LFPG_Generator gen = LFPG_Generator.Cast(obj);
		if (gen)
			return gen.LFPG_GetLoadedWiresJSON();
		return empty;
	}

	protected string OwnerLastJSON(EntityAI obj)
	{
		string empty = "";
		if (!obj)
			return empty;
		LFPG_WireOwnerBase wo = LFPG_WireOwnerBase.Cast(obj);
		if (wo)
			return wo.LFPG_GetLastStoredWiresJSON();
		LFPG_Generator gen = LFPG_Generator.Cast(obj);
		if (gen)
			return gen.LFPG_GetLastStoredWiresJSON();
		return empty;
	}

	protected string OwnerPrevJSON(EntityAI obj)
	{
		string empty = "";
		if (!obj)
			return empty;
		LFPG_WireOwnerBase wo = LFPG_WireOwnerBase.Cast(obj);
		if (wo)
			return wo.LFPG_GetPrevStoredWiresJSON();
		LFPG_Generator gen = LFPG_Generator.Cast(obj);
		if (gen)
			return gen.LFPG_GetPrevStoredWiresJSON();
		return empty;
	}

	protected void DropEntry(string ownerId)
	{
		m_Entries.Remove(ownerId);
		m_Dirty.Remove(ownerId);
		m_SeenEmit.Remove(ownerId);
		m_CapturedMs.Remove(ownerId);
	}

	void MarkOwnerDirty(EntityAI owner)
	{
		#ifdef SERVER
		if (!owner)
			return;
		string id = LFPG_DeviceAPI.GetDeviceId(owner);
		if (id == "")
			return;
		if (!LFPG_DeviceAPI.HasWireStore(owner))
			return;
		bool hasEntry = m_Entries.Contains(id);
		int entryCount = m_Entries.Count();
		if (LFPG_WireJournalMarkAllowed(IsActive(), hasEntry, entryCount, LFPG_WIRE_JOURNAL_MAX_ENTRIES))
		{
			m_Dirty.Set(id, true);
			return;
		}
		if (IsActive() && !hasEntry && entryCount >= LFPG_WIRE_JOURNAL_MAX_ENTRIES)
		{
			if (!m_CapWarned)
			{
				string capMsg = "[WireJournal] Entry cap ";
				capMsg = capMsg + LFPG_WIRE_JOURNAL_MAX_ENTRIES.ToString();
				capMsg = capMsg + " reached; new owners are not journaled until entries retire";
				LFPG_Util.Warn(capMsg);
				m_CapWarned = true;
			}
		}
		#endif
	}

	void OnOwnerSaved(string ownerId, string savedJSON)
	{
		#ifdef SERVER
		if (!IsActive())
			return;
		bool hasEntry = false;
		hasEntry = m_Entries.Contains(ownerId);
		bool isDirty = false;
		isDirty = m_Dirty.Contains(ownerId);
		bool savedEqualsNewest = false;
		if (hasEntry)
		{
			LFPG_WireJournalEntry entry = m_Entries.Get(ownerId);
			if (entry)
			{
				if (savedJSON == entry.m_WiresJSON)
					savedEqualsNewest = true;
			}
		}
		bool seenOnce = false;
		seenOnce = m_SeenEmit.Contains(ownerId);
		int v = 0;
		v = LFPG_WireJournalSaveVerdict(hasEntry, isDirty, savedEqualsNewest, seenOnce);
		if (v == LFPG_WJ_SAVE_FIRST_EMIT)
		{
			m_SeenEmit.Set(ownerId, true);
			string firstMsg = "[WireJournal] first-emit ";
			firstMsg = firstMsg + ownerId;
			LFPG_Util.Debug(firstMsg);
		}
		else
		{
			if (v == LFPG_WJ_SAVE_DURABLE)
			{
				m_Entries.Remove(ownerId);
				m_Dirty.Remove(ownerId);
				m_SeenEmit.Remove(ownerId);
				m_CapturedMs.Remove(ownerId);
				m_FileDirty = true;
				string durMsg = "[WireJournal] durable ";
				durMsg = durMsg + ownerId;
				LFPG_Util.Debug(durMsg);
			}
		}
		#endif
	}

	void Tick(int nowMs)
	{
		#ifdef SERVER
		if (!IsActive())
			return;

		LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
		if (nm)
		{
			if (!nm.IsValidationActive())
			{
				array<string> dirtyIds = new array<string>;
				int di;
				for (di = 0; di < m_Dirty.Count(); di = di + 1)
				{
					dirtyIds.Insert(m_Dirty.GetKey(di));
				}
				array<string> keepIds = new array<string>;
				for (di = 0; di < dirtyIds.Count(); di = di + 1)
				{
					string id = dirtyIds[di];
					EntityAI obj = LFPG_DeviceRegistry.Get().FindById(id);
					string live = "";
					string saved = "";
					string prev = "";
					string loaded = "";
					if (obj)
					{
						live = LFPG_DeviceAPI.GetWiresJSON(obj);
						saved = OwnerLastJSON(obj);
						prev = OwnerPrevJSON(obj);
						loaded = OwnerLoadedJSON(obj);
					}
					bool usable = false;
					if (obj)
						usable = OwnerUsable(obj);
					bool liveEmpty = false;
					if (live == "")
						liveEmpty = true;
					bool savedEmpty = false;
					if (saved == "")
						savedEmpty = true;
					bool liveEqualsSaved = false;
					if (live == saved)
						liveEqualsSaved = true;
					bool prevEmpty = false;
					if (prev == "")
						prevEmpty = true;
					bool loadedEmpty = false;
					if (loaded == "")
						loadedEmpty = true;
					int kind = LFPG_WireJournalCaptureKind(usable, liveEmpty, savedEmpty, liveEqualsSaved, prevEmpty, loadedEmpty);
					if (kind == LFPG_WJ_CAPTURE_REMOVE)
					{
						if (m_Entries.Contains(id))
						{
							DropEntry(id);
							m_FileDirty = true;
						}
						else
						{
							m_Dirty.Remove(id);
							m_SeenEmit.Remove(id);
							m_CapturedMs.Remove(id);
						}
					}
					else if (kind == LFPG_WJ_CAPTURE_KEEP)
					{
						keepIds.Insert(id);
						LFPG_WireOwnerBase keepOwner = LFPG_WireOwnerBase.Cast(obj);
						if (keepOwner)
							keepOwner.LFPG_InvalidateWireJSONCache();
						if (nowMs - m_LastKeepWarnMs >= 60000)
						{
							string keepMsg = "[WireJournal] Wires of ";
							keepMsg = keepMsg + id;
							keepMsg = keepMsg + " did not serialize; retrying";
							LFPG_Util.Warn(keepMsg);
							m_LastKeepWarnMs = nowMs;
						}
					}
					else
					{
						bool canStore = m_Entries.Contains(id);
						if (!canStore)
						{
							if (m_Entries.Count() < LFPG_WIRE_JOURNAL_MAX_ENTRIES)
								canStore = true;
						}
						if (canStore)
						{
							LFPG_WireJournalEntry rec = m_Entries.Get(id);
							if (!rec)
							{
								rec = new LFPG_WireJournalEntry();
								rec.m_OwnerDeviceId = id;
								m_Entries.Set(id, rec);
							}
							bool changed = false;
							if (rec.m_WiresJSON != live)
							{
								rec.m_WiresJSON = live;
								m_SeenEmit.Remove(id);
								changed = true;
							}
							if (rec.m_SavedJSON != saved)
							{
								rec.m_SavedJSON = saved;
								changed = true;
							}
							if (rec.m_SavedPrevJSON != prev)
							{
								rec.m_SavedPrevJSON = prev;
								changed = true;
							}
							if (rec.m_LoadedJSON != loaded)
							{
								rec.m_LoadedJSON = loaded;
								changed = true;
							}
							m_CapturedMs.Set(id, nowMs);
							if (changed)
								m_FileDirty = true;
						}
					}
				}
				m_Dirty.Clear();
				for (di = 0; di < keepIds.Count(); di = di + 1)
				{
					m_Dirty.Set(keepIds[di], true);
				}
			}
		}

		array<string> ttlIds = new array<string>;
		int ti;
		for (ti = 0; ti < m_CapturedMs.Count(); ti = ti + 1)
		{
			string ttlId = m_CapturedMs.GetKey(ti);
			int capturedMs = m_CapturedMs.Get(ttlId);
			if (LFPG_WireJournalTtlExpired(capturedMs, nowMs, LFPG_WIRE_JOURNAL_TTL_MS))
				ttlIds.Insert(ttlId);
		}
		for (ti = 0; ti < ttlIds.Count(); ti = ti + 1)
		{
			DropEntry(ttlIds[ti]);
			m_FileDirty = true;
			string ttlMsg = "[WireJournal] ttl ";
			ttlMsg = ttlMsg + ttlIds[ti];
			LFPG_Util.Debug(ttlMsg);
		}

		if (m_FileDirty)
		{
			if (!FileExist(LFPG_WIRE_JOURNAL_DIR))
				MakeDirectory(LFPG_WIRE_JOURNAL_DIR);
			LFPG_WireJournalStore store = new LFPG_WireJournalStore();
			int ei;
			for (ei = 0; ei < m_Entries.Count(); ei = ei + 1)
			{
				string sid = m_Entries.GetKey(ei);
				LFPG_WireJournalEntry se = m_Entries.Get(sid);
				if (se)
					store.entries.Insert(se);
			}
			float saveStart = 0;
			if (g_Game)
				saveStart = g_Game.GetTickTime();
			bool saveOk = LFPG_FileUtil.AtomicSaveWireJournal(LFPG_WIRE_JOURNAL_FILE, store);
			if (saveOk)
			{
				m_FileDirty = false;
				float saveDurMs = 0;
				if (g_Game)
					saveDurMs = (g_Game.GetTickTime() - saveStart) * 1000;
				string saveMsg = "[WireJournal] saved entries=";
				saveMsg = saveMsg + store.entries.Count().ToString();
				saveMsg = saveMsg + " ms=";
				saveMsg = saveMsg + saveDurMs.ToString();
				LFPG_Util.Debug(saveMsg);
			}
			else
			{
				if (nowMs - m_LastSaveWarnMs >= 60000)
				{
					LFPG_Util.Warn("[WireJournal] Save failed; dirty state retained for retry");
					m_LastSaveWarnMs = nowMs;
				}
			}
		}
		#endif
	}

	void ReplayAtBoot()
	{
		#ifdef SERVER
		if (m_Loaded)
			return;
		m_Loaded = true;
		m_Enabled = true;
		LFPG_ServerSettings st = LFPG_Settings.Get();
		if (st)
			m_Enabled = st.WireJournal;

		array<string> candidates = new array<string>;
		candidates.Insert(LFPG_WIRE_JOURNAL_FILE + ".tmp");
		candidates.Insert(LFPG_WIRE_JOURNAL_FILE);
		candidates.Insert(LFPG_WIRE_JOURNAL_FILE + ".bak.new");
		candidates.Insert(LFPG_WIRE_JOURNAL_FILE + ".bak");

		int ci;
		for (ci = 0; ci < candidates.Count(); ci = ci + 1)
		{
			string path = candidates[ci];
			bool exists = FileExist(path);
			int ver = 0;
			bool versionRead = LFPG_FileUtil.TryReadRawJsonVersion(path, ver);
			int fv = LFPG_WireJournalFileVerdict(exists, versionRead, ver, LFPG_WIRE_JOURNAL_VER);
			if (fv == LFPG_WJ_FILE_FUTURE)
			{
				m_ReadOnly = true;
				string futMsg = "[WireJournal] ";
				futMsg = futMsg + path;
				futMsg = futMsg + " has schema v";
				futMsg = futMsg + ver.ToString();
				futMsg = futMsg + " > v";
				futMsg = futMsg + LFPG_WIRE_JOURNAL_VER.ToString();
				futMsg = futMsg + "; journal off this boot, files left untouched";
				LFPG_Util.Warn(futMsg);
				return;
			}
		}

		if (!m_Enabled)
		{
			bool removedAny = false;
			for (ci = 0; ci < candidates.Count(); ci = ci + 1)
			{
				if (FileExist(candidates[ci]))
				{
					DeleteFile(candidates[ci]);
					removedAny = true;
				}
			}
			if (removedAny)
				LFPG_Util.Info("[WireJournal] Disabled by settings; journal files removed");
			return;
		}

		LFPG_WireJournalStore loadedStore = new LFPG_WireJournalStore();
		string loadedPath = "";
		bool haveLoaded = false;
		bool anyExist = false;
		for (ci = 0; ci < candidates.Count(); ci = ci + 1)
		{
			string cpath = candidates[ci];
			if (!FileExist(cpath))
				continue;
			anyExist = true;
			LFPG_WireJournalStore probe = new LFPG_WireJournalStore();
			string loadErr;
			if (JsonFileLoader<LFPG_WireJournalStore>.LoadFile(cpath, probe, loadErr))
			{
				if (probe.ver == LFPG_WIRE_JOURNAL_VER)
				{
					loadedStore = probe;
					loadedPath = cpath;
					haveLoaded = true;
					break;
				}
			}
		}
		if (!haveLoaded)
		{
			if (anyExist)
				LFPG_Util.Warn("[WireJournal] No readable journal candidate; starting empty");
		}
		else
		{
			if (loadedPath != LFPG_WIRE_JOURNAL_FILE)
			{
				string recMsg = "[WireJournal] Loaded recovery file ";
				recMsg = recMsg + loadedPath;
				LFPG_Util.Warn(recMsg);
				m_FileDirty = true;
			}
			int ri;
			for (ri = 0; ri < loadedStore.entries.Count(); ri = ri + 1)
			{
				LFPG_WireJournalEntry e = loadedStore.entries[ri];
				if (!e)
					continue;
				if (e.m_OwnerDeviceId == "")
					continue;
				m_Entries.Set(e.m_OwnerDeviceId, e);
			}
		}

		int restoredN = 0;
		int unchangedN = 0;
		int staleN = 0;
		int missingN = 0;
		int unusableN = 0;
		array<string> dropIds = new array<string>;
		int nowMs = 0;
		if (g_Game)
			nowMs = g_Game.GetTime();
		int ei;
		for (ei = 0; ei < m_Entries.Count(); ei = ei + 1)
		{
			string oid = m_Entries.GetKey(ei);
			LFPG_WireJournalEntry ent = m_Entries.Get(oid);
			if (!ent)
			{
				dropIds.Insert(oid);
				continue;
			}
			EntityAI obj = LFPG_DeviceRegistry.Get().FindById(oid);
			string L = OwnerLoadedJSON(obj);
			bool usable = false;
			if (obj)
			{
				if (OwnerUsable(obj))
				{
					if (L != "")
					{
						if (ent.m_WiresJSON != "")
							usable = true;
					}
				}
			}
			bool eqW = false;
			if (L == ent.m_WiresJSON)
				eqW = true;
			bool eqS = false;
			if (ent.m_SavedJSON != "")
			{
				if (L == ent.m_SavedJSON)
					eqS = true;
			}
			bool eqP = false;
			if (ent.m_SavedPrevJSON != "")
			{
				if (L == ent.m_SavedPrevJSON)
					eqP = true;
			}
			bool eqL = false;
			if (ent.m_LoadedJSON != "")
			{
				if (L == ent.m_LoadedJSON)
					eqL = true;
			}
			bool ownerFound = false;
			if (obj)
				ownerFound = true;
			int rv = LFPG_WireJournalReplayVerdict(ownerFound, usable, eqW, eqS, eqP, eqL);
			if (rv == LFPG_WJ_REPLAY_RESTORE)
			{
				array<ref LFPG_WireData> restoredWires = new array<ref LFPG_WireData>;
				if (!LFPG_WireHelper.DeserializeJSON(restoredWires, ent.m_WiresJSON, "WireJournal"))
				{
					unusableN = unusableN + 1;
					dropIds.Insert(oid);
					string uMsg = "[WireJournal] unusable ";
					uMsg = uMsg + oid;
					LFPG_Util.Debug(uMsg);
				}
				else
				{
					LFPG_DeviceAPI.ClearDeviceWires(obj);
					int added = 0;
					int rejected = 0;
					int wi;
					for (wi = 0; wi < restoredWires.Count(); wi = wi + 1)
					{
						if (LFPG_DeviceAPI.AddDeviceWire(obj, restoredWires[wi]))
							added = added + 1;
						else
							rejected = rejected + 1;
					}
					m_SeenEmit.Remove(oid);
					m_CapturedMs.Set(oid, nowMs);
					m_Restored.Insert(oid);
					restoredN = restoredN + 1;
					string rMsg = "[WireJournal] Restored ";
					rMsg = rMsg + oid;
					rMsg = rMsg + " wires=";
					rMsg = rMsg + added.ToString();
					rMsg = rMsg + " rejected=";
					rMsg = rMsg + rejected.ToString();
					LFPG_Util.Info(rMsg);
				}
			}
			else if (rv == LFPG_WJ_REPLAY_STALE)
			{
				string sMsg = "[WireJournal] Skipped ";
				sMsg = sMsg + oid;
				sMsg = sMsg + ": stored wires match no journal state";
				LFPG_Util.Warn(sMsg);
				staleN = staleN + 1;
				dropIds.Insert(oid);
			}
			else
			{
				if (rv == LFPG_WJ_REPLAY_MISSING)
					missingN = missingN + 1;
				else if (rv == LFPG_WJ_REPLAY_UNUSABLE)
					unusableN = unusableN + 1;
				else
					unchangedN = unchangedN + 1;
				dropIds.Insert(oid);
				string dMsg = "[WireJournal] drop ";
				dMsg = dMsg + oid;
				LFPG_Util.Debug(dMsg);
			}
		}
		int dj;
		for (dj = 0; dj < dropIds.Count(); dj = dj + 1)
		{
			DropEntry(dropIds[dj]);
			m_FileDirty = true;
		}
		int startN = restoredN + unchangedN + staleN + missingN + unusableN;
		string sum = "[WireJournal] Replay entries=";
		sum = sum + startN.ToString();
		sum = sum + " restored=";
		sum = sum + restoredN.ToString();
		sum = sum + " unchanged=";
		sum = sum + unchangedN.ToString();
		sum = sum + " stale=";
		sum = sum + staleN.ToString();
		sum = sum + " missing=";
		sum = sum + missingN.ToString();
		sum = sum + " unusable=";
		sum = sum + unusableN.ToString();
		LFPG_Util.Info(sum);
		#endif
	}

	void OnStartupValidationDone()
	{
		#ifdef SERVER
		int i;
		for (i = 0; i < m_Restored.Count(); i = i + 1)
		{
			m_Dirty.Set(m_Restored[i], true);
		}
		m_Restored.Clear();
		#endif
	}
};
