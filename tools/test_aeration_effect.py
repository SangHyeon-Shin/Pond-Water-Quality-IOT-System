"""Tests for tools/aeration_effect.py (python3 -m unittest discover -s tools)."""
import math
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import aeration_effect as ae  # noqa: E402


def synthetic_csv(effect=1.0):
    """4 days, 10-minute samples, daily DO cycle, aerator on every other day (+effect mg/L)."""
    lines = ["time,do_mgl,aerator"]
    for step in range(4 * 24 * 6):
        hours = step / 6
        day = int(hours // 24)
        aerator = 1 if day % 2 == 1 else 0
        do = 8.0 + 2.0 * math.sin(2 * math.pi * (hours % 24 - 6) / 24) + (effect if aerator else 0.0)
        lines.append(f"{1_700_000_000 + int(hours * 3600)},{do:.4f},{aerator}")
    return lines


class AerationEffectTest(unittest.TestCase):
    def test_same_hour_difference_recovers_effect(self):
        samples = ae.read_samples(synthetic_csv(effect=1.0))
        self.assertEqual(len(samples), 4 * 24 * 6)
        diff, weight = ae.same_hour_difference(samples, "do_mgl")
        self.assertGreater(weight, 0)
        self.assertAlmostEqual(diff, 1.0, places=2)

    def test_rates_are_computed_per_state(self):
        samples = ae.read_samples(synthetic_csv(effect=0.0))
        rates = ae.rate_by_state(samples, "do_mgl")
        self.assertGreater(rates[0][0], 0)
        self.assertGreater(rates[1][0], 0)
        self.assertLess(abs(rates[0][1]), 0.1)  # a full daily cycle averages to ~0

    def test_gateway_log_with_ide_timestamps(self):
        log = [
            "Pond gateway (NodeMCU) 2.0.0",
            "# CSV,ms,seq,temp_c,ph,turbidity_ntu,tds_ppm,do_mgl,aerator,vcc_v,status_hex,loss_pct",
            "23:58:00.120 -> CSV,1000,1,22.00,7.20,12.0,150,6.50,1,5.000,0x0,0.0",
            "23:59:00.000 -> # alert: DO 3.2 mg/L",
            "23:59:30.456 -> CSV,61000,2,22.00,7.20,12.0,150,6.60,1,5.000,0x0,0.0",
            "00:00:30.000 -> CSV,121000,3,22.00,7.20,12.0,150,nan,255,5.000,0x10,0.0",
            "00:01:30.000 -> CSV,181000,4,21.90,7.10,12.0,150,7.10,0,5.000,0x0,0.0",
        ]
        samples = ae.read_samples(log)
        self.assertEqual([s.aerator for s in samples], [1, 1, 0])  # 255 = uncontrolled is skipped
        self.assertEqual(samples[0].hour, 23)
        self.assertEqual(samples[2].hour, 0)
        self.assertGreater(samples[2].t_hours, samples[1].t_hours)  # midnight handled
        self.assertAlmostEqual(samples[1].values["do_mgl"], 6.6)

    def test_report_runs_without_timestamps(self):
        log = ["CSV,1000,1,22.00,7.20,12.0,150,6.50,1,5.000,0x0,0.0",
               "CSV,61000,2,22.00,7.20,12.0,150,7.50,0,5.000,0x0,0.0"]
        text = ae.report(ae.read_samples(log), "do_mgl")
        self.assertIn("aerator ON", text)
        self.assertIn("n/a", text)


if __name__ == "__main__":
    unittest.main()
