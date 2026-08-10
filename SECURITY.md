# Security policy

Report security or safety-impacting defects privately to `info@thymos.cz`.
Include the affected version/commit, hardware and wiring when relevant,
reproduction steps, observed/expected behavior, and potential impact. Avoid
publishing exploitable details before maintainers have had a reasonable chance
to investigate and coordinate a fix.

This driver does not implement network input or machine-safety policy. Sensor
faults, stale data, transport failures, and offline state must still be mapped
to fail-safe application behavior by the integrating firmware.
