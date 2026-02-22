// =========================================================
// LF_PowerGrid - core defines (v0.7.20)
// =========================================================

// ---- Anti-exploit hard caps (server authoritative) ----
static const int   LFPG_MAX_WAYPOINTS      = 20;
static const float LFPG_MAX_SEGMENT_LEN_M = 25.0;
static const float LFPG_MAX_WIRE_LEN_M    = 250.0;

// v0.7.12: Shared near-limit threshold (80% of max).
// Used by ConnectionRules for WARN_SEGMENT_NEAR_LIMIT / WARN_TOTAL_NEAR_LIMIT.
static const float LFPG_NEAR_LIMIT_RATIO  = 0.80;

// ---- Interaction / actions ----
static const float LFPG_INTERACT_DIST_M = 3.0;

static const int   LFPG_MAX_WIRES_PER_DEVICE = 64;
static const int   LFPG_MAX_WIRES_PER_PLAYER = 100;
static const float LFPG_RPC_COOLDOWN_S       = 0.50;

// ---- Rendering ----
static const float LFPG_CULL_DISTANCE_M    = 50.0;

// Device proximity bubble (v0.7.7):
// If the player is farther than this from BOTH endpoints of a wire,
// the wire is culled early. Acts as a tighter "relevance" radius.
// Overridden by LFPG_ServerSettings.DeviceBubbleM (configurable).
// Set to 0 to disable (falls back to LFPG_CULL_DISTANCE_M only).
static const float LFPG_DEVICE_BUBBLE_M    = 0.0;  // 0 = disabled by default

// Event-driven cable rendering (v0.7.2):
static const float LFPG_CULL_TICK_S        = 2.0;
static const float LFPG_RETRY_TICK_S       = 5.0;
static const int   LFPG_RETRY_MAX          = 12;

// Catenaria (cable sag simulation)
// v0.7.9: Adaptive subdivisions + quadratic sag scaling.
// Subdivisions are determined per-segment based on span length.
// Sag factor grows quadratically beyond SAG_QUAD_REF_M for realism.
static const float LFPG_SAG_FACTOR         = 0.08;
static const float LFPG_SAG_QUAD_REF_M     = 10.0;   // below this: linear sag, above: quadratic
static const float LFPG_SAG_SHORT_THRESH_M = 3.0;    // below this: no sag at all (cable looks taut)

// Wind sway (v0.7.9): subtle oscillation on intermediate cable points.
// Applied per-frame during projection. Endpoints (ports) and waypoints
// do NOT sway — only the sag interpolation points between them.
static const float LFPG_SWAY_AMPLITUDE     = 0.025;  // metres (vertical)
static const float LFPG_SWAY_SPEED         = 0.0008; // radians per millisecond (~0.8 Hz)

// ---- Cable state system (v0.7.8) ----
// Each committed wire has a visual state that determines color,
// and in the future, pattern (dash, pulse, etc.).
// States 0-3 are detectable now; 4-9 are future-ready (need server data).
enum LFPG_CableState
{
    IDLE             = 0,   // Connected, no power flowing
    POWERED          = 1,   // Connected, owner is active/energized
    RESOLVING        = 2,   // Target entity not yet loaded (network bubble edge)
    DISCONNECTED     = 3,   // Target lost / retry exhausted
    WARNING_LOAD     = 4,   // High load (future: needs load calculation)
    CRITICAL_LOAD    = 5,   // Near overload (future)
    ERROR_SHORT      = 6,   // Short circuit / electrical fault (future)
    ERROR_TOPOLOGY   = 7,   // Loop or invalid route (future)
    BLOCKED_LOGIC    = 8,   // Port/switch blocked (future)
    SELECTED         = 9    // Inspection/editing mode (future)
};

// v0.7.10: Retry reason for deferred wire builds.
// TARGET_MISSING: entity not yet loaded in network bubble.
// BUDGET: segment budget exceeded — wire is valid but skipped for capacity.
// Budget retries do NOT increment retryCount (they aren't failures).
enum LFPG_RetryReason
{
    TARGET_MISSING = 0,
    BUDGET         = 1
};

