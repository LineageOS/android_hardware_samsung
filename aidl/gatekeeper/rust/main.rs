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

//! Default (insecure) implementation of the Gatekeeper HAL.
//!
//! This implementation of the HAL is only intended to allow testing and policy compliance.  A real
//! implementation **must implement the TA in a secure environment**, as per CDD 9.11 [C-1-3]: "MUST
//! perform the lock screen authentication in the isolated execution environment".
//!
//! The additional device-specific components that are required for a real implementation of
//! Gatekeeper that is based on the Rust reference implementation are described in
//! system/gatekeeper/rust/README.md.

use gk_hal::channel::SerializedChannel;
use log::{error, info, warn};
use std::fs;
use std::sync::{mpsc, Arc, Mutex};

/// Location of Gatekeeper failure records.  This directory must exist for this implementation of
/// Gatekeeper to run.
static GK_DIR: &str = "/data/vendor/gatekeeper/nonsecure";

/// Name of `IGatekeeper` binder device instance.
static GK_INSTANCE: &str = "default";
/// Name of Gatekeeper `ISharedSecret` binder device instance.
static SS_INSTANCE: &str = "gatekeeper";

static GK_SERVICE: &str = "android.hardware.gatekeeper.IGatekeeper";
static SECRET_SERVICE: &str = "android.hardware.security.sharedsecret.ISharedSecret";

/// Local error type for failures in the HAL service.
#[derive(Debug, Clone)]
struct HalServiceError(String);

impl From<String> for HalServiceError {
    fn from(s: String) -> Self {
        Self(s)
    }
}

fn main() {
    if let Err(HalServiceError(e)) = inner_main() {
        panic!("HAL service failed: {:?}", e);
    }
}

fn inner_main() -> Result<(), HalServiceError> {
    // Initialize Android logging.
    android_logger::init_once(
        android_logger::Config::default()
            .with_tag("gatekeeper-hal-nonsecure")
            .with_max_level(log::LevelFilter::Info)
            .with_log_buffer(android_logger::LogId::System),
    );
    // Redirect panic messages to logcat.
    std::panic::set_hook(Box::new(|panic_info| {
        error!("{}", panic_info);
    }));

    warn!("Insecure Gatekeeper HAL service is starting.");

    info!("Starting thread pool");
    binder::ProcessState::start_thread_pool();

    // Store failure records on disk under a pre-existing directory.
    //
    // This is insecure because it allows a root user in Android to reset failure counts, allowing
    // infinite password retries.
    let dir = std::path::PathBuf::from(GK_DIR);
    let exists = fs::exists(&dir)
        .map_err(|e| HalServiceError(format!("Failed to determine if {dir:?} exists: {e:?}")))?;
    if !exists {
        return Err(HalServiceError(format!(
            "Required directory {dir:?} does not exist!"
        )));
    }
    if !dir.is_dir() {
        return Err(HalServiceError(format!(
            "Required directory {dir:?} is not a directory!"
        )));
    }

    // Create a TA in-process, which acts as a local channel for communication.
    let channel = Arc::new(Mutex::new(LocalInsecureTa::new(dir)));

    let ss_service = gk_hal::sharedsecret::SharedSecretService::new_as_binder(channel.clone());
    let service_name = format!("{SECRET_SERVICE}/{SS_INSTANCE}");
    binder::add_service(&service_name, ss_service.as_binder()).map_err(|e| {
        HalServiceError(format!("Failed to register service {service_name}: {e:?}"))
    })?;

    let gk_service = gk_hal::GatekeeperService::new_as_binder(channel);
    let service_name = format!("{GK_SERVICE}/{GK_INSTANCE}");
    binder::add_service(&service_name, gk_service.as_binder()).map_err(|e| {
        HalServiceError(format!("Failed to register service {service_name}: {e:?}"))
    })?;

    info!("Successfully registered Gatekeeper HAL service");
    binder::ProcessState::join_thread_pool();
    info!("Gatekeeper HAL service is terminating"); // should not reach here
    Ok(())
}

/// Implementation of the Gatekeeper TA that runs locally in-process (and which is therefore
/// insecure).
#[derive(Debug)]
pub struct LocalInsecureTa {
    in_tx: mpsc::Sender<Vec<u8>>,
    out_rx: mpsc::Receiver<Vec<u8>>,
}

impl LocalInsecureTa {
    /// Create a new (insecure) instance.
    pub fn new(dir: std::path::PathBuf) -> Self {
        // Create a pair of channels to communicate with the TA thread.
        let (in_tx, in_rx) = mpsc::channel();
        let (out_tx, out_rx) = mpsc::channel();

        // The TA code expects to run single threaded, so spawn a thread to run it in.
        std::thread::spawn(move || {
            let mut ta = gk_ta_nonsecure::build_ta(&dir);
            loop {
                let req_data: Vec<u8> = in_rx.recv().expect("failed to receive next req");
                let rsp_data = ta.process(&req_data);
                out_tx.send(rsp_data).expect("failed to send out rsp");
            }
        });
        Self { in_tx, out_rx }
    }
}

impl SerializedChannel for LocalInsecureTa {
    const MAX_SIZE: usize = usize::MAX;

    fn execute(&mut self, req_data: &[u8]) -> binder::Result<Vec<u8>> {
        self.in_tx
            .send(req_data.to_vec())
            .expect("failed to send in request");
        Ok(self.out_rx.recv().expect("failed to receive response"))
    }
}
