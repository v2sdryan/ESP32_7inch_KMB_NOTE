# KMB touch favourites

Open the on-device settings panel and choose up to eight KMB routes. Each
favourite stores stop ID, route, bound, service type, stop name and optional
alias. Duplicate favourites and a ninth entry are rejected. Changes are saved
atomically to LittleFS and ETA refresh order follows the visible favourite
order.

If no favourite is configured, the bus panel displays the setup prompt. The
slideshow pauses while settings are open, so an in-progress edit cannot be
hidden by the automatic ten-second rotation.
