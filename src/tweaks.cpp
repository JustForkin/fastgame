#include "tweaks.hpp"
#include <sys/resource.h>
#include "ioprio.hpp"

namespace fs = std::filesystem;

Tweaks::Tweaks(Config* config)
    : cfg(config), scheduler(std::make_unique<Scheduler>()), radeon(std::make_unique<Radeon>()) {
#ifdef USE_NVIDIA
  nvidia = std::make_unique<Nvidia>();
#endif
}

void Tweaks::apply_global() {
  auto governor = cfg->get_key<std::string>("general.cpu.governor.game", "performance");
  auto sched_child_runs_first = cfg->get_key("general.cpu.scheduler.sched_child_runs_first.game", -1);

  auto hugepage_state = cfg->get_key<std::string>("general.memory.hugepage.game", "");
  auto hugepage_defrag = cfg->get_key<std::string>("general.memory.hugepage.defrag.game", "");
  auto hugepage_shmem_enabled = cfg->get_key<std::string>("general.memory.hugepage.shmem_enabled.game", "");

  auto vfs_cache_pressure = cfg->get_key("general.memory.vm.vfs_cache_pressure.game", -1);

  auto disk_scheduler = cfg->get_key<std::string>("general.disk.scheduler.game", "");
  auto disk_read_ahead = cfg->get_key("general.disk.read-ahead.game", -1);
  auto disk_nr_requests = cfg->get_key("general.disk.nr-requests.game", -1);
  auto disk_rq_affinity = cfg->get_key("general.disk.rq-affinity.game", -1);

  change_cpu_governor(governor);
  change_cfs_parameter("sched_child_runs_first", sched_child_runs_first);
  set_hugepages(hugepage_state, hugepage_defrag, hugepage_shmem_enabled);
  change_vm_parameter("vfs_cache_pressure", vfs_cache_pressure);
  change_disk_parameter("scheduler", disk_scheduler);
  change_disk_parameter("read_ahead_kb", disk_read_ahead);
  change_disk_parameter("nr_requests", disk_nr_requests);
  change_disk_parameter("rq_affinity", disk_rq_affinity);

  set_cpu_dma_latency(0);

  if (radeon->has_gpu()) {
    auto power_dpm_force_performance_level =
        cfg->get_key<std::string>("general.radeon.power_dpm_force_performance_level.game", "auto");

    auto hwmon_index = cfg->get_key("general.radeon.hwmon.index", -1);
    auto power1_cap = cfg->get_key("general.radeon.hwmon.power1_cap.game", -1);

    radeon->set_power_dpm_force_performance_level(power_dpm_force_performance_level);
    radeon->set_power_cap(hwmon_index, power1_cap);
  }

#ifdef USE_NVIDIA
  if (nvidia->has_gpu()) {
    auto gpu_offset = cfg->get_key("general.nvidia.clock-offset.gpu.game", 0);
    auto memory_offset = cfg->get_key("general.nvidia.clock-offset.memory.game", 0);
    auto powermizer_mode = cfg->get_key<std::string>("general.nvidia.powermizer-mode.game", "auto");
    auto power_limit = cfg->get_key("general.nvidia.power-limit.game", -1);

    nvidia->set_powermizer_mode(0, powermizer_mode);
    nvidia->set_clock_offset(0, gpu_offset, memory_offset);
    nvidia->nvml->set_power_limit(0, power_limit);
  }
#endif
}

void Tweaks::apply_process(const std::string& game, const int& pid) {
  change_niceness(game, pid);
  change_scheduler_policy(game, pid);
  change_iopriority(game, pid);

  auto mask = cfg->get_profile_key_array<int>(game, "cpu-affinity");

  scheduler->set_affinity(pid, mask);
}

