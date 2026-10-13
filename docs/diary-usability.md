# Diary usability review and implementation

## Main findings

The old workflow asked people to understand files, keyboard shortcuts and
format choices before writing. Small list rows and keyboard-only confirmation
prompts made a touchscreen feel secondary. Local saving and remote sync were
also easy to confuse. The revised workflow starts with the day, keeps a
consistent touch navigation bar, and exposes technical features through Tools.

## Daily use

The shared status header shows date/time, battery and connected WiFi on every
touch-screen page, including writing and pairing. Tap it to adjust the clock;
open writing must save first and modal dialogs keep control until dismissed.
The editor has no default keyboard-shortcut legend. Actual editing shortcuts
remain available; save errors and other transient status messages still show.
Content and touch coordinates reserve the header's space in both orientations.

- Daily timeline: written moments and check-ins share the day's Markdown
  file. The day view lists level-two headings in file order, with excerpts
  and Earlier/Later paging. Existing unsectioned writing appears as Earlier
  writing; heading-like text inside fenced code is not a new moment. Read
  day still opens the complete document. Browsing uses a bounded streaming
  parser rather than loading a second whole document into memory.
- Add a moment: today's moment uses the local time at the moment you start
  it; another date uses Add to this day with no automatic time. The normal
  editor opens at the end, and inserts the heading only on the first
  nonblank input. Existing writing is preserved, an empty cancelled moment
  creates neither file nor metadata, and normal autosave/recovery applies
  after typing starts. Edit the generated Moment heading for an optional
  title. Times are ordinary Markdown and do not change on subsequent edits.
- Check in: mood (Good, Okay, Low, Mixed) and energy (Low, Steady, High)
  are optional touch choices, with no default selection. Tap a selected
  choice again to clear it. Save requires at least one choice; Skip creates
  nothing. Repeated check-ins append separate sections rather than replacing
  an earlier one. No keyboard, network, tracking chart or scoring is needed.
  Before Save, leaving the screen discards unsubmitted choices. If saving
  fails after insertion, the draft stays in the editor buffer and Save retries
  the same content instead of inserting it again. A later successful autosave
  can also persist that retained draft.
- Tap a timeline item to read that section, then Add a few words to continue
  it in the full document editor. The cursor is placed at the section's end;
  subsequent moments remain in the same file. You can also edit earlier text
  and its heading there. A keyboard is needed only for typing, not check-ins.

- Home: Continue writing, Today's entry, Calendar, My documents, Sync & backup,
  Settings. A random original wholesome quote comes from a fixed collection
  of 100 unique quotes. The clock/date opens touch date and time controls.
- Calendar: Monday-first month grid, previous/next month buttons, a star on
  dates with an entry, and a heavier border around today. Tap the month heading
  to jump to another month or year without stepping through every month. Tap any day to
  preview an entry or explicitly create one. Browsing an empty day does not
  create a file.
- Storage: one Markdown file per day, `YYYY-MM-DD.md`, at the SD card root.
  Existing matching files appear automatically. Other notes remain in My
  documents. Files stay compatible with the existing Git sync directory.
- Writing: a paired keyboard supplies text. Application navigation, actions,
  month selection, menus and confirmations use touch. Home and Settings have
  no persistent keyboard-selection highlight. Boot opens Home without pairing;
  BLE automatically scans and reconnects in the background. Write opens a
  waiting page when needed; Home leaves that page without stopping pairing.
  Pairing-code notices can be tapped to hide them while pairing continues.
  Navigation does not disconnect an existing keyboard. Device sleep or the
  keyboard's own sleep/range/power state can still drop the connection.
  Touch readers use the normal inactivity timer, not the no-keyboard timeout.
- Navigation: Home, Back, a contextual action (Today / Save / New note /
  Confirm), and Tools. Nested network, layout, restart and USB pickers have
  direct touch activation. Save/exit and delete choices have larger hit areas.
- Tools retains new notes, calendar, sync, settings, date/time, Advanced,
  undo/redo, copy/paste, find, sleep, selection, split views, pane focus, help,
  save as, rename, delete, and a manual display refresh. Text fields still use the paired keyboard;
  Confirm and Back are touch controls.
- Tools is grouped into Writing, Documents and Device. Technical actions stay
  under Device; Documents includes reading, journal and Privacy & recovery.