// State colors: ARGB format.
// Based on accessibility guidelines: color + pattern, not color alone.
// Palette designed for readability on DayZ's typically dark/outdoor environments.
static const int LFPG_STATE_COLOR_IDLE          = 0xFF7A7F87;  // neutral gray
static const int LFPG_STATE_COLOR_POWERED       = 0xFF2E9B59;  // green
static const int LFPG_STATE_COLOR_RESOLVING     = 0xFF6B7A99;  // blue-gray
static const int LFPG_STATE_COLOR_DISCONNECTED  = 0xFF6A625C;  // broken gray
static const int LFPG_STATE_COLOR_WARNING       = 0xFFD39B00;  // amber
static const int LFPG_STATE_COLOR_CRITICAL      = 0xFFE67E22;  // orange
static const int LFPG_STATE_COLOR_ERROR_SHORT   = 0xFFC94242;  // red
static const int LFPG_STATE_COLOR_ERROR_TOPO    = 0xFF8A3FFC;  // purple
static const int LFPG_STATE_COLOR_BLOCKED       = 0xFF6B7A99;  // blue-gray
static const int LFPG_STATE_COLOR_SELECTED      = 0xFF00B4D8;  // cyan

// ---- Cable rendering (v0.7.7+: Canvas 2D + LOD + depth width + alpha fade) ----
// Legacy color constants (kept for backward compat; state colors preferred).
static const int    LFPG_CABLE_COLOR         = 0xFF505050;
static const int    LFPG_CABLE_COLOR_OFF     = 0xFF303030;
static const float  LFPG_CABLE_WIDTH         = 2.5;

// LOD visual tiers (v0.7.7):
static const float LFPG_LOD_CLOSE_M         = 15.0;
static const float LFPG_LOD_MID_M           = 30.0;

// Shadow pass
static const float LFPG_SHADOW_WIDTH_ADD    = 2.0;
static const int   LFPG_SHADOW_COLOR        = 0x60000000;

// Highlight pass
static const float LFPG_HIGHLIGHT_WIDTH_SUB = 1.0;
static const int   LFPG_HIGHLIGHT_ALPHA     = 0x30;

// Depth-based width modulation (v0.7.7):
static const float LFPG_DEPTH_WIDTH_REF     = 8.0;
static const float LFPG_DEPTH_WIDTH_MIN     = 1.0;
static const float LFPG_DEPTH_WIDTH_MAX     = 6.0;

// Alpha fade by distance (v0.7.7):
static const float LFPG_ALPHA_FADE_START_M  = 35.0;

// ---- Joints and endcaps (v0.7.8, depth-scaled v0.7.9) ----
// Visual connectors at waypoints and port endpoints.
// Only drawn at LOD close (< LFPG_LOD_CLOSE_M).
// Sizes scale with depth (same reference as cable width).
static const float LFPG_JOINT_SIZE          = 4.0;   // cross half-size (px) at reference depth
static const float LFPG_JOINT_SIZE_MIN      = 2.0;   // minimum px
static const float LFPG_JOINT_SIZE_MAX      = 10.0;  // maximum px
static const float LFPG_ENDCAP_SIZE         = 5.0;   // tick half-length (px) at reference depth
static const float LFPG_ENDCAP_SIZE_MIN     = 2.0;
static const float LFPG_ENDCAP_SIZE_MAX     = 12.0;
static const float LFPG_ENDCAP_WIDTH        = 2.0;   // tick line width (px)

// ---- Pre-connection validation (v0.7.12 — Sprint 2, B2) ----
// Result status from LFPG_ConnectionRules.CanPreConnect().
// Values 0-9 = connectable (OK/warnings), 10+ = not connectable (invalid).
// Helper returns status + reason string, NEVER UI/color.
enum LFPG_PreConnectStatus
{
    OK                        = 0,   // Valid connection
    WARN_SEGMENT_NEAR_LIMIT   = 1,   // Segment at 80%+ of max (yellow)
    WARN_TOTAL_NEAR_LIMIT     = 2,   // Total wire at 80%+ of max (yellow)
    WARN_TOTAL_EXCEEDED       = 3,   // Total exceeds max (amber)
    NO_TARGET                 = 5,   // No device under cursor (grey)
    INVALID_SAME_DIRECTION    = 10,  // Both ports same dir (IN+IN / OUT+OUT)
    INVALID_SELF_CONNECTION   = 11,  // Source and target are same device
    INVALID_PORT_NOT_FOUND    = 12,  // Port doesn't exist on device
    INVALID_PORT_OCCUPIED     = 13,  // Target port already has a wire
    INVALID_SEGMENT_TOO_LONG  = 14,  // Last segment exceeds max
    INVALID_TOTAL_TOO_LONG    = 15,  // Total wire exceeds max
    INVALID_MAX_WAYPOINTS     = 16,  // Too many waypoints
    INVALID_NOT_SOURCE        = 17,  // Source endpoint is not a power source
    INVALID_NOT_CONSUMER      = 18,  // Target endpoint is not a consumer
    INVALID_DEVICE_IN_CARGO   = 19,  // Device is in inventory/cargo
    INVALID_GENERIC           = 20   // Catch-all
};

