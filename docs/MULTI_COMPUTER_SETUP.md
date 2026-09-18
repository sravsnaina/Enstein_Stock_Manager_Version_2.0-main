# Running Stock Manager on several computers

All data lives in **one shared database**. One computer hosts it (the *server*);
every other computer runs the same `.exe` / binary and connects to it over the
LAN. Windows and Ubuntu machines can mix freely — the app speaks the same
PostgreSQL protocol from both.

Everything below is done from **Settings → Database Connection** inside the app.

## Before you start

- All computers must be on the same network (same Wi-Fi, or wired to the same
  router). A LAN cable and Wi-Fi on the same router count as the same network.
- Pick the computer that stays switched on as the server. Laptops that get
  closed or taken home make poor servers.
- You need the server computer's own login password once, for the install step.

## Step 1 — make one computer the server

On the computer that already has your work in it:

1. Open **Database Connection**.
2. Click **① Set up this computer as the server**, and enter your system
   password when asked.

This installs PostgreSQL, registers it as a service that survives reboots,
opens it to the LAN, creates the `stockmanager` database and a login, and opens
the firewall port. When it finishes, the dialog shows the **host, port,
database, user and password**.

> Write those five values down. The password is shown only once, and every other
> computer needs it.

## Step 2 — point the server computer at its own database

Still on the same computer, in section **②**:

1. Choose **PostgreSQL (connect to shared server)**.
2. The host/port/database/user/password fields are already filled in.
3. Click **Connect & Save**.

The status line must read `Connected: PostgreSQL (…)`. If it says it fell back
to a local file, the connection did not work — fix that before going further,
or your edits will not be shared.

## Step 3 — copy your existing work onto the server

The new database starts **empty**. Everything you did before this point is
still in this computer's local file, so it has to be copied up **once**:

In section **③**, click **Copy this computer's data to the server**.

This copies stock rows, item master, vendors, purchase orders and their line
items, GRNs, issue notes, the movement history and your user logins. It also
carries the numbering counters over, so if you were on `PO-0007` the next order
is `PO-0008` — numbering never restarts and never collides.

It is safe to click more than once: any table that already holds shared rows is
left untouched, so it can neither duplicate nor overwrite anything. Run it only
on the computer that has the existing work — running it on a fresh machine
copies nothing.

## Step 4 — connect every other computer

On each additional computer, install and run the same application, then:

1. Open **Database Connection**.
2. Choose **PostgreSQL (connect to shared server)**.
3. Type the **host, port, database, user and password** from Step 1. The host is
   the server computer's LAN address (for example `192.168.0.107`).
4. Click **Connect & Save**.

Do **not** run Step 1 or Step 3 on these machines. They only connect.

That computer now shows the same stock, the same purchase orders and the same
PO numbers as the server, and anything it changes is visible everywhere else.

## Edits appear on every machine on their own

Once connected, each copy watches the shared database and reloads within a few
seconds of anyone changing anything — stock, item master, vendors, purchase
orders, GRNs, issue notes, movement history. There is no button to press; the
status bar says *"Updated — someone else changed the shared data"* when it
happens. The Refresh button is still there if you want it immediately.

The check is one small query every 4 seconds, and only against a server — a
computer using its own local file has no other writers to watch.

### One caveat: the stock grid

Purchase orders, GRNs and issue notes are written row by row, so two people
working on different orders never interfere.

The **stock grid** is different: it is stored as one whole table, so saving it
writes every row. If two people edit the grid at the same moment, the app now
refuses the stale write rather than wiping the other person's rows — the person
who saved second sees *"Someone else changed the stock while you were editing"*,
their grid refreshes with the other version, and they re-enter their change. So
nothing is silently lost, but for bulk stock edits it is still best if one person
works the grid at a time.

## Checking it worked

- The status line in Database Connection reads
  `Connected: PostgreSQL (<server address>/stockmanager)` on every machine.
- The purchase order list shows the same orders everywhere.
- Create a PO on one computer; reopen the Purchase Orders screen on another and
  it is there, with the next number following on.

## If a computer cannot connect

| What you see | Usually means |
|---|---|
| `No route to host` | Different networks, or the server computer is off/asleep. |
| `Connection refused` | PostgreSQL is not running on the server, or the port is closed. |
| `password authentication failed` | The password was mistyped. It is case-sensitive. |
| Falls back to a local file | Any of the above — the app stays usable, but that computer's edits are **not** shared until it reconnects. |

The server's LAN address can change when the router hands out a new one. If
connections stop working after a reboot, re-check the address on the server
(shown in the Database Connection dialog) and update the clients — or give the
server a fixed address in your router.

## A note on backups

Everything now lives on the server computer. Its PostgreSQL data directory is
the thing worth backing up; the other computers hold no copy of record.
