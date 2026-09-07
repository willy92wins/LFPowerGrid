#!/usr/bin/env python3
"""T5 fault-injection driver: read a DayZ script log and decide if a case's
log oracle held. Does not talk to the game. Does not fill the matrix.

Exit codes:
  0 PASS
  1 FAIL (armed, fired if required, oracle missed or forbid hit)
  2 INCONCLUSIVE (gate off, JSON not armed, log missing, fire missing)
  3 usage / unknown case
"""
from __future__ import print_function

import argparse
import json
import os
import sys


def read_text(path):
    raw = open(path, "rb").read()
    if raw.startswith(b"\xff\xfe") or raw.startswith(b"\xfe\xff"):
        return raw.decode("utf-16", errors="replace")
    for enc in ("utf-8-sig", "utf-8", "cp1252", "latin-1"):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            continue
    return raw.decode("utf-8", errors="replace")


def load_cases(path):
    with open(path, "r", encoding="utf-8") as fh:
        data = json.load(fh)
    by_id = {}
    for row in data.get("cases", []):
        by_id[row["id"]] = row
    return data, by_id


def has(text, needle):
    return needle in text


def decide(case, blob):
    reasons = []
    injectable = bool(case.get("injectable"))
    require_fire = bool(case.get("require_fire"))
    require_crash = bool(case.get("require_crash"))
    require_observe = bool(case.get("require_observe"))
    needs_json = injectable or require_observe

    armed = has(blob, "LFPG_FAULTINJECT ARMED")
    disarmed = has(blob, "LFPG_FAULTINJECT DISARMED")
    fire = has(blob, "LFPG_FAULTINJECT FIRE")
    crash_now = has(blob, "LFPG_FAULTINJECT CRASH_NOW")

    if needs_json:
        if not armed:
            if disarmed:
                return "INCONCLUSIVE", ["JSON present but DISARMED (phrase/enabled/scenario)"]
            return "INCONCLUSIVE", [
                "no LFPG_FAULTINJECT ARMED in log: compile gate is off, or JSON was not read"
            ]
        if require_fire and not fire:
            return "INCONCLUSIVE", ["ARMED but no FIRE: action may not have reached the hook"]
        if require_crash and not crash_now:
            return "INCONCLUSIVE", ["ARMED but no CRASH_NOW: process may not have hit the window"]
        if require_observe and not has(blob, "LFPG_FAULTINJECT OBSERVE"):
            return "INCONCLUSIVE", ["ARMED but no OBSERVE: the RPC may not have completed"]

    for needle in case.get("expect_contains") or []:
        if not has(blob, needle):
            reasons.append("missing expected: " + needle)

    for needle in case.get("forbid_contains") or []:
        if has(blob, needle):
            reasons.append("forbidden present: " + needle)

    if reasons:
        return "FAIL", reasons
    return "PASS", ["all listed log oracles matched"]


def main(argv):
    here = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(description="T5 fault-injection log oracle")
    parser.add_argument("--case", help="case id from cases.json")
    parser.add_argument("--log", action="append", default=[], help="script log / RPT (repeatable)")
    parser.add_argument("--cases-file", default=os.path.join(here, "cases.json"))
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args(argv)

    data, by_id = load_cases(args.cases_file)

    if args.list:
        for row in data.get("cases", []):
            inj = "inject" if row.get("injectable") else "input"
            print("%s\t%s\t%s" % (row["id"], row.get("ficha"), inj))
        return 0

    if not args.case:
        print("usage: t5_fault_driver.py --case ID --log boot1.rpt [--log boot2.rpt]", file=sys.stderr)
        return 3
    if args.case not in by_id:
        print("unknown case: " + args.case, file=sys.stderr)
        return 3
    if not args.log:
        print("INCONCLUSIVE\tno log files given (instrument was not run here)")
        return 2

    chunks = []
    missing = []
    for path in args.log:
        if not os.path.isfile(path):
            missing.append(path)
            continue
        chunks.append(read_text(path))
    if missing:
        print("INCONCLUSIVE\tlog file missing: " + "; ".join(missing))
        return 2
    if not chunks:
        print("INCONCLUSIVE\tno readable logs")
        return 2

    blob = "\n".join(chunks)
    case = by_id[args.case]
    verdict, reasons = decide(case, blob)
    print(verdict + "\t" + args.case)
    for r in reasons:
        print("  " + r)
    if verdict == "PASS":
        return 0
    if verdict == "FAIL":
        return 1
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
