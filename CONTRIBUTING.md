# Contributing

Keep changes focused and covered by the repository quality gates.

Before opening a pull request:

- Run `python tools/check_core_timing_guard.py`.
- Run `python tools/check_cli_contract.py`.
- Run `python -c "import json; json.load(open('library.json'))"`.
- Run `python -m platformio run -e ex_bringup_s3`.
- Run `python -m platformio run -e ex_bringup_s2`.
- Run `python -m platformio run -e ex_api_smoke_s3`.
- Keep public API changes reflected in `README.md` and `CHANGELOG.md`.