void Tweaks::remove() {
  auto governor = cfg->get_key<std::string>("general.cpu.governor.default", "schedutil");
  auto sched_child_runs_first = cfg->get_key("general.cpu.scheduler.sched_child_runs_first.default", -1);

  auto hugepage_state = cfg->get_key<std::string>("general.memory.hugepage.default", "");
  auto hugepage_defrag = cfg->get_key<std::string>("general.memory.hugepage.defrag.default", "");
  auto hugepage_shmem_enabled = cfg->get_key<std::string>("general.memory.hugepage.shmem_enabled.default", "");

  auto vfs_cache_pressure = cfg->get_key("general.memory.vm.vfs_cache_pressure.default", -1);

  auto disk_scheduler = cfg->get_key<std::string>("general.disk.scheduler.default", "");
  auto disk_read_ahead = cfg->get_key("general.disk.read-ahead.default", -1);
  auto disk_nr_requests = cfg->get_key("general.disk.nr-requests.default", -1);
  auto disk_rq_affinity = cfg->get_key("general.disk.rq-affinity.default", -1);

  change_cpu_governor(governor);
  change_cfs_parameter("sched_child_runs_first", sched_child_runs_first);
  set_hugepages(hugepage_state, hugepage_defrag, hugepage_shmem_enabled);
  change_vm_parameter("vfs_cache_pressure", vfs_cache_pressure);
  change_disk_parameter("scheduler", disk_scheduler);
  change_disk_parameter("read_ahead_kb", disk_read_ahead);
  change_disk_parameter("nr_requests", disk_nr_requests);
  change_disk_parameter("rq_affinity", disk_rq_affinity);

  if (cpu_dma_ofstream.is_open()) {
    cpu_dma_ofstream.close();
  }

  if (radeon->has_gpu()) {
    auto power_dpm_force_performance_level =
        cfg->get_key<std::string>("general.radeon.power_dpm_force_performance_level.default", "auto");

    auto hwmon_index = cfg->get_key("general.radeon.hwmon.index", -1);
    auto power1_cap = cfg->get_key("general.radeon.hwmon.power1_cap.default", -1);

    radeon->set_power_dpm_force_performance_level(power_dpm_force_performance_level);
    radeon->set_power_cap(hwmon_index, power1_cap);
  }

#ifdef USE_NVIDIA
  if (nvidia->has_gpu()) {
    auto gpu_offset = cfg->get_key("general.nvidia.clock-offset.gpu.default", 0);
    auto memory_offset = cfg->get_key("general.nvidia.clock-offset.memory.default", 0);
    auto powermizer_mode = cfg->get_key<std::string>("general.nvidia.powermizer-mode.default", "auto");
    auto power_limit = cfg->get_key("general.nvidia.power-limit.default", -1);

    nvidia->set_powermizer_mode(0, powermizer_mode);
    nvidia->set_clock_offset(0, gpu_offset, memory_offset);
    nvidia->nvml->set_power_limit(0, power_limit);
  }
#endif
}

void Tweaks::change_cpu_governor(const std::string& name) {
  auto path = fs::path("/sys/devices/system/cpu/present");

  std::ostringstream stream;

  stream << std::ifstream(path).rdbuf();

  auto out = stream.str();

  out.erase(0, 2);  // remove the characters 0-

  int ncores = std::stoi(out);

  for (int n = 0; n < ncores; n++) {
    std::ofstream f;
    auto f_path = "/sys/devices/system/cpu/cpu" + std::to_string(n) + "/cpufreq/scaling_governor";

    f.open(f_path);

    f << name;

    f.close();
  }

  std::cout << log_tag + "changed cpu frequency governor to: " << name << std::endl;
}

void Tweaks::change_cfs_parameter(const std::string& name, const int& value) {
  if (value == -1) {
    return;
  }

  auto path = fs::path("/proc/sys/kernel/" + name);

  std::ofstream f;

  f.open(path);

  f << value;

  f.close();

  std::cout << log_tag + "changed cfs parameter " + name + " to: " << value << std::endl;
}

void Tweaks::change_niceness(const std::string& game, const int& pid) {
  auto niceness = cfg->get_profile_key(game, "niceness", 0);

  setpriority(PRIO_PROCESS, pid, niceness);
}

void Tweaks::change_scheduler_policy(const std::string& game, const int& pid) {
  auto sched_policy = cfg->get_profile_key<std::string>(game, "scheduler-policy", "invalid");

  auto sched_priority = cfg->get_profile_key(game, "scheduler-policy-priority", 0);

  if (sched_policy != "invalid") {
    scheduler->set_policy(pid, sched_policy, sched_priority);
  }
}

void Tweaks::change_iopriority(const std::string& game, const int& pid) {
  auto io_class = cfg->get_profile_key<std::string>(game, "io-class", "invalid");

  auto io_priority = cfg->get_profile_key(game, "io-priority", 7);

  ioprio_set(pid, io_class, io_priority);
}

void Tweaks::set_hugepages(const std::string& state, const std::string& defrag, const std::string& shmem_enabled) {
  if (state == "") {
    return;
  }

  auto path = fs::path("/sys/kernel/mm/transparent_hugepage/enabled");

  std::ofstream f;

  f.open(path);

  f << state;

  std::cout << log_tag + "changed transparent hugepage state to: " << state << std::endl;

  f.close();

  if (defrag == "") {
    return;
  }

  path = fs::path("/sys/kernel/mm/transparent_hugepage/defrag");

  f.open(path);

  f << defrag;

  std::cout << log_tag + "changed transparent hugepage defrag method to: " << defrag << std::endl;

  f.close();

  if (shmem_enabled == "") {
    return;
  }

  path = fs::path("/sys/kernel/mm/transparent_hugepage/shmem_enabled");

  f.open(path);

  f << shmem_enabled;

  std::cout << log_tag + "changed transparent hugepage shmem_enabled method to: " << shmem_enabled << std::endl;

  f.close();
}

void Tweaks::set_cpu_dma_latency(const int& latency_us = 0) {
  cpu_dma_ofstream.open("/dev/cpu_dma_latency");

  cpu_dma_ofstream << latency_us;

  std::cout << log_tag + "/dev/cpu_dma_latency latency: " << latency_us << " us" << std::endl;
}

void Tweaks::change_vm_parameter(const std::string& name, const int& value) {
  if (value == -1) {
    return;
  }

  auto path = fs::path("/proc/sys/vm/" + name);

  std::ofstream f;

  f.open(path);

  f << value;

  f.close();

  std::cout << log_tag + "changed vm parameter " + name + " to: " << value << std::endl;
}
