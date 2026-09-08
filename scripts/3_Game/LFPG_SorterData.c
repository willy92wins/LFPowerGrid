// =========================================================
// LF_PowerGrid - Sorter filter data model (v1.2.0 Sprint S1)
//
// Shared data classes for filter rules and output configuration.
// Used by:
//   - LFPG_SorterUI.c (client: build/edit filters)
//   - LFPG_Sorter.c (server: persist + evaluate, Sprint S2/S3)
//   - LFPG_TickSorters (server: evaluate items, Sprint S3)
//
// JSON serialization format (compact):
//   {"o":[
//     {"r":[{"t":0,"v":"WEAPON"},{"t":1,"v":"M4"}],"ca":false},
//     {"r":[],"ca":true}
//   ]}
//
// Layer: 3_Game (no 4_World dependencies).
// =========================================================

// ---------------------------------------------------------
// Filter type constants
// ---------------------------------------------------------
static const int LFPG_SORT_FILTER_CATEGORY = 0;
static const int LFPG_SORT_FILTER_PREFIX   = 1;
static const int LFPG_SORT_FILTER_CONTAINS = 2;
static const int LFPG_SORT_FILTER_RARITY   = 3; // FUTURE: not yet exposed in UI
static const int LFPG_SORT_FILTER_SLOT     = 4;

// ---------------------------------------------------------
// Category constants (match ResolveCategory output)
// ---------------------------------------------------------
static const string LFPG_SORT_CAT_WEAPON     = "WEAPON";
static const string LFPG_SORT_CAT_ATTACHMENT  = "ATTACHMENT";
static const string LFPG_SORT_CAT_AMMO       = "AMMO";
static const string LFPG_SORT_CAT_CLOTHING   = "CLOTHING";
static const string LFPG_SORT_CAT_FOOD       = "FOOD";
static const string LFPG_SORT_CAT_MEDICAL    = "MEDICAL";
static const string LFPG_SORT_CAT_TOOL       = "TOOL";
static const string LFPG_SORT_CAT_MISC       = "MISC";

// ---------------------------------------------------------
// Limits
// ---------------------------------------------------------
static const int LFPG_SORT_MAX_OUTPUTS        = 6;
static const int LFPG_SORT_MAX_RULES_PER_OUT  = 8;
static const int LFPG_SORT_MAX_JSON_BYTES     = 2048;

static const int LFPG_SORTER_ACK_NONE         = 0;
static const int LFPG_SORTER_ACK_KEPT_OLD     = 1;
static const int LFPG_SORTER_ACK_REPLACED     = 2;

// ---------------------------------------------------------
// Slot size presets
// ---------------------------------------------------------
static const string LFPG_SORT_SLOT_TINY   = "1-2";    // 1x1, 1x2
static const string LFPG_SORT_SLOT_SMALL  = "3-6";    // 2x2, 1x3..
static const string LFPG_SORT_SLOT_MEDIUM = "7-12";   // 2x4, 3x4..
static const string LFPG_SORT_SLOT_LARGE  = "13-50";  // big items

// ---------------------------------------------------------
// Single filter rule
// ---------------------------------------------------------
class LFPG_SortFilterRule
{
    int    m_Type;            // LFPG_SORT_FILTER_*
    string m_Value;           // original sanitized value (persistence/UI)
    string m_NormalizedValue; // lowercase once for runtime prefix/contains checks

    void LFPG_SortFilterRule()
    {
        m_Type = 0;
        m_Value = "";
        m_NormalizedValue = "";
    }

    // Check for duplicate: same type + same value
    bool Equals(int otherType, string otherValue)
    {
        if (m_Type != otherType)
            return false;
        if (m_Value != otherValue)
            return false;
        return true;
    }

