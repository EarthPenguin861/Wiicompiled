#include "guest_flat_memory.h"

#include <TargetConditionals.h>
#include <mach/mach.h>
#if !TARGET_OS_TV
#include <mach/mach_vm.h>
#endif
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace GuestFlat {
bool g_requiresCheckedAccess = false;
namespace {

struct Store {
    Backing kind = Backing::Owned;
    uint32_t owned = 0;
    uint64_t size = 0;
    uint8_t* host = nullptr;
};

struct Mapping {
    uint32_t base = 0;
    uint64_t size = 0;
    uint8_t* host = nullptr;
};

// Apple TV exposes the legacy vm_* API but intentionally rejects mach_vm.h.
 // On 64-bit Darwin, vm_address_t/vm_size_t are pointer-width, so those APIs
 // can represent the guest reservation just as the mach_vm_* variants do.
#if TARGET_OS_TV
using GuestVmAddress = vm_address_t;
using GuestVmSize = vm_size_t;

kern_return_t ReserveGuestAddress(task_t task, GuestVmAddress* address, GuestVmSize size, int flags) {
    return vm_allocate(task, address, size, flags);
}
kern_return_t ReleaseGuestAddress(task_t task, GuestVmAddress address, GuestVmSize size) {
    return vm_deallocate(task, address, size);
}
kern_return_t RemapGuestAlias(task_t targetTask, GuestVmAddress* targetAddress, GuestVmSize size,
                              GuestVmAddress mask, int flags, task_t sourceTask,
                              GuestVmAddress sourceAddress, boolean_t copy,
                              vm_prot_t* currentProtection, vm_prot_t* maximumProtection,
                              vm_inherit_t inheritance) {
    return vm_remap(targetTask, targetAddress, size, mask, flags, sourceTask, sourceAddress, copy,
                    currentProtection, maximumProtection, inheritance);
}
#else
using GuestVmAddress = mach_vm_address_t;
using GuestVmSize = mach_vm_size_t;

kern_return_t ReserveGuestAddress(task_t task, GuestVmAddress* address, GuestVmSize size, int flags) {
    return mach_vm_allocate(task, address, size, flags);
}
kern_return_t ReleaseGuestAddress(task_t task, GuestVmAddress address, GuestVmSize size) {
    return mach_vm_deallocate(task, address, size);
}
kern_return_t RemapGuestAlias(task_t targetTask, GuestVmAddress* targetAddress, GuestVmSize size,
                              GuestVmAddress mask, int flags, task_t sourceTask,
                              GuestVmAddress sourceAddress, boolean_t copy,
                              vm_prot_t* currentProtection, vm_prot_t* maximumProtection,
                              vm_inherit_t inheritance) {
    return mach_vm_remap(targetTask, targetAddress, size, mask, flags, sourceTask, sourceAddress, copy,
                         currentProtection, maximumProtection, inheritance);
}
#endif

std::mutex g_mutex;
std::vector<Store> g_stores;
std::vector<Mapping> g_mappings;
std::vector<RegionRequest> g_layout;
uint8_t* g_base = nullptr;
bool g_active = false;

inline uint64_t RoundUp(uint64_t value, uint64_t align) {
    return (value + align - 1) & ~(align - 1);
}

uint64_t Offset(const RegionRequest& r) {
    if (r.backing == Backing::Mem1) return r.base & 0x1fffffffu;
    if (r.backing == Backing::Mem2) return (r.base & 0x1fffffffu) - 0x10000000u;
    return 0;
}

bool Same(const std::vector<RegionRequest>& a, const std::vector<RegionRequest>& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
        [](const auto& x, const auto& y) { return x.base == y.base && x.size == y.size && x.backing == y.backing; });
}

} // namespace

bool IsActive() { return g_active; }

