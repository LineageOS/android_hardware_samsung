/*
 * Copyright (C) 2021 The Android Open Source Project
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
 *
 * rebalance-interrupts:
 *
 * One-shot distribution of unassigned* IRQs to CPU cores.
 * Useful for devices with the ARM-GIC-v3, as the Linux driver will take any
 * interrupt assigned with an all-cores mask and always have it run on core 0.
 *
 * This should be run once, long enough after boot that all drivers have
 * registered their interrupts.
 *
 * This program is configured to spread the load across all the cores in
 * CPUFREQ policy 0.  This is because other cores may be hotplugged in
 * or out, and if hotplugged out the interrupts would be sent to core0 always.
 *
 * It might be wise to avoid core0 so that any later-added IRQs don't overcrowd
 * core 0.
 *
 * Any program that has an actual IRQ related performance constraint should
 * override any settings assigned by this and assign the IRQ to the same
 * core as the code whose performance is impacted by the IRQ.
 *
 */

#include <dirent.h>
#include <sys/types.h>

#include <iostream>
#include <list>
#include <map>
#include <vector>

#define LOG_TAG "rebalance_interrupts"

#include <android-base/file.h>
#include <android-base/format.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

#define POLICY0_CORES_PATH "/sys/devices/system/cpu/cpufreq/policy0/affected_cpus"
#define SYSFS_IRQDIR "/sys/kernel/irq"
#define PROC_IRQDIR "/proc/irq"

using android::base::ParseInt;
using android::base::ParseUint;
using android::base::ReadFileToString;
using android::base::Trim;
using android::base::WriteStringToFile;
using std::list;
using std::map;
using std::pair;
using std::string;
using std::vector;

// Return a vector of strings describing the affected CPUs for cpufreq
// Policy 0.
vector<int> Policy0AffectedCpus() {
    string policy0_cores_unparsed;
    if (!ReadFileToString(POLICY0_CORES_PATH, &policy0_cores_unparsed)) return vector<int>();
    string policy0_trimmed = android::base::Trim(policy0_cores_unparsed);
    vector<string> cpus_as_string = android::base::Split(policy0_trimmed, " ");

    vector<int> cpus_as_int;
    for (int i = 0; i < cpus_as_string.size(); ++i) {
        int cpu;
        if (!ParseInt(cpus_as_string[i].c_str(), &cpu)) return vector<int>();
        cpus_as_int.push_back(cpu);
    }
    return cpus_as_int;
}

// Return a vector of strings describing the CPU masks for cpufreq Policy 0.
vector<string> Policy0CpuMasks() {
    vector<int> cpus = Policy0AffectedCpus();
    vector<string> cpu_masks;
    for (int i = 0; i < cpus.size(); ++i) cpu_masks.push_back(fmt::format("{0:02x}", 1 << cpus[i]));
    return cpu_masks;
}

// Read the actions for the given irq# from sysfs, and add it to action_to_irq
bool AddEntryToIrqmap(const char* irq, map<string, list<string>>& action_to_irqs) {
    const string irq_base(SYSFS_IRQDIR "/");
    string irq_actions_path = irq_base + irq + "/actions";

    string irq_actions;
    if (!ReadFileToString(irq_actions_path, &irq_actions)) return false;

    irq_actions = Trim(irq_actions);

    if (irq_actions == "(null)") irq_actions = "";

    action_to_irqs[irq_actions].push_back(irq);

    return true;
}

// Get a mapping of driver "action" to IRQ#s for each IRQ# in
// SYSFS_IRQDIR.
bool GetIrqmap(map<string, list<string>>& action_to_irqs) {
    bool some_success = false;
    std::unique_ptr<DIR, decltype(&closedir)> irq_dir(opendir(SYSFS_IRQDIR), closedir);
    if (!irq_dir) {
        PLOG(ERROR) << "opening dir " SYSFS_IRQDIR;
        return false;
    }

    struct dirent* entry;
    while ((entry = readdir(irq_dir.get()))) {
        // If the directory entry isn't a parsable number, skip it.
        // . and .. get skipped here.
        unsigned throwaway;
        if (!ParseUint(entry->d_name, &throwaway)) continue;

        some_success |= AddEntryToIrqmap(entry->d_name, action_to_irqs);
    }
    return some_success;
}