    // Display label for UI tags/chips
    string GetDisplayLabel()
    {
        string label = "";
        if (m_Type == LFPG_SORT_FILTER_CATEGORY)
        {
            label = m_Value;
        }
        else if (m_Type == LFPG_SORT_FILTER_PREFIX)
        {
            label = "PRE:";
            label = label + m_Value;
        }
        else if (m_Type == LFPG_SORT_FILTER_CONTAINS)
        {
            label = "HAS:";
            label = label + m_Value;
        }
        else if (m_Type == LFPG_SORT_FILTER_RARITY)
        {
            label = "RAR:";
            label = label + m_Value;
        }
        else if (m_Type == LFPG_SORT_FILTER_SLOT)
        {
            label = "SLOT:";
            label = label + m_Value;
        }
        else
        {
            label = "?:";
            label = label + m_Value;
        }
        return label;
    }

    // Sanitize user input: strip characters that would break JSON
    // serialization (double quotes, backslash) and limit length.
    static string SanitizeValue(string raw)
    {
        if (raw == "")
            return "";

        string result = "";
        int rawLen = raw.Length();
        if (rawLen > 64)
        {
            rawLen = 64;
        }

        int ci;
        for (ci = 0; ci < rawLen; ci = ci + 1)
        {
            string ch = raw.Substring(ci, 1);
            if (ch == "\"")
                continue;
            if (ch == "\\")
                continue;
            if (ch == "{")
                continue;
            if (ch == "}")
                continue;
            if (ch == "[")
                continue;
            if (ch == "]")
                continue;
            result = result + ch;
        }

        return result;
    }
};

// ---------------------------------------------------------
// Per-output configuration (up to 8 rules, optional catch-all)
// ---------------------------------------------------------
class LFPG_SortOutputConfig
{
    ref array<ref LFPG_SortFilterRule> m_Rules;
    bool m_IsCatchAll;

    void LFPG_SortOutputConfig()
    {
        m_Rules = new array<ref LFPG_SortFilterRule>;
        m_IsCatchAll = false;
    }

    bool AddRule(int ruleType, string ruleValue)
    {
        // Enforce max rules
        if (m_Rules.Count() >= LFPG_SORT_MAX_RULES_PER_OUT)
            return false;

        // Sanitize value: strip quotes that would break JSON
        string safeValue = LFPG_SortFilterRule.SanitizeValue(ruleValue);
        if (safeValue == "")
        {
            // U2 (2026-04-26): admin sees in RPT why an Add silently failed.
            string wRaw = "[SortConfig] AddRule rejected: sanitized value is empty (raw='";
            wRaw = wRaw + ruleValue;
            wRaw = wRaw + "')";
            LFPG_Util.Warn(wRaw);
            return false;
        }

        // Check duplicate
        int i;
        for (i = 0; i < m_Rules.Count(); i = i + 1)
        {
            if (m_Rules[i].Equals(ruleType, safeValue))
                return false;
        }

        LFPG_SortFilterRule rule = new LFPG_SortFilterRule();
        rule.m_Type = ruleType;
        rule.m_Value = safeValue;
        rule.m_NormalizedValue = safeValue + "";
        if (ruleType == LFPG_SORT_FILTER_PREFIX || ruleType == LFPG_SORT_FILTER_CONTAINS)
        {
            rule.m_NormalizedValue.ToLower();
        }
        m_Rules.Insert(rule);
        return true;
    }

    void RemoveRuleAt(int index)
    {
        if (index < 0)
            return;
        if (index >= m_Rules.Count())
            return;
        m_Rules.Remove(index);
    }

    void ClearRules()
    {
        m_Rules.Clear();
        m_IsCatchAll = false;
    }

    int GetRuleCount()
    {
        return m_Rules.Count();
    }

    bool HasRule(int ruleType, string ruleValue)
    {
        int i;
        for (i = 0; i < m_Rules.Count(); i = i + 1)
        {
            if (m_Rules[i].Equals(ruleType, ruleValue))
                return true;
        }
        return false;
    }
};

