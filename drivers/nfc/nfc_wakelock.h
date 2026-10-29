/*
 * NFC Wakelock
 *
 * This program is free software; you can redistribute it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the License, or (at your
 * option) any later version.
 *
 */

#ifndef _NFC_WAKELOCK_H
#define _NFC_WAKELOCK_H

#include <linux/ktime.h>
#include <linux/device.h>
#include <linux/version.h>

#define wake_lock_init(a, b, c)	nfc_wake_lock_init(a, c)
#define wake_lock_destroy(a)	nfc_wake_lock_destroy(a)
#define wake_lock_timeout(a, b)	nfc_wake_lock_timeout(a, b)
#define wake_lock_active(a)	nfc_wake_lock_active(a)
#define wake_lock(a)		nfc_wake_lock(a)
#define wake_unlock(a)		nfc_wake_unlock(a)

struct nfc_wake_lock {
	struct wakeup_source *ws;
};

static inline void nfc_wake_lock_init(struct nfc_wake_lock *lock, const char *name)
{
	lock->ws = wakeup_source_register(NULL, name);
}

static inline void nfc_wake_lock_destroy(struct nfc_wake_lock *lock)
{
	if (lock->ws)
		wakeup_source_unregister(lock->ws);
}

static inline void nfc_wake_lock(struct nfc_wake_lock *lock)
{
	if (lock->ws)
		__pm_stay_awake(lock->ws);
}

static inline void nfc_wake_lock_timeout(struct nfc_wake_lock *lock, long timeout)
{
	if (lock->ws)
		__pm_wakeup_event(lock->ws, jiffies_to_msecs(timeout));
}

static inline void nfc_wake_unlock(struct nfc_wake_lock *lock)
{
	if (lock->ws)
		__pm_relax(lock->ws);
}

static inline int nfc_wake_lock_active(struct nfc_wake_lock *lock)
{
	return lock->ws ? lock->ws->active : 0;
}

#endif