// Given a map of irq actions -> IRQs,
// find out which ones haven't been assigned and add those to
// rebalance_actions.
void FindUnassignedIrqs(const map<string, list<string>>& action_to_irqs,
                        list<pair<string, list<string>>>& rebalance_actions) {
    for (const auto& action_to_irqs_entry : action_to_irqs) {
        bool rebalance = true;
        for (const auto& irq : action_to_irqs_entry.second) {
            string smp_affinity;
            string proc_path(PROC_IRQDIR "/");
            proc_path += irq + "/smp_affinity";
            ReadFileToString(proc_path, &smp_affinity);
            smp_affinity = Trim(smp_affinity);

            // Try to respect previoulsy set IRQ affinities.
            // On ARM interrupt controllers under Linux, if an IRQ is assigned
            // to more than one core it will only be assigned to the lowest core.
            // Assume any IRQ which is set to more than one core in the lowest four
            // CPUs hasn't been assigned and needs to be rebalanced.
            if (smp_affinity.back() == '0' || smp_affinity.back() == '1' ||
                smp_affinity.back() == '2' || smp_affinity.back() == '4' ||
                smp_affinity.back() == '8') {
                rebalance = false;
            }

            // Treat each unnamed action IRQ as independent.
            if (action_to_irqs_entry.first.empty()) {
                if (rebalance) {
                    pair<string, list<string>> empty_action_irq;
                    empty_action_irq.first = "";
                    empty_action_irq.second.push_back(irq);
                    rebalance_actions.push_back(empty_action_irq);
                }
                rebalance = true;
            }
        }
        if (rebalance && !action_to_irqs_entry.first.empty()) {
            rebalance_actions.push_back(
                    std::make_pair(action_to_irqs_entry.first, action_to_irqs_entry.second));
        }
    }
}

// Read the file at `path`, Trim whitespace, see if it matches `expected_value`.
// Print the results to stdout.
void ReportIfAffinityUpdated(const std::string expected_value, const std::string path) {
    std::string readback, report;
    ReadFileToString(path, &readback);
    readback = Trim(readback);
    if (readback != expected_value) {
        report += "Unable to set ";
    } else {
        report += "Success setting ";
    }
    report += path;
    report += ": found " + readback + " vs " + expected_value + "\n";
    LOG(DEBUG) << report;
}

// Evenly distribute the IRQ actions across all the Policy0 CPUs.
// Assign all the IRQs of an action to a single CPU core.
bool RebalanceIrqs(const list<pair<string, list<string>>>& action_to_irqs) {
    int mask_index = 0;
    std::vector<std::string> affinity_masks = Policy0CpuMasks();

    if (affinity_masks.empty()) {
        LOG(ERROR) << "Unable to find Policy0 CPUs for IRQ assignment.";
        return false;
    }

    for (const auto& action_to_irq : action_to_irqs) {
        for (const auto& irq : action_to_irq.second) {
            std::string affinity_path(PROC_IRQDIR "/");
            affinity_path += irq + "/smp_affinity";
            WriteStringToFile(affinity_masks[mask_index], affinity_path);
            ReportIfAffinityUpdated(affinity_masks[mask_index], affinity_path);
        }
        mask_index = (mask_index + 1) % affinity_masks.size();
    }
    return true;
}

void ChownIrqAffinity() {
    std::unique_ptr<DIR, decltype(&closedir)> irq_dir(opendir(PROC_IRQDIR), closedir);
    if (!irq_dir) {
        PLOG(ERROR) << "opening dir " PROC_IRQDIR;
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(irq_dir.get()))) {
        // If the directory entry isn't a parsable number, skip it.
        // . and .. get skipped here.
        unsigned throwaway;
        if (!ParseUint(entry->d_name, &throwaway)) continue;

        string affinity_path(PROC_IRQDIR "/");
        affinity_path += entry->d_name;
        affinity_path += "/smp_affinity";
        chown(affinity_path.c_str(), 1000, 1000);

        string affinity_list_path(PROC_IRQDIR "/");
        affinity_list_path += entry->d_name;
        affinity_list_path += "/smp_affinity_list";
        chown(affinity_list_path.c_str(), 1000, 1000);
    }
}

int main(int /* argc */, char* /* argv */[]) {
    map<string, list<string>> irq_mapping;
    list<pair<string, list<string>>> action_to_irqs;

    // Find the mapping of "irq actions" to IRQs.
    // Each IRQ has an assocatied irq_actions field, showing the actions
    // associated with it.  Multiple IRQs have the same actions.
    // Generate the mapping of actions to IRQs with that action,
    // as these IRQs should all be mapped to the same cores.
    if (!GetIrqmap(irq_mapping)) {
        LOG(ERROR) << "Unable to read IRQ mappings.  Are you root?";
        return 1;
    }

    // Change ownership of smp_affinity and smp_affinity_list handles
    // from root to system.
    ChownIrqAffinity();

    // Some IRQs are already assigned to a subset of cores, usually for
    // good reason (like some drivers have an IRQ per core, for per-core
    // queues.)  Find the set of IRQs that haven't been mapped to specific
    // cores.
    FindUnassignedIrqs(irq_mapping, action_to_irqs);

    // Distribute the rebalancable IRQs across all cores.
    return RebalanceIrqs(action_to_irqs) ? 0 : 1;
}
