# Persistence and notifications

Durable document storage with acknowledged flushes, and Android OS
notification scheduling. Read this when app data must survive a restart or a
game needs reminders.

## Document persistence

The `kryon/app_context` export provides `DocumentStorageReady() -> bool`
and `DocumentFlush() -> bool`, bound to `app_context:DocumentStorageReadyHost`
and `app_context:DocumentFlushHost` (both zero arguments, bool result).
`AppLocal` and the direct Canvas app host implement them. Other hosts must
explicitly grant and dispatch these capabilities; importing them grants no access.

Readiness is nonblocking: true requires an open private document namespace and,
in a browser, successful initial IndexedDB hydration. Before readiness, reads
report `DOCUMENT_ERROR`, writes and flush return false. The host opens storage
with `AppFilesOpen` after hydration; the guest does not initialize host storage.

`DocumentWrite`/`WriteStore` use queued browser semantics. A true write
result alone does not acknowledge persistence. After successful writes,
`DocumentFlush` returns true only when the backing store has acknowledged a
snapshot including those writes. It waits for any previous snapshot, then
requests and awaits a fresh one. Later writes remain in memory for a subsequent
flush; this operation neither clears a host's pending-write flag nor reloads
files from storage. Failure, unavailable storage, a 15-second browser deadline,
or namespace revocation returns false. False does not roll back queued bytes
or promise that an already running backend transaction has been cancelled.

The native provider syncs file bytes before atomic replacement and syncs parent
directories after rename, including new ancestors. Platforms/filesystems that
cannot sync directories reject that write rather than claim durability. The
browser reuses IDBFS and Asyncify; its acknowledgement is the successful IDBFS
transaction callback, subject to the browser's persistence/eviction policy.
Atomic replacements have distinct mtimes even within one millisecond, as IDBFS
uses mtimes to detect changes.

Hosts use `AppFilesReady(files)` and `AppFilesFlush(files)` for their authorized
`AppFiles`. The no-argument guest operation cannot name or read another node.
The browser backing store is the shared `/kryon-apps` mount: the host's
physical sync can include other queued namespaces, without exposing their paths,
bytes, handles, or authority to the guest. An AppFiles outside this mount can still stage packed assets, but reports
false for durable readiness and flush. Initial hydration is never restarted by flush.

Call on the owning thread. Browser flush yields; keep the owner and its AppFiles
alive until it returns and never reenter its running VM while suspended. Close
with `AppFilesClose` before changing account/resource ownership or rebinding the
namespace; close/open increments a generation, and an in-flight flush then
returns false even when its snapshot write succeeds. Do not overwrite live AppFiles
or mutate its root/lock directly. Readiness is availability, not an account grant.

`make -j1 document-storage-test` exercises native and actual disposable headless
IndexedDB storage, source/saved compilation and portable AppLocal dispatch.

## Local notifications

`kryon/notification_schedule` schedules Android OS reminders independently of
push, network access, and the running app loop. `DailyNotificationsSupported()`
reports scheduler support independently of permission. Other native platforms
return false; portable apps use their authenticated host capability.

| Call | Effect |
| --- | --- |
| `ScheduleDailyNotification(props, hour, minute=0, suppress_on_open=false, not_before_unix_ms=0, weekdays=127, skip_local_date=0)` | Persists one rule. Weekday bits are Sunday=1 through Saturday=64. Times use the device's current timezone; `not_before_unix_ms` is an absolute UTC lower bound. `skip_local_date` excludes one YYYYMMDD date, for example today's completion |
| `ScheduleNotificationAt(props, year, month, day, hour, minute=0)` | Schedules one local date; an elapsed date/time is rejected |
| `ScheduleNotificationDates(props, dates, hour, minute=0)` | Keeps one alarm for a caller-evaluated, finite sequence of at most 3660 strictly increasing valid YYYYMMDD dates. Apps own recurrence and refresh this explicit horizon |
| `CancelDailyNotification(tag, id)` | Cancels any of those rule types and its visible notification. Reusing `(tag,id)` replaces the rule |
| `NotificationAppOpened(tag,id)` | Durably marks that particular rule's local date and cancels its visible notice. Games call it on actual game entry, resume and local date changes while playing. A collection host must never mark its other games opened when the host foregrounds |

Scheduling persists even before permission is granted and never prompts.
Applications request permission through `notification.RequestNotifications`;
Kryon's Android host prompts once and preserves a refusal. System notification
settings remain authoritative. Failed scheduling returns false. Android's
inexact alarms can arrive later during idle; force-stopped applications cannot
receive alarms until relaunched. Reboot, clock, timezone and package-replacement
broadcasts rebuild future alarms from private preferences. Dates already opened
or delivered remain suppressed when the clock moves backward. Android allows at
most 480 active Kryon rules per host package and 64 per authenticated hosted app;
replacements do not consume another slot and cancellation frees it.

The Android scaffold declares the notification receiver, POST_NOTIFICATIONS
and RECEIVE_BOOT_COMPLETED permissions, and receiver actions.
Include `android_notifications` in the native graph so receiver exports survive
linking. `notification_desktop` dispatches immediate notification effects to the
Android implementation when ANDROID_BUILD is defined. Custom activities provide
`requestKryonNotifications()` using the scaffold's UI-thread callback.

In-app `Toast.Props.seconds` is independent: a toast's caller chooses display
length. OS notifications have no app-chosen toast deadline and remain subject
to Android's notification settings and user dismissal.
