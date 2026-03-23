// =========================================================
// LF_PowerGrid - Sorter filter data model (v1.2.0 Sprint S1)
//
// Shared data classes for filter rules and output configuration.
// Used by:
//   - LFPG_SorterUI.c (client: build/edit filters)
//   - LF_Sorter.c (server: persist + evaluate, Sprint S2/S3)
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
    int    m_Type;   // LFPG_SORT_FILTER_*
    string m_Value;  // meaning depends on type

    void LFPG_SortFilterRule()
    {
        m_Type = 0;
        m_Value = "";
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
            return false;

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

    // Helper: IndexOf starting from a position (Enforce lacks this)
    static int IndexOfFrom(string haystack, int startPos, string needle)
    {
        int hLen = haystack.Length();
        if (startPos < 0)
            return -1;
        if (startPos >= hLen)
            return -1;
        int subLen = hLen - startPos;
        string sub = haystack.Substring(startPos, subLen);
        int idx = sub.IndexOf(needle);
        if (idx < 0)
            return -1;
        return startPos + idx;
    }

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
    // Returns true on success. On failure, config is left empty.
    bool FromJSON(string json)
    {
        ResetAll();

        if (json == "")
            return false;

        // Simple state-machine parser for our known compact format.
        // We know the structure exactly, so no need for a general JSON parser.
        // Finds each output block, then each rule within.

        int jsonLen = json.Length();
        if (jsonLen < 10)
            return false;

        // JSON needle strings (Enforce: no string literals as function params)
        string kOArr = "\"o\":[";
        string kRArr = "\"r\":[";
        string kOpenBrace = "{";
        string kCloseBrace = "}";
        string kCloseBracket = "]";
        string kTypeKey = "\"t\":";
        string kComma = ",";
        string kValKey = "\"v\":\"";
        string kQuote = "\"";
        string kCaKey = "\"ca\":";
        string kTrue = "true";

        // Find the outputs array start: "o":[
        int oArrStart = json.IndexOf(kOArr);
        if (oArrStart < 0)
            return false;

        int pos = oArrStart + 5;  // skip past "o":[
        int outIdx = 0;

        while (pos < jsonLen && outIdx < LFPG_SORT_MAX_OUTPUTS)
        {
            // Find next output object start {
            int objStart = IndexOfFrom(json, pos, kOpenBrace);
            if (objStart < 0)
                break;

            // Find the rules array "r":[
            int rArrStart = IndexOfFrom(json, objStart, kRArr);
            if (rArrStart < 0)
                break;

            int rPos = rArrStart + 5;  // skip past "r":[

            // Parse rules until ]
            while (rPos < jsonLen)
            {
                // Find next rule object {
                int ruleStart = IndexOfFrom(json, rPos, kOpenBrace);
                if (ruleStart < 0)
                    break;

                // Check if we hit ] before { (end of rules array)
                int rArrEnd = IndexOfFrom(json, rPos, kCloseBracket);
                if (rArrEnd >= 0 && rArrEnd < ruleStart)
                    break;

                // Parse "t":N (supports multi-digit types)
                int tStart = IndexOfFrom(json, ruleStart, kTypeKey);
                if (tStart < 0)
                    break;
                int tValStart = tStart + 4;
                if (tValStart >= jsonLen)
                    break;
                int tEnd = IndexOfFrom(json, tValStart, kComma);
                if (tEnd < 0)
                    break;
                int tLen = tEnd - tValStart;
                if (tLen <= 0)
                    break;
                string tStr = json.Substring(tValStart, tLen);
                int ruleType = tStr.ToInt();

                // Parse "v":"..."
                int vStart = IndexOfFrom(json, tValStart, kValKey);
                if (vStart < 0)
                    break;
                int vValStart = vStart + 5;
                if (vValStart >= jsonLen)
                    break;
                int vEnd = IndexOfFrom(json, vValStart, kQuote);
                if (vEnd < 0)
                    break;

                int vLen = vEnd - vValStart;
                string ruleValue = "";
                if (vLen > 0)
                {
                    ruleValue = json.Substring(vValStart, vLen);
                }

                // Add rule to current output
                LFPG_SortOutputConfig outCfg = GetOutput(outIdx);
                if (outCfg)
                {
                    outCfg.AddRule(ruleType, ruleValue);
                }

                // Skip past this rule's closing }
                int ruleEnd = IndexOfFrom(json, vEnd, kCloseBrace);
                if (ruleEnd < 0)
                    break;
                rPos = ruleEnd + 1;
            }

            // Parse catch-all: "ca":true/false
            int caStart = IndexOfFrom(json, rArrStart, kCaKey);
            if (caStart >= 0)
            {
                int caValStart = caStart + 5;
                if (caValStart + 4 <= jsonLen)
                {
                    string caVal = json.Substring(caValStart, 4);
                    LFPG_SortOutputConfig outCfgCa = GetOutput(outIdx);
                    if (outCfgCa)
                    {
                        if (caVal == kTrue)
                        {
                            outCfgCa.m_IsCatchAll = true;
                        }
                    }
                }
            }

            // Find end of this output object.
            // Search from rArrStart (always valid) to avoid using
            // caStart which may be -1 if "ca" was missing.
            // The closing } for this output is after both "r":[] and "ca":
            int searchFrom = rArrStart;
            if (caStart >= 0)
            {
                searchFrom = caStart;
            }
            int objEnd = IndexOfFrom(json, searchFrom, kCloseBrace);
            if (objEnd < 0)
                break;

            pos = objEnd + 1;
            outIdx = outIdx + 1;
        }

        return true;
    }

};