// ---------------------------------------------------------
// Full filter configuration (6 outputs)
// ---------------------------------------------------------
class LFPG_SortConfig
{
    ref array<ref LFPG_SortOutputConfig> m_Outputs;

    void LFPG_SortConfig()
    {
        m_Outputs = new array<ref LFPG_SortOutputConfig>;
        int i;
        for (i = 0; i < LFPG_SORT_MAX_OUTPUTS; i = i + 1)
        {
            m_Outputs.Insert(new LFPG_SortOutputConfig());
        }
    }

    LFPG_SortOutputConfig GetOutput(int idx)
    {
        if (idx < 0)
            return null;
        if (idx >= m_Outputs.Count())
            return null;
        return m_Outputs[idx];
    }

    void ResetAll()
    {
        int i;
        for (i = 0; i < m_Outputs.Count(); i = i + 1)
        {
            m_Outputs[i].ClearRules();
        }
    }

    // ---- JSON serialization (compact format) ----
    // Format: {"o":[{"r":[{"t":0,"v":"WEAPON"}],"ca":false}, ...]}
    // F4-E: Custom IndexOfFrom removed — native string.IndexOfFrom(startPos, needle) used instead.

    string ToJSON()
    {
        string json = "{";
        json = json + "\"o\":[";

        int oi;
        for (oi = 0; oi < m_Outputs.Count(); oi = oi + 1)
        {
            if (oi > 0)
            {
                json = json + ",";
            }

            LFPG_SortOutputConfig outCfg = m_Outputs[oi];
            json = json + "{\"r\":[";

            int ri;
            for (ri = 0; ri < outCfg.m_Rules.Count(); ri = ri + 1)
            {
                if (ri > 0)
                {
                    json = json + ",";
                }
                LFPG_SortFilterRule rule = outCfg.m_Rules[ri];
                json = json + "{\"t\":";
                json = json + rule.m_Type.ToString();
                json = json + ",\"v\":\"";
                json = json + rule.m_Value;
                json = json + "\"}";
            }

            json = json + "],\"ca\":";
            if (outCfg.m_IsCatchAll)
            {
                json = json + "true";
            }
            else
            {
                json = json + "false";
            }
            json = json + "}";
        }

        json = json + "]}";
        return json;
    }

	// ---- JSON deserialization ----
	// Publish only a complete document; failed reads preserve the previous config.
	bool FromJSON(string json)
	{
		LFPG_SortConfig parsed = new LFPG_SortConfig();
		LFPG_SortConfigJSONReader reader = new LFPG_SortConfigJSONReader(json);
		if (!reader.ReadConfig(parsed))
			return false;

		m_Outputs = parsed.m_Outputs;
		return true;
	}

};

// Reader for the schema emitted by ToJSON. Every token is consumed at the
// cursor: a missing field cannot be borrowed from the next rule or output.
class LFPG_SortConfigJSONReader
{
	protected string m_JSON;
	protected int m_Pos;

	void LFPG_SortConfigJSONReader(string json)
	{
		m_JSON = json;
		m_Pos = 0;
	}

	protected void SkipWhitespace()
	{
		while (m_Pos < m_JSON.Length())
		{
			string ch = m_JSON.Substring(m_Pos, 1);
			if (ch != " " && ch != "\t" && ch != "\r" && ch != "\n")
				return;
			m_Pos = m_Pos + 1;
		}
	}

	protected bool Consume(string token)
	{
		SkipWhitespace();
		int tokenLen = token.Length();
		if (m_Pos + tokenLen > m_JSON.Length())
			return false;
		if (m_JSON.Substring(m_Pos, tokenLen) != token)
			return false;
		m_Pos = m_Pos + tokenLen;
		return true;
	}

