// =========================================================
// LF_PowerGrid - BTC Price Fetcher (Sprint BTC-1)
//
// Server-only singleton that periodically fetches the BTC
// price from CoinGecko (or configured API) using DayZ's
// native RestApi. Caches the result for UI queries.
//
// Architecture:
//   NetworkManager timer (60s) → LFPG_BTCPriceFetcher.Tick()
//     → RestContext.GET(callback, path)
//       → LFPG_BTCPriceCallback.OnSuccess(data)
//         → Parse JSON → update cached price
//
// The cached price is read by ATM RPC handlers when a player
// opens the ATM UI (on-demand, no broadcast needed).
//
// RestApi notes (DayZ engine):
//   - GetRestApi() / CreateRestApi() — engine singleton
//   - RestContext = GetRestApi().GetRestContext(baseUrl)
//   - ctx.GET(callback, path) — async, callback fires later
//   - ctx.GET_now(path) — sync (BLOCKS main thread, avoid)
//   - ctx.SetHeader("key: value") — set request headers
//   - RestCallback: OnSuccess(string data, int dataSize),
//     OnError(int errorCode), OnTimeout()
//   - Timeout limits: 3-120 seconds (engine-enforced)
//
// JSON parsing:
//   CoinGecko /simple/price response is predictable:
//   {"bitcoin":{"eur":60900}}
//   We extract the float value after the configured currency
//   key (e.g. "eur":) manually.
//   No generic JSON parser available in Enforce Script.
// =========================================================

// ---- RestCallback subclass for price fetch ----
class LFPG_BTCPriceCallback : RestCallback
{
    override void OnSuccess(string data, int dataSize)
    {
        LFPG_BTCPriceFetcher fetcher = LFPG_BTCPriceFetcher.Get();
        if (!fetcher)
            return;

        fetcher.HandlePriceResponse(data);
    }

    override void OnError(int errorCode)
    {
        string errMsg = "[LFPG_BTCPrice] API error code: ";
        errMsg = errMsg + errorCode.ToString();
        LFPG_Util.Warn(errMsg);

        LFPG_BTCPriceFetcher fetcher = LFPG_BTCPriceFetcher.Get();
        if (fetcher)
        {
            fetcher.HandlePriceError();
        }
    }

    override void OnTimeout()
    {
        string timeoutMsg = "[LFPG_BTCPrice] API request timed out";
        LFPG_Util.Warn(timeoutMsg);

        LFPG_BTCPriceFetcher fetcher = LFPG_BTCPriceFetcher.Get();
        if (fetcher)
        {
            fetcher.HandlePriceError();
        }
    }
};

// ---- Price Fetcher singleton ----
class LFPG_BTCPriceFetcher
{
    // Singleton
    protected static ref LFPG_BTCPriceFetcher s_Instance;

    // RestApi state
    protected ref RestApi m_RestApi;
    protected RestContext m_RestCtx;
    protected ref LFPG_BTCPriceCallback m_Callback;
    protected bool m_Initialized;

    // Cached price state
    protected float m_CachedPrice;          // Price per BTC in fiat (or LFPG_BTC_PRICE_UNAVAILABLE)
    protected float m_LastFetchTimeMs;       // GetGame().GetTickTime() of last successful fetch
    protected int m_ConsecutiveErrors;       // Error counter for backoff
    protected bool m_FetchInProgress;        // Guard against overlapping requests

    // ---- Constructor ----
    void LFPG_BTCPriceFetcher()
    {
        m_CachedPrice = LFPG_BTC_PRICE_UNAVAILABLE;
        m_LastFetchTimeMs = 0.0;
        m_ConsecutiveErrors = 0;
        m_FetchInProgress = false;
        m_Initialized = false;
        m_Callback = new LFPG_BTCPriceCallback();
    }

    // ---- Singleton accessor ----
    static LFPG_BTCPriceFetcher Get()
    {
        return s_Instance;
    }

    static void Create()
    {
        if (!s_Instance)
        {
            s_Instance = new LFPG_BTCPriceFetcher();
        }
    }

    static void Destroy()
    {
        s_Instance = null;
    }