// Preview colors: UI interprets PreConnect status into ARGB colors.
// These live in Defines (not in the helper) because color = UI concern.
static const int LFPG_PREVIEW_COLOR_OK          = 0xFF00DD00;  // green
static const int LFPG_PREVIEW_COLOR_WARN        = 0xFFFFDD00;  // yellow
static const int LFPG_PREVIEW_COLOR_OVER        = 0xFFD39B00;  // amber (total exceeded)
static const int LFPG_PREVIEW_COLOR_INVALID     = 0xFFFF3333;  // red
static const int LFPG_PREVIEW_COLOR_NO_TARGET   = 0xFF888888;  // grey

// ---- Load calculation (v0.7.8) ----
// Used in PropagateFrom to determine WARNING/CRITICAL states.
// LoadRatio = totalConsumption / sourceCapacity.
static const float LFPG_LOAD_WARNING_THRESHOLD  = 0.80;   // >= 80% = WARNING
static const float LFPG_LOAD_CRITICAL_THRESHOLD = 1.00;   // >= 100% = CRITICAL

// Default values (used when device doesn't declare custom values).
// LFPG devices override via LFPG_GetCapacity / LFPG_GetConsumption.
// Vanilla devices: capacity read from CompEM.GetEnergyMax (as rate proxy),
//                  consumption from CompEM.GetEnergyUsage.
static const float LFPG_DEFAULT_SOURCE_CAPACITY    = 50.0;  // units/s
static const float LFPG_DEFAULT_CONSUMER_CONSUMPTION = 10.0; // units/s

// v0.7.10: Client-side wire count limit per owner (DecodeOwner).
// Guards against malformed/malicious JSON payloads flooding the client.
// Matches server-side LFPG_MAX_WIRES_PER_DEVICE by default.
static const int   LFPG_MAX_WIRES_PER_OWNER_CLIENT = 64;

// Hard cap: maximum total segments rendered per client.
static const int   LFPG_MAX_RENDERED_SEGS    = 512;

// ---- Occlusion (raycast-based Z-buffer emulation) ----
static const float LFPG_OCC_INTERVAL_MS      = 200.0;
static const int   LFPG_OCC_HYSTERESIS       = 2;
static const int   LFPG_OCC_MAX_RAYCASTS     = 20;
static const float LFPG_OCC_LONG_WIRE_M      = 10.0;
static const float LFPG_OCC_CAM_MOVE_THRESH  = 0.3;
static const float LFPG_OCC_CAM_DIR_THRESH   = 0.02;

// Forced occlusion recheck (v0.7.9): extra delay added to
// occNextCheckMs when camera is stationary. Catches moving
// objects (doors, vehicles) that may occlude/reveal cables
// while the player stands still. Combined with stagger,
// each wire rechecks every ~3s when camera is static.
static const float LFPG_OCC_FORCED_RECHECK_MS = 800.0;


// ---- Persistence (v0.7.15, Sprint 3) ----
// Schema versions for chained migration. Bump when adding/changing persisted fields.
// See LFPG_Migrators.c for migration chain and compatibility strategy.
static const int   LFPG_PERSIST_VER = 2;
static const int   LFPG_VANILLA_PERSIST_VER = 2;
static const float LFPG_VANILLA_FLUSH_S = 30.0;

// World coordinate bounds for waypoint validation (generous for custom maps up to 20km).
// v0.7.16: Absolute bounds are a secondary guard. Primary validation is inter-waypoint
// distance (LFPG_MAX_WIRE_LEN_M). Waypoints outside absolute bounds are discarded (not clamped).
static const float LFPG_COORD_MIN = -500.0;
static const float LFPG_COORD_MAX = 20500.0;

// ---- Port name constants (v0.7.16, H7) ----
// Centralized string constants reduce GC pressure from repeated literals
// and prevent typo-related bugs across the codebase.
static const string LFPG_PORT_OUTPUT_1 = "output_1";
static const string LFPG_PORT_INPUT_1  = "input_1";

// ---- RPC channel and sub-IDs ----
static const int   LFPG_RPC_CHANNEL = 259990;

enum LFPG_RPC_SubId
{
    FINISH_WIRING = 2,
    CUT_WIRES = 3,
    REQUEST_FULL_SYNC = 4,
    SYNC_OWNER_WIRES = 5,
    DIAG_CLIENT_LOG = 7,
    CLIENT_MSG = 8,
    CUT_PORT = 9
};

// ---- Telemetry (v0.7.13 — Sprint 2.5, G1/G5) ----
// Interval between telemetry log dumps (ms).
// 5000ms = every 5 seconds when active. Low overhead: only increments counters per-frame,
// dumps on interval, then resets. No allocations.
static const float LFPG_TELEM_INTERVAL_MS = 5000.0;