	protected bool ReadString(out string value)
	{
		value = "";
		if (!Consume("\""))
			return false;
		int start = m_Pos;
		while (m_Pos < m_JSON.Length())
		{
			string ch = m_JSON.Substring(m_Pos, 1);
			if (ch == "\"")
			{
				if (m_Pos > start)
					value = m_JSON.Substring(start, m_Pos - start);
				m_Pos = m_Pos + 1;
				return true;
			}
			// Compact wire strings do not support escapes or raw JSON control bytes.
			if (ch == "\\" || ch.ToAscii() < 32)
				return false;
			m_Pos = m_Pos + 1;
		}
		return false;
	}

	protected bool ReadType(out int ruleType)
	{
		SkipWhitespace();
		int start = m_Pos;
		if (m_Pos < m_JSON.Length() && m_JSON.Substring(m_Pos, 1) == "-")
			m_Pos = m_Pos + 1;
		int digitsStart = m_Pos;
		string digits = "0123456789";
		while (m_Pos < m_JSON.Length())
		{
			string ch = m_JSON.Substring(m_Pos, 1);
			if (digits.IndexOf(ch) < 0)
				break;
			m_Pos = m_Pos + 1;
		}
		if (m_Pos == digitsStart)
			return false;
		if (m_Pos - digitsStart > 1 && m_JSON.Substring(digitsStart, 1) == "0")
			return false;
		string typeText = m_JSON.Substring(start, m_Pos - start);
		ruleType = typeText.ToInt();
		return true;
	}

	protected bool ReadRule(LFPG_SortOutputConfig output)
	{
		if (!Consume("{"))
			return false;
		bool hasType = false;
		bool hasValue = false;
		int ruleType = 0;
		string ruleValue = "";
		while (true)
		{
			string key;
			if (!ReadString(key) || !Consume(":"))
				return false;
			if (key == "t" && !hasType)
			{
				if (!ReadType(ruleType))
					return false;
				hasType = true;
			}
			else if (key == "v" && !hasValue)
			{
				if (!ReadString(ruleValue))
					return false;
				hasValue = true;
			}
			else
				return false;

			if (Consume("}"))
				break;
			if (!Consume(","))
				return false;
		}
		if (!hasType || !hasValue)
			return false;
		return output.AddRule(ruleType, ruleValue);
	}

	protected bool ReadRules(LFPG_SortOutputConfig output)
	{
		if (!Consume("["))
			return false;
		if (Consume("]"))
			return true;
		while (true)
		{
			if (!ReadRule(output))
				return false;
			if (Consume("]"))
				return true;
			if (!Consume(","))
				return false;
		}
		return false;
	}

	protected bool ReadOutput(LFPG_SortOutputConfig output)
	{
		if (!Consume("{"))
			return false;
		bool hasRules = false;
		bool hasCatchAll = false;
		while (true)
		{
			string key;
			if (!ReadString(key) || !Consume(":"))
				return false;
			if (key == "r" && !hasRules)
			{
				if (!ReadRules(output))
					return false;
				hasRules = true;
			}
			else if (key == "ca" && !hasCatchAll)
			{
				if (Consume("true"))
					output.m_IsCatchAll = true;
				else if (Consume("false"))
					output.m_IsCatchAll = false;
				else
					return false;
				hasCatchAll = true;
			}
			else
				return false;

			if (Consume("}"))
				return hasRules && hasCatchAll;
			if (!Consume(","))
				return false;
		}
		return false;
	}

	bool ReadConfig(LFPG_SortConfig config)
	{
		if (!Consume("{") || !Consume("\"o\"") || !Consume(":") || !Consume("["))
			return false;
		int outputIndex = 0;
		if (!Consume("]"))
		{
			while (true)
			{
				if (outputIndex >= LFPG_SORT_MAX_OUTPUTS)
					return false;
				LFPG_SortOutputConfig output = config.GetOutput(outputIndex);
				if (!ReadOutput(output))
					return false;
				outputIndex = outputIndex + 1;
				if (Consume("]"))
					break;
				if (!Consume(","))
					return false;
			}
		}
		if (!Consume("}"))
			return false;
		SkipWhitespace();
		return m_Pos == m_JSON.Length();
	}
};
