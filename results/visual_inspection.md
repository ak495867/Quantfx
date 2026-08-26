# Visual inspection findings

The shortfall-by-broker figure is readable at 1980x1080 with clear pair labels, venue legend, and a consistent pips-per-unit axis. It shows BROKER_C below BROKER_B and BROKER_A for all four pairs under the configured candidate cost assumptions.

The routing-share figure is readable with normalized 0-to-1 stacked bars and venue legend. BROKER_C receives effectively all routed ticks because its configured lower modeled cost dominates the deterministic router. This is a model/configuration result rather than evidence that BROKER_C is superior in live markets; live quote fan-in and time-varying venue health are not included in this benchmark.

The regenerated shortfall figure is readable and correctly scaled at roughly 0.25–0.70 pips per unit. Its title now states that venue costs are deterministic modeled values, not observed broker quotes.

The regenerated routing figure is readable and explicitly titled as deterministic modeled allocation. The nearly full BROKER_C bars are visually accurate but demonstrate concentration caused by the selected synthetic venue parameters; A and B are present in the legend but receive no visible share at this scale.