void Initialize(const std::vector<RegionRequest>& regions) {
    std::lock_guard lock(g_mutex);
    const uint64_t pageSize = static_cast<uint64_t>(getpagesize());
    g_requiresCheckedAccess = pageSize > kGuestPageSize;
    if (g_active) {
        if (!Same(g_layout, regions))
            throw std::runtime_error("flat guest layout cannot be remapped");
        return;
    }

    for (size_t i = 0; i < regions.size(); ++i) {
        const auto& r = regions[i];
        if (r.size == 0) continue;
        if (r.size > kGuestSpaceSize - r.base ||
            r.base % pageSize != 0 || Offset(r) % pageSize != 0) {
            throw std::runtime_error("invalid flat guest memory region");
        }
        const uint64_t end = static_cast<uint64_t>(r.base) + RoundUp(r.size, pageSize);
        for (size_t j = 0; j < i; ++j) {
            const auto& prior = regions[j];
            if (prior.size == 0) continue;
            const uint64_t priorEnd = static_cast<uint64_t>(prior.base) + RoundUp(prior.size, pageSize);
            if (r.base < priorEnd && prior.base < end) {
                throw std::runtime_error("overlapping flat guest memory regions");
            }
        }
    }

    GuestVmAddress address = static_cast<GuestVmAddress>(kFixedFlatGuestBase);
    if (ReserveGuestAddress(mach_task_self(), &address, static_cast<GuestVmSize>(kGuestSpaceSize),
                            VM_FLAGS_FIXED) != KERN_SUCCESS ||
        address != static_cast<GuestVmAddress>(kFixedFlatGuestBase)) {
        throw std::runtime_error("unable to reserve fixed 4 GiB Apple guest address space");
    }
    g_base = reinterpret_cast<uint8_t*>(address);

    std::vector<Store> stores;
    try {
        for (const auto& r : regions) {
            if (!r.size) continue;
            const uint32_t owned = r.backing == Backing::Owned ? r.base : 0;
            auto it = std::find_if(stores.begin(), stores.end(), [&](const Store& s) {
                return s.kind == r.backing && s.owned == owned;
            });
            const uint64_t need = RoundUp(Offset(r) + r.size, pageSize);
            if (it == stores.end()) {
                stores.push_back({r.backing, owned, need, nullptr});
            } else {
                it->size = std::max(it->size, need);
            }
        }

        for (auto& s : stores) {
            void* host = mmap(nullptr, static_cast<size_t>(s.size), PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANON, -1, 0);
            if (host == MAP_FAILED) {
                throw std::runtime_error("unable to allocate anonymous macOS guest backing store");
            }
            s.host = static_cast<uint8_t*>(host);
        }

        for (const auto& r : regions) {
            if (!r.size) continue;
            const uint32_t owned = r.backing == Backing::Owned ? r.base : 0;
            const auto& s = *std::find_if(stores.begin(), stores.end(), [&](const Store& x) {
                return x.kind == r.backing && x.owned == owned;
            });
            GuestVmAddress target = reinterpret_cast<GuestVmAddress>(g_base + r.base);
            vm_prot_t cur = VM_PROT_NONE, max = VM_PROT_NONE;
            const kern_return_t kr = RemapGuestAlias(
                mach_task_self(),
                &target,
                static_cast<GuestVmSize>(RoundUp(r.size, pageSize)),
                0,
                VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
                mach_task_self(),
                reinterpret_cast<GuestVmAddress>(s.host + Offset(r)),
                FALSE,
                &cur,
                &max,
                VM_INHERIT_NONE
            );
            if (kr != KERN_SUCCESS) {
                throw std::runtime_error(std::string("Apple guest alias remap failed: ") + mach_error_string(kr));
            }
            g_mappings.push_back({r.base, r.size, s.host + Offset(r)});
        }

        g_layout = regions;
        g_stores = std::move(stores);
        g_active = true;
    } catch (...) {
        for (auto& s : stores) {
            if (s.host != nullptr) {
                munmap(s.host, static_cast<size_t>(s.size));
            }
        }
        ReleaseGuestAddress(mach_task_self(), address, static_cast<GuestVmSize>(kGuestSpaceSize));
        g_mappings.clear();
        g_stores.clear();
        g_layout.clear();
        g_base = nullptr;
        throw;
    }
}

uint8_t* HostPointer(uint32_t a) {
    for (const auto& m : g_mappings) {
        if (a >= m.base && uint64_t(a - m.base) < m.size) {
            return m.host + (a - m.base);
        }
    }
    return nullptr;
}

void ProtectDeferredRange(uint32_t, size_t) {}
void UnprotectDeferredRange(uint32_t, size_t) {}
void RegisterExecutableRange(uint32_t, uint32_t) {}
FaultCounters Counters() { return {}; }
void LogFaultSummary() noexcept {}
bool HandleAccessViolation(void*, bool) noexcept { return false; }

} // namespace GuestFlat