- Reading is read-only, with comfortable line spacing, normal/large text,
  touch scrolling and older/newer dated-entry controls. Files are read in
  4 KB UTF-8-boundary-safe chunks with Earlier/More text controls; no second
  full-document buffer is needed. Markdown source is shown as text, not a
  separate rich Markdown renderer. Continue and single-pane file taps open
  reading first; Write opens the editor. Split-pane file selection stays editable.
- Calendar's Journal list shows dated entries newest first with short excerpts.
  Day previews separate Read and Write. Optional prompts are available on empty
  days and in Writing tools; adding is explicit, with no streaks or scoring.

## Privacy, recovery and export

Before a changed editor save overwrites an existing file, its previous content
is copied under `.diary/versions/<timestamp>-<sequence>/<original-name>`.
If that copy fails, the save stops with the current writing still in memory.
Local versions cover editor saves, not every Git checkout or external USB edit.
Git's own history remains available on a computer. A first save of a new file
has no prior content to archive.

Delete moves content to `.diary/trash` before removing the original. Clean
open views are cleared so autosave cannot recreate a trashed note; modified
views must be saved first. The next Git sync can propagate the deletion, but
does not erase historical commits. Trash is local and is not uploaded by the
flat-directory Git client. Metadata such as cursor position is not restored.

Privacy & recovery lists versions, Trash and exports. Tap a row to read it or
Restore copy to create a new `recovered-*` note in My documents. Restoration
never overwrites an existing filename or changes the stored recovery copy.
The recovered note does not replace a dated calendar entry automatically.

Export all saves all open notes, then copies root-level Markdown, Fountain and
text documents into `.diary/exports`. It excludes credentials and Git metadata.
Copy that hidden folder off the SD card using a computer for an independent
backup. A failed export reports partial completion and leaves originals intact.
Local versions are full copies on each changed save, with no automatic pruning;
they consume card space. Explore -> Storage offers confirmed manual version
cleanup; manage Trash/export folders on a computer.

The destination screen strips URL user-info, query and fragment fields and
shows the repository, branch and subfolder. It does not verify repository
visibility: check server permissions yourself. There is no PIN lock or SD
encryption in this build. Local copies cannot protect against loss of the card.

## Data and time

Named documents autosave after at least three seconds without keyboard input,
checked every five seconds. Saves use the existing atomic SD write operation.
Failures leave the modified document open and report that it was not saved.
Switching to another diary entry first saves the current document, or opens
the naming prompt for an untitled document. Sync saves all open documents and
does not start if any could not be saved. Editing is held while Git sync runs,
and a sync completion does not reload over modified writing.

The BM8563 is read as UTC at boot. Invalid RTC readings do not silently become
today: the user is sent to Date & time. Touch controls set date, time and a
persisted UTC offset, and write UTC back to the hardware clock. Initial offset
is UTC. Manual offset remains available; diary5 also adds regional DST presets
and opt-in network time (see below). If the hardware clock
write fails, the UI explains that the setting only applies to this session.

Local save success is distinct from Git sync success. Sync & backup reports
progress and failures, provides WiFi setup and Draftbox setup, and retains
support for an existing `git.cfg`. Nothing is uploaded merely by opening an
entry. Use a repository whose access permissions suit a private diary.

## Rendering decisions

The original shell's outlined, rounded monochrome tiles informed the layout.
The AGENTS.md research reports severe ghosting from the custom-LUT integration;
that pipeline was not restored. No panel voltages, SPI rates, LUTs, refresh
thresholds or display-driver timings were changed for this diary work.

Safe UI-level work reduces needless updates: no keyboard tile focus repaint,
no continuously animated screens, one bounded press-feedback frame, cached
Home status text, stable quotes between visits, and coalesced intermediate Git
messages (at most once a second, with final results always delivered). Calendar
and Home invalidate their final content without an extra pre-clear request.
Existing full-refresh/ghosting safeguards and the manual Refresh action remain.
These are reductions in redraw requests, not a measured panel-speed claim.

## Validation and limits

