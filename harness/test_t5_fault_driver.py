#!/usr/bin/env python3
"""Oracle tests for t5_fault_driver.decide.

Synthetic logs come from harness-counterexamples.json (F02). The case
contract is E04_crash_after_credit in cases.json. Does not launch DayZ.

Run: python harness/test_t5_fault_driver.py
"""

import json
import os
import unittest

import t5_fault_driver


HERE = os.path.dirname(os.path.abspath(__file__))

# harness-counterexamples.json: positive_expected_markers, split as the
# brief requires (boot 1 = ARMED/FIRE/CRASH_NOW, boot 2 = OBSERVE).
BOOT1_POSITIVE = (
    "LFPG_FAULTINJECT ARMED scenario=E04_crash_after_credit\n"
    "LFPG_FAULTINJECT FIRE hook=E04_crash_after_credit\n"
    "LFPG_FAULTINJECT CRASH_NOW\n"
)
BOOT2_POSITIVE = (
    "LFPG_FAULTINJECT OBSERVE reconcile current=110 destroyed=10\n"
)

# harness-counterexamples.json: negative_missing_fire
LOG_MISSING_FIRE = (
    "LFPG_FAULTINJECT ARMED scenario=E04_crash_after_credit\n"
    "NO_FIRE hook=E04_crash_after_credit\n"
    "LFPG_FAULTINJECT CRASH_NOW\n"
    "LFPG_FAULTINJECT OBSERVE reconcile current=110 destroyed=10\n"
)

# harness-counterexamples.json: negative_wrong_scenario_zero_destroy_no_restart_evidence
LOG_WRONG_SCENARIO = (
    "LFPG_FAULTINJECT ARMED scenario=UNRELATED\n"
    "LFPG_FAULTINJECT OBSERVE reconcile current=0 destroyed=0\n"
    "LFPG_FAULTINJECT CRASH_NOW\n"
    "LFPG_FAULTINJECT FIRE hook=E04_crash_after_credit\n"
)

# Correct markers, FIRE before the matching ARMED.
LOG_FIRE_BEFORE_ARMED = (
    "LFPG_FAULTINJECT FIRE hook=E04_crash_after_credit\n"
    "LFPG_FAULTINJECT ARMED scenario=E04_crash_after_credit\n"
    "LFPG_FAULTINJECT CRASH_NOW\n"
    "LFPG_FAULTINJECT OBSERVE reconcile current=110 destroyed=10\n"
)

# R-02b: stale FIRE, then a complete chain after the matching ARMED.
BOOT1_STALE_FIRE_THEN_CHAIN = (
    "LFPG_FAULTINJECT FIRE hook=E04_crash_after_credit\n"
    "LFPG_FAULTINJECT ARMED scenario=E04_crash_after_credit\n"
    "LFPG_FAULTINJECT FIRE hook=E04_crash_after_credit\n"
    "LFPG_FAULTINJECT CRASH_NOW\n"
)

BOOT2_UNRELATED_THEN_OBSERVE = (
    "LFPG_FAULTINJECT ARMED scenario=UNRELATED\n"
    "LFPG_FAULTINJECT OBSERVE reconcile current=0 destroyed=0\n"
)

BOOT1_ARMED_SUFFIX_EXTRA = (
    "LFPG_FAULTINJECT ARMED scenario=E04_crash_after_credit-extra\n"
    "LFPG_FAULTINJECT FIRE hook=E04_crash_after_credit\n"
    "LFPG_FAULTINJECT CRASH_NOW\n"
)

BOOT1_DELETE_MARKER_FAIL = (
    "LFPG_FAULTINJECT ARMED scenario=E04_delete_marker_fail\n"
    "LFPG_FAULTINJECT FIRE hook=E04_clear_after_destroy\n"
    "sell-intent marker could not be cleared after destroy; rebased balanceBefore to current\n"
)


def load_case(case_id):
    path = os.path.join(HERE, "cases.json")
    with open(path, "r", encoding="utf-8") as fh:
        data = json.load(fh)
    for row in data.get("cases", []):
        if row["id"] == case_id:
            return row
    raise AssertionError(case_id + " missing from cases.json")


def load_e04():
    return load_case("E04_crash_after_credit")


class DecideE04(unittest.TestCase):
    def setUp(self):
        self.case = load_e04()

    def test_positive_split_across_two_logs_is_pass(self):
        verdict, reasons = t5_fault_driver.decide(
            self.case, [BOOT1_POSITIVE, BOOT2_POSITIVE])
        self.assertEqual(verdict, "PASS", reasons)

    def test_negative_missing_fire_is_inconclusive(self):
        verdict, reasons = t5_fault_driver.decide(self.case, LOG_MISSING_FIRE)
        self.assertEqual(verdict, "INCONCLUSIVE", reasons)

    def test_wrong_scenario_does_not_pass(self):
        verdict, reasons = t5_fault_driver.decide(self.case, LOG_WRONG_SCENARIO)
        self.assertNotEqual(verdict, "PASS", reasons)

    def test_fire_before_armed_is_fail(self):
        verdict, reasons = t5_fault_driver.decide(
            self.case, LOG_FIRE_BEFORE_ARMED)
        self.assertEqual(verdict, "FAIL", reasons)

    def test_armed_hyphen_suffix_is_inconclusive(self):
        # R-01
        verdict, reasons = t5_fault_driver.decide(
            self.case, [BOOT1_ARMED_SUFFIX_EXTRA, BOOT2_POSITIVE])
        self.assertEqual(verdict, "INCONCLUSIVE", reasons)

    def test_unrelated_armed_before_observe_does_not_pass(self):
        # R-02
        verdict, reasons = t5_fault_driver.decide(
            self.case, [BOOT1_POSITIVE, BOOT2_UNRELATED_THEN_OBSERVE])
        self.assertNotEqual(verdict, "PASS", reasons)
        self.assertEqual(verdict, "INCONCLUSIVE", reasons)

    def test_stale_fire_before_matching_armed_chain_is_pass(self):
        # R-02b
        verdict, reasons = t5_fault_driver.decide(
            self.case, [BOOT1_STALE_FIRE_THEN_CHAIN, BOOT2_POSITIVE])
        self.assertEqual(verdict, "PASS", reasons)

    def test_delete_marker_fail_two_logs_is_pass(self):
        # R-03: restart without require_crash does not need CRASH_NOW/OBSERVE
        case = load_case("E04_delete_marker_fail")
        verdict, reasons = t5_fault_driver.decide(
            case, [BOOT1_DELETE_MARKER_FAIL, ""])
        self.assertEqual(verdict, "PASS", reasons)

    def test_crash_and_observe_in_same_log_does_not_pass(self):
        # R-03: require_crash still needs OBSERVE in a later --log
        same_log = BOOT1_POSITIVE + BOOT2_POSITIVE
        verdict, reasons = t5_fault_driver.decide(
            self.case, [same_log, ""])
        self.assertNotEqual(verdict, "PASS", reasons)


if __name__ == "__main__":
    unittest.main(verbosity=2)