    // ---- Initialize RestApi context ----
    // Must be called AFTER config is loaded (from NM constructor).
    void Init()
    {
        if (m_Initialized)
            return;

        #ifdef SERVER
        // Get or create the engine RestApi singleton
        m_RestApi = GetRestApi();
        if (!m_RestApi)
        {
            m_RestApi = CreateRestApi();
        }

        if (!m_RestApi)
        {
            string errNoApi = "[LFPG_BTCPrice] FATAL: Cannot create RestApi";
            LFPG_Util.Error(errNoApi);
            return;
        }

        // Create context with base URL from config
        string baseUrl = LFPG_BTCConfig.GetApiUrl();
        m_RestCtx = m_RestApi.GetRestContext(baseUrl);

        if (!m_RestCtx)
        {
            string errNoCtx = "[LFPG_BTCPrice] FATAL: Cannot create RestContext for ";
            errNoCtx = errNoCtx + baseUrl;
            LFPG_Util.Error(errNoCtx);
            return;
        }

        // Set headers if API key is configured
        string apiKey = LFPG_BTCConfig.GetApiKey();
        if (apiKey != "")
        {
            // CoinGecko Pro uses x-cg-pro-api-key header
            // CoinGecko Demo uses x-cg-demo-api-key header
            // We support both — user configures the appropriate one
            string header = "x-cg-demo-api-key: ";
            header = header + apiKey;
            m_RestCtx.SetHeader(header);

            string keyMsg = "[LFPG_BTCPrice] API key configured";
            LFPG_Util.Info(keyMsg);
        }

        // Set timeout (30 seconds)
        int timeoutOption = ERESTOPTION_READOPERATION;
        int timeoutValue = 30;
        m_RestApi.SetOption(timeoutOption, timeoutValue);

        m_Initialized = true;

        string initMsg = "[LFPG_BTCPrice] Initialized. Base URL: ";
        initMsg = initMsg + baseUrl;
        LFPG_Util.Info(initMsg);

        // Fetch immediately on init (don't wait for first tick)
        FetchPrice();
        #endif
    }

    // ---- Tick (called from NetworkManager timer) ----
    // Called every LFPG_BTC_PRICE_CHECK_MS (60s).
    void Tick()
    {
        #ifdef SERVER
        if (!m_Initialized)
            return;

        // Don't overlap requests
        if (m_FetchInProgress)
        {
            string busyMsg = "[LFPG_BTCPrice] Skipping tick (request in progress)";
            LFPG_Util.Debug(busyMsg);
            return;
        }

        FetchPrice();
        #endif
    }

    // ---- Issue async GET request ----
    protected void FetchPrice()
    {
        #ifdef SERVER
        if (!m_RestCtx)
        {
            string errNoCtx = "[LFPG_BTCPrice] Cannot fetch: no RestContext";
            LFPG_Util.Warn(errNoCtx);
            return;
        }

        if (!m_Callback)
        {
            string errNoCb = "[LFPG_BTCPrice] Cannot fetch: no callback";
            LFPG_Util.Warn(errNoCb);
            return;
        }

        string path = LFPG_BTCConfig.GetApiPath();
        m_FetchInProgress = true;

        string fetchMsg = "[LFPG_BTCPrice] Fetching: ";
        fetchMsg = fetchMsg + path;
        LFPG_Util.Debug(fetchMsg);

        m_RestCtx.GET(m_Callback, path);
        #endif
    }

    // ---- Handle successful API response ----
    // Called from LFPG_BTCPriceCallback.OnSuccess().
    void HandlePriceResponse(string data)
    {
        #ifdef SERVER
        m_FetchInProgress = false;

        if (!data || data == "")
        {
            string emptyMsg = "[LFPG_BTCPrice] Empty response";
            LFPG_Util.Warn(emptyMsg);
            HandlePriceError();
            return;
        }

        // Parse price from JSON response
        // Expected format: {"bitcoin":{"eur":60900}}
        float parsedPrice = ParsePriceFromJSON(data);

        if (parsedPrice <= 0.0)
        {
            string parseErr = "[LFPG_BTCPrice] Failed to parse price from response (len=";
            parseErr = parseErr + data.Length().ToString();
            parseErr = parseErr + ")";
            LFPG_Util.Warn(parseErr);
            HandlePriceError();
            return;
        }

        float prevPrice = m_CachedPrice;
        m_CachedPrice = parsedPrice;
        m_ConsecutiveErrors = 0;

        // Log the update
        string priceStr = FormatPrice(parsedPrice);
        string vsCurLog = LFPG_BTCConfig.GetVsCurrency();
        string okMsg = "[LFPG_BTCPrice] Updated: ";
        okMsg = okMsg + priceStr;
        okMsg = okMsg + " ";
        okMsg = okMsg + vsCurLog;

        if (prevPrice > 0.0)
        {
            string prevStr = FormatPrice(prevPrice);
            okMsg = okMsg + " (prev: ";
            okMsg = okMsg + prevStr;
            okMsg = okMsg + " ";
            okMsg = okMsg + vsCurLog;
            okMsg = okMsg + ")";
        }
        LFPG_Util.Info(okMsg);

        // Update last fetch time
        if (GetGame())
        {
            m_LastFetchTimeMs = GetGame().GetTickTime();
        }
        #endif
    }

