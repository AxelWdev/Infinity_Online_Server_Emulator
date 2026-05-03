# Local Data Directory

This directory contains the starter CSV data used by the current public C++ server.

Included data:

- `setting/*.csv`: item, skill, character, and game item tables used by the catalog loaders.
- `missions/*.csv`: mission entity/layout data used by UDP mission sync.

Generated helper outputs such as `item_id_catalog.json`, `item_type_catalog.json`, and `package_contents.csv` are still ignored. Keep private exports or regenerated catalogs local unless you intentionally review and publish them.
