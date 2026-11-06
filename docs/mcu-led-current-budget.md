# MCU LED current budget

The control surface firmware caps the total LED current so a full white pad wall cannot brown out the board. This page shows the arithmetic. The code is `Firmware/kick-mcu/src/output/current_budget.cpp`, and `tests/current_budget_test.cpp` asserts every number below.

## Every input here is an assumption

I do not have the board, the LED parts, the driver chips or the supply rating. The defaults in `CurrentModel` are placeholders chosen to be plausible for 5 mm style RGB pad LEDs and small indicator LEDs. Replace them when the schematic exists. The code and the test change in one place (`CurrentModel`), and this page changes with them.

| Input | Value | Basis |
|---|---|---|
| Pad channel (R, G or B) at 100% duty | 20 mA | assumed typical full-scale LED current |
| Pads | 16, 3 channels each | the pad count is known, the channel count is the usual RGB |
| Button LED at 100% duty | 5 mA | assumed indicator LED |
| Button LEDs | 53 | `BtnLedSequence`, the host's LED table |
| Strip LED at 100% duty | 5 mA | assumed |
| Strip LEDs | 12 | assumed, the real count is not public |
| Total LED cap | 600 mA | assumed share of the 5 V rail; not measured and not from a datasheet |

The model has one rule: the current of a PWM-dimmed LED behind a constant-current driver is proportional to its duty. It ignores the driver's own quiescent current, supply ripple and any thermal derating.

## Worst case, no cap

Full duty on every channel gives:

- Pads: 16 pads x 3 channels x 20 mA = 960 mA
- Buttons: 53 x 5 mA = 265 mA
- Strip: 12 x 5 mA = 60 mA
- Everything lit white: 960 + 265 + 60 = 1285 mA

A white pad wall alone is 960 mA, 360 mA over the 600 mA cap.

## The cap

Each draw, the renderer sums the duty of every channel and button, converts that to mA with the numbers above (rounded up), and, if the total is over the cap, multiplies every duty by one factor.

The factor is `floor(cap * 256 / estimate)` in 1/256ths, so the scaled load never exceeds the cap:

- White pad wall alone: floor(600 x 256 / 960) = 160, so 160/256 = 62.5% of full duty, which is exactly 600 mA.
- Everything lit white: floor(600 x 256 / 1285) = 119, so 119/256 = 46.5% of full duty, which is 597.3 mA before rounding of the individual duties.
- Anything at or under 600 mA is left alone (factor 256).

Duties are scaled as `(duty * factor) >> 8`, which also rounds down. The test sweeps loads across the three outputs and checks that the estimate after scaling never exceeds the cap.

## Order of operations

1. Perceptual scaling (the sample-assigned dimming, the master brightness) happens on the colour, before gamma.
2. Gamma (default 2.2, adjustable from 1 to 4) turns that into a duty.
3. The current cap scales the duty. It acts after gamma because current follows duty, not perceived brightness.

One consequence: when the cap bites, a bright wall dims uniformly in duty, which after gamma looks a little darker than the linear ratio suggests. I did not compensate for that.

## What hardware has to settle

- The real channel currents. If the parts run at 10 mA the white wall is 480 mA and the cap never fires.
- The real supply headroom, which sets the cap.
- Whether the driver chips offer a global current setting that makes a firmware cap redundant or additive.
- The strip LED count and current.
- Inrush when all LEDs switch on at once. The model has no time dimension; the renderer does not ramp.