// ---- Logging ----
static const string LFPG_LOG_PREFIX = "[LF_PowerGrid] ";
static const bool   LFPG_LOG_ENABLED  = true;
static const int    LFPG_LOG_LEVEL    = 2;
static const bool   LFPG_DIAG_ENABLED = true;

// ---- Device types (Sprint 4.1) ----
// Determines node behavior in the electrical graph and future propagation.
enum LFPG_DeviceType
{
    UNKNOWN     = 0,   // Fallback — not classified
    SOURCE      = 1,   // Generates energy (generators)
    CONSUMER    = 2,   // Consumes energy (lamps, appliances)
    PASSTHROUGH = 3    // Retransmits energy (splitter, future switch/fuse)
};

// ---- Dirty reason masks (Sprint 4.2, active + differentiated) ----
// Bitwise masks for LFPG_ElecNode.m_DirtyMask.
// ProcessDirtyQueue uses these to optimize evaluation per node:
// TOPOLOGY: graph structure changed (wire added/removed) → full re-evaluation
// INPUT:    upstream power delivery changed → re-evaluate inputs from neighbors
// INTERNAL: device-local state changed (source toggled) →
//           sources skip input eval; non-sources treated as INPUT
static const int LFPG_DIRTY_TOPOLOGY = 1;
static const int LFPG_DIRTY_INPUT    = 2;
static const int LFPG_DIRTY_INTERNAL = 4;

// ---- Edge flags (Sprint 4.2, active — used in propagation) ----
static const int LFPG_EDGE_ENABLED    = 1;
static const int LFPG_EDGE_OVERLOADED = 2;

// ---- Propagation budget (Sprint 4.2, active) ----
// ProcessDirtyQueue processes at most NODE_BUDGET nodes per tick.
// At 10Hz tick rate: 640 nodes/second throughput (steady-state).
static const int LFPG_PROPAGATE_NODE_BUDGET   = 64;

// RESERVED (Sprint 4.3): Edge-level budget for inner loop of ProcessDirtyQueue.
// Not yet applied — ProcessDirtyQueue only budgets by node count.
// Will limit total edges visited per tick to prevent hot-node fan-out spikes.
static const int LFPG_PROPAGATE_EDGE_BUDGET   = 256;

// ACTIVE (Sprint 4.2 S2b, H3): Higher budget during warmup / self-heal drain.
// Used by TickPropagation when m_WarmupActive is true (after startup, bulk cut, self-heal).
// Reduces convergence latency from ~seconds to ~hundreds of ms for medium networks.
// At 10Hz: 1280 nodes/second throughput during warmup.
static const int LFPG_PROPAGATE_WARMUP_BUDGET = 128;

// ---- Cycle protection (Sprint 4.2, active) ----
// Max times a node can be re-enqueued within a single epoch (= one ProcessDirtyQueue call).
// Prevents infinite oscillation in feedback topologies.
// H7 note: "epoch" here means one tick-batch (one call to ProcessDirtyQueue),
// NOT a full propagation wave (which may span multiple ticks if queue > budget).
// Requeue counts are reset at the start of each new epoch.
static const int LFPG_MAX_REQUEUE_PER_EPOCH = 5;

// ---- Propagation epsilon (Sprint 4.2, active) ----
// Power changes smaller than epsilon do not propagate downstream.
// Prevents cascading updates from floating point noise.
static const float LFPG_PROPAGATION_EPSILON = 0.001;

// ---- Propagation tick interval (Sprint 4.2) ----
// Milliseconds between ProcessDirtyQueue calls. 100ms = 10Hz.
static const float LFPG_PROPAGATE_TICK_MS = 100.0;

// ---- Dirty queue compaction (Sprint 4.2 S2, H4) ----
// Head-index approach: entries are consumed from the front via m_DirtyQueueHead.
// Physical compaction only occurs when head exceeds this threshold.
// Prevents per-tick array copies while bounding memory growth.
static const int LFPG_DIRTY_QUEUE_COMPACT_THRESHOLD = 128;

// ---- Graph limits (Sprint 4.1, audit fix in 4.2) ----
// MAX_FANOUT_PER_PORT: max wires per output port (1 = point-to-point).
// MAX_EDGES_PER_NODE: total edges (in + out) per node.
// Sprint 4.2 fix (Audit #1): AddEdgeInternal now checks BOTH
// outgoing count on source AND incoming count on target.
static const int LFPG_MAX_FANOUT_PER_PORT = 1;
static const int LFPG_MAX_EDGES_PER_NODE  = 12;

static const string LFPG_VERSION_STR = "0.7.21";
