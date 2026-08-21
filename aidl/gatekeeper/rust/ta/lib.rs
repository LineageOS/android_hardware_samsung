/*
 * Copyright (C) 2023 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//! Local in-process implementation of the KeyMint TA. This is insecure and should
//! only be used for testing purposes.

use gk_boringssl as boring;
use gk_ta::{traits, traits::SecureFilesystem, Error};
use gk_wire::MillisecondsSinceEpoch;
use log::{error, info, warn};
use std::fs;

/// Build a [`gk_ta::GatekeeperTa`] instance for nonsecure use.
pub fn build_ta(dir: &std::path::Path) -> gk_ta::GatekeeperTa {
    info!("Building NON-SECURE Gatekeeper Rust TA");

    let rng = boring::Rng;
    let clock = StdClock::default();
    let auth_key = traits::ExplicitAuthKey::new(Box::new(boring::HmacSha256));

    // Store failure records on the filesystem under the given directory. This is not secure.
    let std_fs = StdFilesystem {
        dir: std::path::PathBuf::from(dir),
    };

    // Pre-shared key of all-zeros for `ISharedSecret` agreement, matching:
    // - `kFakeAgreementKey` in `system/keymaster/km_openssl/soft_keymaster_enforcement.cpp`
    // - `Keys::kak` in `hardware/interfaces/security/keymint/aidl/default/ta/soft.rs`
    const SS_PRESHARED_KEY: traits::Aes256Key = [0; 32];

    let imp = traits::Implementation {
        rng: Box::new(rng),
        clock: Box::new(clock),
        compare: Box::new(boring::ConstEq),
        hmac: Box::new(boring::HmacSha256),
        password: Box::new(NonsecurePasswordKey),
        auth_key: Box::new(auth_key),
        failures: Box::new(std_fs),
        shared_secret: Some(traits::SharedSecretImplementation {
            preshared_key: Box::new(traits::FixedPresharedKey(SS_PRESHARED_KEY)),
            derive: Box::new(boring::BoringAesCmac),
        }),
    };
    gk_ta::GatekeeperTa::new(imp)
}

/// Monotonic clock which relies on Linux's clock_gettime(2) via `libc`.
#[derive(Default)]
pub struct StdClock;

impl traits::MonotonicClock for StdClock {
    fn now(&self) -> MillisecondsSinceEpoch {
        let mut time = libc::timespec {
            tv_sec: 0,
            tv_nsec: 0,
        };
        // Use `CLOCK_BOOTTIME` for consistency with the times used by the Cuttlefish C++
        // implementation of Gatekeeper (and because it includes time when the system is suspended,
        // unlike `CLOCK_MONOTONIC`).
        let rc =
        // SAFETY: `time` is a valid structure whose lifetime extends beyond the call, and has
        // exclusive mutable access.
            unsafe { libc::clock_gettime(libc::CLOCK_BOOTTIME, &mut time as *mut libc::timespec) };
        if rc < 0 {
            log::warn!("failed to get time!");
            return MillisecondsSinceEpoch(0);
        }
        MillisecondsSinceEpoch(((time.tv_sec * 1000) + (time.tv_nsec / 1000 / 1000)).into())
    }
}

/// Fake password key.
struct NonsecurePasswordKey;

impl traits::PasswordKeyRetrieval for NonsecurePasswordKey {
    fn key(&self) -> Result<traits::OpaqueOr<traits::HmacKey>, gk_ta::Error> {
        let fake_key = vec![0; 32];
        Ok(traits::OpaqueOr::Explicit(traits::HmacKey(fake_key)))
    }
}

/// Representation of a flat directory for files.
struct StdFilesystem {
    dir: std::path::PathBuf,
}

impl SecureFilesystem for StdFilesystem {
    type Iter = StdDirIterator;
    fn read(&self, filename: &str) -> Result<Vec<u8>, Error> {
        let mut path = self.dir.clone();
        path.push(filename);
        fs::read(&path).map_err(|e| {
            info!("failed to read {path:?}: {e:?}");
            Error::NotFound
        })
    }
    fn write(&self, filename: &str, data: &[u8]) -> Result<(), Error> {
        let mut path = self.dir.clone();
        path.push(filename);
        fs::write(&path, data).map_err(|e| {
            error!("failed to write to {path:?}: {e:?}");
            Error::Internal
        })
    }
    fn delete(&self, filename: &str) -> Result<(), Error> {
        let mut path = self.dir.clone();
        path.push(filename);
        fs::remove_file(&path).map_err(|e| {
            warn!("failed to delete {path:?}: {e:?}");
            Error::NotFound
        })
    }
    fn list(&self) -> Result<Self::Iter, Error> {
        let iter = fs::read_dir(&self.dir).map_err(|e| {
            error!("failed to list {:?}: {e:?}", self.dir);
            Error::Internal
        })?;
        Ok(StdDirIterator { iter })
    }
}

struct StdDirIterator {
    iter: std::fs::ReadDir,
}

impl Iterator for StdDirIterator {
    type Item = String;
    fn next(&mut self) -> Option<Self::Item> {
        let next = self.iter.next();
        match next {
            Some(Ok(entry)) => match entry.file_name().to_str() {
                Some(filename) => Some(filename.to_string()),
                None => {
                    error!("directory entry {entry:?} does not have a String filename!");
                    None
                }
            },
            Some(Err(e)) => {
                error!("failed to get next directory entry: {e:?}");
                None
            }
            None => None,
        }
    }
}