Host tests cover every Gregorian date from 2020 through 2099, invalid diary
filenames, all 100 unique ASCII quotes, existing-entry preservation, SD read
and write failures, save-before-open guards, Home routes, touch timer cleanup,
and flasher regressions. Real LVGL host previews exercise pointer press/release,
Home, Settings, calendar, empty-day, clock and sync layouts in both orientations.
The host renderer uses a larger heap than LVGL's 64 KB default; firmware uses
the existing PSRAM-backed allocator.

Physical-device verification remains necessary for touch accuracy, RTC
persistence, SD removal during a save, real WiFi/Git failures, and ghosting.
There is one Markdown file per day containing multiple moments/check-ins,
no encryption-at-rest feature, and no migration of arbitrarily named notes
into calendar dates. Moments are displayed in file order (normally capture
order), not sorted by editable heading text. No claims of hardware timing improvement are made without
measurements on the device.

## Organisation, safety and personal preferences (diary5)

Journal -> Explore (also Tools -> Documents -> Explore diary) adds Bookmarks,
Topics, On this day, Revisit a day, Storage, Personalise, Time & zone and
Review conflicts. Home keeps its six tiles. All navigation uses touch;
entering prose or a personal prompt still requires a keyboard.

- Open a dated entry, then Tools -> Documents -> Bookmark & topics. The bookmark
  and eight optional topics apply to the whole day, not an individual moment.
  They live in a `<!-- diary: bookmark=1 tags=Family,Travel -->` Markdown comment
  and travel with Git sync and export. They are hidden from reading/excerpts.
- On this day lists the same month/day in earlier years (including exact leap
  day matching). Revisit opens a random dated entry only when deliberately tapped.
  No private writing appears automatically on Home.
- Sync & backup -> Save / backup status separates unsaved open notes, local days,
  and matches against the last known remote state. It is not a live server
  verification. Authentication/network failure does not erase local writing.
- Review conflicts offers Keep both, First version or Second version, shows the
  proposed result, then requires confirmation. A previous version is preserved;
  sync again afterward. Malformed/nested/diff3 conflicts and results exceeding
  the 4 KB review limit require a computer. Git history may retain both versions.
- Storage shows card capacity and space used by Versions, Trash and exports.
  Low-space warnings use 16 MiB or 5% free. Keep all is the default. Choosing
  20 or 50 versions per document only enables a manual Review cleanup action;
  nothing is automatically pruned. Confirmation permanently deletes older
  version copies on that card, never current entries, Trash or exports. Make
  an off-card backup first. Inventories over 10,000 copies stop cleanup safely.
- Time & zone offers manual UTC offset, UTC, UK/Ireland, Central Europe, US
  Eastern/Central/Mountain/Pacific and Sydney using explicit POSIX DST rules.
  This is a limited preset list, not a complete location database. Rule changes
  require a firmware update. The hardware clock remains UTC. Optional automatic
  network time is off by default; Sync time now is deliberate. Neither enables
  WiFi, and Git sync no longer independently changes the clock. Network time
  needs an existing connection and an idle Git sync; failed RTC writes are shown.
- Read a moment, then Tools -> Documents -> Move moment. Choose a destination
  day and confirm. The section is appended without replacing destination writing
  or changing its original time. Source/destination versions and a `.diary/move`
  journal protect retries. After interruption, the same command offers Retry
  recorded move. If either file changed outside that operation, retry stops:
  inspect the journal snapshots and files on a computer rather than overwriting.
- Settings -> Personalise diary controls reading size/spacing, quote favourites,
  hiding and quote-off, plus Add your own prompt. Quote/preferences are local NVS
  settings, not synced. Custom prompts live in `diary-prompts.txt`, one per line,
  at most 100 prompts of 256 bytes each, and participate in ordinary file sync.
  Use a computer to remove/reorder prompts. Built-in prompts remain available.

Label edits, conflict reads and moment moves are bounded to UTF-8, non-NUL
documents up to 128 KiB. Larger/UTF-16 documents remain normal editor documents
but these extra operations refuse them. Recovery copies use SD space and are
not encrypted or an independent backup. Preview renders real firmware layouts
with sample data; destructive, network and preference actions are illustrative.
No panel LUT, waveform, voltage or font asset changes are part of diary5.

Timezone rule reference: IANA tzdb `europe`, `northamerica`, `australasia`:
https://data.iana.org/time-zones/tzdb/ . Presets intentionally exclude regions
that differ from the named representative zone (for example, Arizona).