    // ---- Handle API error (error or timeout) ----
    void HandlePriceError()
    {
        #ifdef SERVER
        m_FetchInProgress = false;
        m_ConsecutiveErrors = m_ConsecutiveErrors + 1;

        string errMsg = "[LFPG_BTCPrice] Consecutive errors: ";
        errMsg = errMsg + m_ConsecutiveErrors.ToString();

        // If we had a price before, keep using it (stale is better than nothing)
        if (m_CachedPrice > 0.0)
        {
            errMsg = errMsg + " (keeping stale price: ";
            string staleStr = FormatPrice(m_CachedPrice);
            errMsg = errMsg + staleStr;
            errMsg = errMsg + ")";
        }
        LFPG_Util.Warn(errMsg);
        #endif
    }

    // ---- JSON price parser ----
    // Parses CoinGecko response format: {"bitcoin":{"eur":NUMBER}}
    // The currency key (e.g. "eur") is read from config.
    // Returns the float price, or -1.0 on parse failure.
    protected float ParsePriceFromJSON(string data)
    {
        // Build search key from configured currency: "eur" → "\"eur\":"
        string vsCur = LFPG_BTCConfig.GetVsCurrency();
        string searchKey = "\"";
        searchKey = searchKey + vsCur;
        searchKey = searchKey + "\":";

        int keyPos = data.IndexOf(searchKey);
        if (keyPos < 0)
        {
            // Try without quotes (some formatters)
            string altKey = vsCur;
            altKey = altKey + ":";
            keyPos = data.IndexOf(altKey);
            if (keyPos < 0)
                return -1.0;

            int altKeyLen = altKey.Length();
            int altStart = keyPos + altKeyLen;
            string altRemainder = data.Substring(altStart, data.Length() - altStart);
            return ExtractFloatFromStart(altRemainder);
        }

        int keyLen = searchKey.Length();
        int valueStart = keyPos + keyLen;

        // Get the substring after the currency key
        int remainLen = data.Length() - valueStart;
        if (remainLen <= 0)
            return -1.0;

        string remainder = data.Substring(valueStart, remainLen);
        return ExtractFloatFromStart(remainder);
    }

    // Extract a float from the beginning of a string.
    // Reads digits and one decimal point, stops at any non-numeric char.
    // e.g. "67187.33}}" → 67187.33
    protected float ExtractFloatFromStart(string s)
    {
        if (!s || s == "")
            return -1.0;

        // Skip leading whitespace
        int start = 0;
        int sLen = s.Length();
        int ch;

        while (start < sLen)
        {
            ch = s.Get(start);
            // space=32, tab=9, newline=10, cr=13
            if (ch == 32 || ch == 9 || ch == 10 || ch == 13)
            {
                start = start + 1;
            }
            else
            {
                break;
            }
        }

        if (start >= sLen)
            return -1.0;

        // Find end of number: scan for first char that is NOT digit, '.', or '-'
        int endPos = start;
        bool hasDigit = false;

        while (endPos < sLen)
        {
            ch = s.Get(endPos);
            // '0'=48 .. '9'=57, '.'=46, '-'=45
            bool isNumChar = false;
            if (ch >= 48 && ch <= 57)
            {
                isNumChar = true;
                hasDigit = true;
            }
            if (ch == 46 || ch == 45)
            {
                isNumChar = true;
            }

            if (!isNumChar)
            {
                break;
            }

            endPos = endPos + 1;
        }

        if (!hasDigit)
            return -1.0;

        int numLen = endPos - start;
        if (numLen <= 0)
            return -1.0;

        string numStr = s.Substring(start, numLen);
        float result = numStr.ToFloat();
        return result;
    }

    // ---- Format price for logging (2 decimal places) ----
    protected string FormatPrice(float price)
    {
        // NaN guard
        if (price != price)
            return "NaN";

        if (price < 0.0)
            return "N/A";

        // Integer part
        int intPart = (int)price;

        // Decimal part (2 digits)
        float remainder = price - intPart;
        int decPart = (int)(remainder * 100.0);
        if (decPart < 0)
        {
            decPart = -decPart;
        }

        string result = intPart.ToString();
        result = result + ".";
        if (decPart < 10)
        {
            result = result + "0";
        }
        result = result + decPart.ToString();
        return result;
    }

    // ---- Public getters ----

    // Returns cached BTC price in configured fiat currency (default EUR).
    // Returns LFPG_BTC_PRICE_UNAVAILABLE (-1.0) if no price has been fetched.
    float GetCachedPrice()
    {
        return m_CachedPrice;
    }

    // Returns true if a valid price is available.
    bool IsPriceAvailable()
    {
        if (m_CachedPrice <= 0.0)
            return false;
        return true;
    }

    // Returns consecutive error count (0 = healthy).
    int GetConsecutiveErrors()
    {
        return m_ConsecutiveErrors;
    }

    // Returns true if the fetcher has been initialized.
    bool IsInitialized()
    {
        return m_Initialized;
    }
};
