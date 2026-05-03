# Local Data Directory

This directory contains starter CSV data used by the current public C++ server.

Included data:

- `setting/*.csv`: item, skill, character, and game item tables used by the catalog loaders.
- `package_contents.csv`: package/bundle item expansion table used by shop purchases.
- `missions/mission_0_6.csv`: included mission entity/layout data.

The repo does not currently include a full `setting/mission.csv` mission catalog. Generated helper outputs such as `item_id_catalog.json` and `item_type_catalog.json` are still ignored. Keep private regenerated catalogs local unless you intentionally review and publish them.
