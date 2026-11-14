// streambench — measures whether a streamed, quantized DiT can fit the
// ImageEdit low-VRAM budget on THIS machine.
//
// It does not load a real model. It reproduces the memory traffic and the
// arithmetic of one: bit-packed weight blocks live in system RAM, are streamed
// through a two-slot VRAM ring on a transfer queue, and are dequantized inside
// the compute shader that multiplies them. From the measured bus throughput,
// compute throughput and overlap it projects the time for the denoising phase
// and asserts it against the budget.
//
// The projection is a LOWER bound for a real model: it covers the linear
// layers only (no attention softmax, norms, or VAE) and assumes RAM holds the
// whole quantized model.

#define VK_NO_PROTOTYPES
#include "volk.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <mutex>
#include <string>
#include <vector>

#include "shaders/qmatmul.spv.h"

namespace {

constexpr uint32_t TILE = 8; // must match the shader

struct Args {
    int device = -1;
    uint32_t dim = 3072;        // hidden size of the Qwen-Image DiT
    uint32_t tokens = 3200;     // 640x640 target + one 640x640 reference
    uint32_t bits = 4;          // 2 or 4 bits per weight
    uint32_t layers = 60;       // transformer blocks in the full model
    uint32_t steps = 4;         // distilled steps
    double layerMB = 0;         // 0 = derive from --params-b and --bits
    double paramsB = 20.0;      // model size in billions of parameters
    uint32_t sample = 3;        // layers actually executed per phase
    uint32_t chunk = 256;       // tokens per dispatch (keeps each submit short
                                // enough for the Windows 2 s GPU watchdog)
    double vramBudgetMB = 1050; // ring + activations + scratch (1.8 GB - 750 MB headroom)
    double budgetS = 85;        // phase-2 target, upper end
    bool noAssert = false;
    std::string json;
};

struct PC {
    uint32_t dIn, dOut, tBase, tCount, wBase, bits, pad0, pad1;
    float lut[16];
};

double now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

[[noreturn]] void die(const char* what, VkResult r = VK_SUCCESS) {
    std::fprintf(stderr, "error: %s (VkResult %d)\n", what, (int)r);
    std::exit(1);
}
#define CHECK(call) do { VkResult r_ = (call); if (r_ != VK_SUCCESS) die(#call, r_); } while (0)

struct Buffer {
    VkBuffer buf = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    void* ptr = nullptr;
};

struct Ctx {
    VkInstance inst = VK_NULL_HANDLE;
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    VkDevice dev = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties memProps{};
    uint32_t computeFam = 0, transferFam = 0;
    VkQueue computeQ = VK_NULL_HANDLE, transferQ = VK_NULL_HANDLE;
    bool sharedQueue = false;     // transfer and compute are the same VkQueue
    bool dedicatedTransfer = false;
    bool hasBudgetExt = false;
    std::mutex qMutex;            // only taken when sharedQueue
    VkDeviceSize deviceLocalBytes = 0;
};

uint32_t findMem(const Ctx& c, uint32_t typeBits, VkMemoryPropertyFlags want, VkMemoryPropertyFlags avoid = 0) {
    for (int pass = 0; pass < 2; ++pass)
        for (uint32_t i = 0; i < c.memProps.memoryTypeCount; ++i) {
            VkMemoryPropertyFlags f = c.memProps.memoryTypes[i].propertyFlags;
            if (!(typeBits & (1u << i)) || (f & want) != want) continue;
            if (pass == 0 && (f & avoid)) continue;
            return i;
        }
    die("no suitable memory type");
}

Buffer makeBuffer(Ctx& c, VkDeviceSize size, VkBufferUsageFlags usage, bool hostVisible) {
    Buffer b;
    b.size = size;
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    uint32_t fams[2] = {c.computeFam, c.transferFam};
    if (c.computeFam != c.transferFam) {
        bi.sharingMode = VK_SHARING_MODE_CONCURRENT;
        bi.queueFamilyIndexCount = 2;
        bi.pQueueFamilyIndices = fams;
    }
    CHECK(vkCreateBuffer(c.dev, &bi, nullptr, &b.buf));
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(c.dev, b.buf, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = hostVisible
        ? findMem(c, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        : findMem(c, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    VkResult r = vkAllocateMemory(c.dev, &ai, nullptr, &b.mem);
    if (r != VK_SUCCESS) {
        std::fprintf(stderr, "error: could not allocate %.0f MB of %s memory (VkResult %d)\n",
                     size / 1048576.0, hostVisible ? "host-visible" : "device-local", (int)r);
        std::exit(1);
    }
    CHECK(vkBindBufferMemory(c.dev, b.buf, b.mem, 0));
    if (hostVisible) CHECK(vkMapMemory(c.dev, b.mem, 0, VK_WHOLE_SIZE, 0, &b.ptr));
    else c.deviceLocalBytes += req.size;
    return b;
}

void submitAndWait(Ctx& c, VkQueue q, VkCommandBuffer cb, VkFence fence) {
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    CHECK(vkResetFences(c.dev, 1, &fence));
    if (c.sharedQueue) {
        std::lock_guard<std::mutex> g(c.qMutex);
        CHECK(vkQueueSubmit(q, 1, &si, fence));
    } else {
        CHECK(vkQueueSubmit(q, 1, &si, fence));
    }
    CHECK(vkWaitForFences(c.dev, 1, &fence, VK_TRUE, UINT64_MAX));
}

void beginOnce(VkCommandBuffer cb) {
    CHECK(vkResetCommandBuffer(cb, 0));
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    CHECK(vkBeginCommandBuffer(cb, &bi));
}

void copyBuffer(Ctx& c, VkQueue q, VkCommandBuffer cb, VkFence f, VkBuffer src, VkBuffer dst, VkDeviceSize n) {
    beginOnce(cb);
    VkBufferCopy r{0, 0, n};
    vkCmdCopyBuffer(cb, src, dst, 1, &r);
    CHECK(vkEndCommandBuffer(cb));
    submitAndWait(c, q, cb, f);
}

uint64_t rngState = 0x9E3779B97F4A7C15ull;
inline uint64_t rng() {
    rngState ^= rngState << 13; rngState ^= rngState >> 7; rngState ^= rngState << 17;
    return rngState;
}

bool parse(int argc, char** argv, Args& a) {
    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];
        auto val = [&]() -> const char* {
            if (i + 1 >= argc) { std::fprintf(stderr, "missing value for %s\n", k.c_str()); std::exit(1); }
            return argv[++i];
        };
        if (k == "--device") a.device = std::atoi(val());
        else if (k == "--dim") a.dim = (uint32_t)std::atoi(val());
        else if (k == "--tokens") a.tokens = (uint32_t)std::atoi(val());
        else if (k == "--bits") a.bits = (uint32_t)std::atoi(val());
        else if (k == "--layers") a.layers = (uint32_t)std::atoi(val());
        else if (k == "--steps") a.steps = (uint32_t)std::atoi(val());
        else if (k == "--layer-mb") a.layerMB = std::atof(val());
        else if (k == "--params-b") a.paramsB = std::atof(val());
        else if (k == "--sample-layers") a.sample = (uint32_t)std::atoi(val());
        else if (k == "--chunk-tokens") a.chunk = (uint32_t)std::atoi(val());
        else if (k == "--vram-budget-mb") a.vramBudgetMB = std::atof(val());
        else if (k == "--budget-s") a.budgetS = std::atof(val());
        else if (k == "--no-assert") a.noAssert = true;
        else if (k == "--json") a.json = val();
        else {
            std::printf(
                "streambench [options]\n"
                "  --device N          Vulkan device index (default: first discrete GPU)\n"
                "  --dim N             hidden size (default 3072)\n"
                "  --tokens N          tokens per DiT pass (default 3200 = 640x640 + 1 reference)\n"
                "  --bits 2|4          bits per weight (default 4)\n"
                "  --params-b X        model size, billions of parameters (default 20)\n"
                "  --layers N          transformer blocks (default 60)\n"
                "  --steps N           denoising steps (default 4)\n"
                "  --layer-mb X        override the per-layer block size\n"
                "  --sample-layers N   layers executed per phase (default 3)\n"
                "  --chunk-tokens N    tokens per dispatch (default 256)\n"
                "  --vram-budget-mb X  ring + activations budget (default 1050)\n"
                "  --budget-s X        phase-2 time budget in seconds (default 85)\n"
                "  --no-assert         always exit 0\n"
                "  --json FILE         also write results as JSON\n");
            return false;
        }
    }
    if (a.bits != 2 && a.bits != 4) { std::fprintf(stderr, "--bits must be 2 or 4\n"); std::exit(1); }
    if (a.sample < 2) a.sample = 2;
    a.dim = std::max<uint32_t>(32, a.dim / 32 * 32);
    a.tokens = std::max<uint32_t>(TILE, (a.tokens + TILE - 1) / TILE * TILE);
    a.chunk = std::max<uint32_t>(TILE, a.chunk / TILE * TILE);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    Args a;
    if (!parse(argc, argv, a)) return 0;

    // ---- instance + device -------------------------------------------------
    if (volkInitialize() != VK_SUCCESS) {
        std::fprintf(stderr, "error: no Vulkan loader found. Install or update your GPU driver.\n");
        return 1;
    }
    Ctx c;
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "streambench";
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    CHECK(vkCreateInstance(&ici, nullptr, &c.inst));
    volkLoadInstance(c.inst);

    uint32_t nDev = 0;
    CHECK(vkEnumeratePhysicalDevices(c.inst, &nDev, nullptr));
    if (!nDev) die("no Vulkan devices");
    std::vector<VkPhysicalDevice> devs(nDev);
    CHECK(vkEnumeratePhysicalDevices(c.inst, &nDev, devs.data()));
    int pick = a.device, bestRank = -1;
    std::printf("Vulkan devices:\n");
    for (uint32_t i = 0; i < nDev; ++i) {
        VkPhysicalDeviceProperties p;
        vkGetPhysicalDeviceProperties(devs[i], &p);
        const char* types[] = {"other", "integrated", "discrete", "virtual", "cpu"};
        std::printf("  [%u] %s (%s)\n", i, p.deviceName, types[std::min<uint32_t>(p.deviceType, 4)]);
        int rank = p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 3
                 : p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2
                 : p.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU ? 0 : 1;
        if (a.device < 0 && rank > bestRank) { bestRank = rank; pick = (int)i; }
    }
    if (pick < 0 || pick >= (int)nDev) die("bad --device index");
    c.phys = devs[pick];
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(c.phys, &props);
    vkGetPhysicalDeviceMemoryProperties(c.phys, &c.memProps);
    bool isDiscrete = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    bool isCpu = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;

    VkDeviceSize dedicatedVram = 0;
    for (uint32_t i = 0; i < c.memProps.memoryHeapCount; ++i)
        if (c.memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            dedicatedVram = std::max(dedicatedVram, c.memProps.memoryHeaps[i].size);
    std::printf("Using [%d] %s, device-local heap %.0f MB\n", pick, props.deviceName, dedicatedVram / 1048576.0);

    uint32_t nFam = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(c.phys, &nFam, nullptr);
    std::vector<VkQueueFamilyProperties> fams(nFam);
    vkGetPhysicalDeviceQueueFamilyProperties(c.phys, &nFam, fams.data());
    int cf = -1, tf = -1;
    for (uint32_t i = 0; i < nFam; ++i)
        if ((fams[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && cf < 0) cf = (int)i;
    for (uint32_t i = 0; i < nFam; ++i)
        if ((fams[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
            !(fams[i].queueFlags & (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT)) && tf < 0) tf = (int)i;
    if (cf < 0) die("no compute queue");
    c.computeFam = (uint32_t)cf;
    uint32_t transferIdx = 0;
    if (tf >= 0) { c.transferFam = (uint32_t)tf; c.dedicatedTransfer = true; }
    else {
        c.transferFam = c.computeFam;
        if (fams[cf].queueCount >= 2) transferIdx = 1; else c.sharedQueue = true;
    }

    uint32_t nExt = 0;
    vkEnumerateDeviceExtensionProperties(c.phys, nullptr, &nExt, nullptr);
    std::vector<VkExtensionProperties> exts(nExt);
    vkEnumerateDeviceExtensionProperties(c.phys, nullptr, &nExt, exts.data());
    for (auto& e : exts)
        if (!std::strcmp(e.extensionName, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)) c.hasBudgetExt = true;
    if (props.apiVersion < VK_API_VERSION_1_1) c.hasBudgetExt = false;
    const char* budgetExt = VK_EXT_MEMORY_BUDGET_EXTENSION_NAME;

    float prio[2] = {1.0f, 1.0f};
    VkDeviceQueueCreateInfo qci[2]{};
    uint32_t nQci = 1;
    qci[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci[0].queueFamilyIndex = c.computeFam;
    qci[0].queueCount = (c.transferFam == c.computeFam && transferIdx == 1) ? 2 : 1;
    qci[0].pQueuePriorities = prio;
    if (c.transferFam != c.computeFam) {
        qci[1] = qci[0];
        qci[1].queueFamilyIndex = c.transferFam;
        qci[1].queueCount = 1;
        nQci = 2;
    }
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = nQci;
    dci.pQueueCreateInfos = qci;
    if (c.hasBudgetExt) { dci.enabledExtensionCount = 1; dci.ppEnabledExtensionNames = &budgetExt; }
    CHECK(vkCreateDevice(c.phys, &dci, nullptr, &c.dev));
    volkLoadDevice(c.dev);
    vkGetDeviceQueue(c.dev, c.computeFam, 0, &c.computeQ);
    vkGetDeviceQueue(c.dev, c.transferFam, transferIdx, &c.transferQ);
    std::printf("Transfer queue: %s\n", c.dedicatedTransfer ? "dedicated family"
                : c.sharedQueue ? "SHARED with compute (driver exposes one queue; overlap will be limited)"
                : "second queue of the compute family");

    // ---- sizes -------------------------------------------------------------
    const uint32_t D = a.dim, T = a.tokens;
    const uint32_t cpw = 32 / a.bits;
    const uint64_t matWords = (uint64_t)D * (D / cpw);
    const uint64_t matBytes = matWords * 4;
    double layerMB = a.layerMB > 0 ? a.layerMB : a.paramsB * 1e9 * a.bits / 8.0 / a.layers / 1048576.0;
    uint32_t mats = (uint32_t)std::max<uint64_t>(1, (uint64_t)(layerMB * 1048576.0) / matBytes);
    const uint64_t layerBytes = mats * matBytes;
    const uint64_t actBytes = (uint64_t)T * D * 4;
    const double flopsPerLayer = 2.0 * D * D * (double)T * mats;
    std::printf("Model: %.1fB params at %u bpw -> %.2f GB in RAM, %u layers x %.0f MB (%u matrices of %ux%u)\n",
                a.paramsB, a.bits, layerBytes * (double)a.layers / 1073741824.0, a.layers,
                layerBytes / 1048576.0, mats, D, D);
    std::printf("Pass:  %u tokens, %u steps, %.2f TFLOP per layer, %.0f TFLOP per image (linear layers only)\n",
                T, a.steps, flopsPerLayer / 1e12, flopsPerLayer * a.layers * a.steps / 1e12);

    // ---- resources ---------------------------------------------------------
    VkBufferUsageFlags stor = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    Buffer slot[2], staging[2];
    for (int i = 0; i < 2; ++i) {
        slot[i] = makeBuffer(c, layerBytes, stor | VK_BUFFER_USAGE_TRANSFER_DST_BIT, false);
        staging[i] = makeBuffer(c, std::max(layerBytes, actBytes), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
    }
    Buffer X = makeBuffer(c, actBytes, stor | VK_BUFFER_USAGE_TRANSFER_DST_BIT, false);
    Buffer Y = makeBuffer(c, actBytes, stor | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, false);
    const double usedMB = c.deviceLocalBytes / 1048576.0;
    std::printf("VRAM allocated by the ring: %.0f MB (2 slots x %.0f MB + %.0f MB activations)\n",
                usedMB, layerBytes / 1048576.0, 2 * actBytes / 1048576.0);

    // host-side "model in RAM": one distinct block per sampled layer
    std::vector<std::vector<uint32_t>> blocks(a.sample);
    for (auto& b : blocks) {
        b.resize(layerBytes / 4);
        for (size_t i = 0; i + 1 < b.size(); i += 2) { uint64_t r = rng(); b[i] = (uint32_t)r; b[i + 1] = (uint32_t)(r >> 32); }
    }
    PC pc{};
    pc.dIn = D; pc.dOut = D; pc.bits = a.bits;
    const uint32_t nCodes = 1u << a.bits;
    for (uint32_t i = 0; i < nCodes; ++i)
        pc.lut[i] = (-1.0f + 2.0f * i / (nCodes - 1)) / std::sqrt((float)D);

    VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool cPool, tPool;
    cpi.queueFamilyIndex = c.computeFam;  CHECK(vkCreateCommandPool(c.dev, &cpi, nullptr, &cPool));
    cpi.queueFamilyIndex = c.transferFam; CHECK(vkCreateCommandPool(c.dev, &cpi, nullptr, &tPool));
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    VkCommandBuffer cCb, tCb;
    cai.commandPool = cPool; CHECK(vkAllocateCommandBuffers(c.dev, &cai, &cCb));
    cai.commandPool = tPool; CHECK(vkAllocateCommandBuffers(c.dev, &cai, &tCb));
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence cFence, tFence;
    CHECK(vkCreateFence(c.dev, &fci, nullptr, &cFence));
    CHECK(vkCreateFence(c.dev, &fci, nullptr, &tFence));

    // pipeline
    VkDescriptorSetLayoutBinding lb[3]{};
    for (uint32_t i = 0; i < 3; ++i) {
        lb[i].binding = i; lb[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        lb[i].descriptorCount = 1; lb[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo dli{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dli.bindingCount = 3; dli.pBindings = lb;
    VkDescriptorSetLayout dsl;
    CHECK(vkCreateDescriptorSetLayout(c.dev, &dli, nullptr, &dsl));
    VkPushConstantRange pcr{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC)};
    VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pli.setLayoutCount = 1; pli.pSetLayouts = &dsl;
    pli.pushConstantRangeCount = 1; pli.pPushConstantRanges = &pcr;
    VkPipelineLayout layout;
    CHECK(vkCreatePipelineLayout(c.dev, &pli, nullptr, &layout));
    VkShaderModuleCreateInfo smi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smi.codeSize = sizeof(qmatmul_spv); smi.pCode = qmatmul_spv;
    VkShaderModule sm;
    CHECK(vkCreateShaderModule(c.dev, &smi, nullptr, &sm));
    VkComputePipelineCreateInfo pci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pci.stage.module = sm; pci.stage.pName = "main";
    pci.layout = layout;
    VkPipeline pipe;
    CHECK(vkCreateComputePipelines(c.dev, VK_NULL_HANDLE, 1, &pci, nullptr, &pipe));

    VkDescriptorPoolSize dps{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6};
    VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpi.maxSets = 2; dpi.poolSizeCount = 1; dpi.pPoolSizes = &dps;
    VkDescriptorPool dpool;
    CHECK(vkCreateDescriptorPool(c.dev, &dpi, nullptr, &dpool));
    VkDescriptorSet dset[2];
    for (int s = 0; s < 2; ++s) {
        VkDescriptorSetAllocateInfo dai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dai.descriptorPool = dpool; dai.descriptorSetCount = 1; dai.pSetLayouts = &dsl;
        CHECK(vkAllocateDescriptorSets(c.dev, &dai, &dset[s]));
        VkDescriptorBufferInfo bi[3] = {{slot[s].buf, 0, VK_WHOLE_SIZE}, {X.buf, 0, VK_WHOLE_SIZE}, {Y.buf, 0, VK_WHOLE_SIZE}};
        VkWriteDescriptorSet w[3]{};
        for (uint32_t i = 0; i < 3; ++i) {
            w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w[i].dstSet = dset[s]; w[i].dstBinding = i; w[i].descriptorCount = 1;
            w[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; w[i].pBufferInfo = &bi[i];
        }
        vkUpdateDescriptorSets(c.dev, 3, w, 0, nullptr);
    }

    // activations
    std::vector<float> xHost((size_t)T * D);
    for (auto& v : xHost) v = (float)((rng() >> 40) / 8388608.0 - 1.0);
    std::memcpy(staging[0].ptr, xHost.data(), actBytes);
    copyBuffer(c, c.transferQ, tCb, tFence, staging[0].buf, X.buf, actBytes);

    // ---- primitives --------------------------------------------------------
    double memcpyS = 0, busS = 0; // accumulated by upload()
    auto upload = [&](uint32_t layer, int s) {
        double t0 = now();
        std::memcpy(staging[s].ptr, blocks[layer % a.sample].data(), layerBytes);
        double t1 = now();
        copyBuffer(c, c.transferQ, tCb, tFence, staging[s].buf, slot[s].buf, layerBytes);
        double t2 = now();
        memcpyS += t1 - t0; busS += t2 - t1;
    };
    auto dispatch = [&](int s, uint32_t mat, uint32_t tBase, uint32_t tCount) {
        beginOnce(cCb);
        vkCmdBindPipeline(cCb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
        vkCmdBindDescriptorSets(cCb, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &dset[s], 0, nullptr);
        PC p = pc;
        p.wBase = (uint32_t)(mat * matWords); p.tBase = tBase; p.tCount = tCount;
        vkCmdPushConstants(cCb, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &p);
        vkCmdDispatch(cCb, (D + 63) / 64, tCount / TILE, 1);
        CHECK(vkEndCommandBuffer(cCb));
        submitAndWait(c, c.computeQ, cCb, cFence);
    };
    auto computeLayer = [&](int s) {
        for (uint32_t m = 0; m < mats; ++m)
            for (uint32_t t = 0; t < T; t += a.chunk)
                dispatch(s, m, t, std::min(a.chunk, T - t));
    };

    // ---- correctness check against the CPU ---------------------------------
    upload(0, 0);
    dispatch(0, 0, 0, TILE);
    Buffer rb = makeBuffer(c, (VkDeviceSize)TILE * D * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
    copyBuffer(c, c.transferQ, tCb, tFence, Y.buf, rb.buf, (VkDeviceSize)TILE * D * 4);
    const float* yGpu = (const float*)rb.ptr;
    double maxErr = 0, maxRef = 0;
    const uint32_t mask = nCodes - 1;
    for (uint32_t o = 0; o < D; ++o) {
        const uint32_t* row = blocks[0].data() + (size_t)o * (D / cpw);
        for (uint32_t t = 0; t < TILE; ++t) {
            double acc = 0;
            const float* xr = xHost.data() + (size_t)t * D;
            for (uint32_t i = 0; i < D; ++i)
                acc += (double)pc.lut[(row[i / cpw] >> ((i % cpw) * a.bits)) & mask] * xr[i];
            maxErr = std::max(maxErr, std::fabs(acc - yGpu[(size_t)t * D + o]));
            maxRef = std::max(maxRef, std::fabs(acc));
        }
    }
    bool correct = maxErr <= 1e-3 * std::max(1.0, maxRef);
    std::printf("Dequant+matmul vs CPU reference: max abs error %.2e (max value %.2f) -> %s\n",
                maxErr, maxRef, correct ? "OK" : "MISMATCH");

    // ---- phase A: transfer only -------------------------------------------
    memcpyS = busS = 0;
    for (uint32_t n = 0; n < a.sample; ++n) upload(n, n & 1);
    const double gb = layerBytes * (double)a.sample / 1e9;
    const double busGBs = gb / busS, hostGBs = gb / memcpyS;
    const double xferPerLayer = (memcpyS + busS) / a.sample;
    std::printf("A. transfer only : %.2f GB/s RAM->VRAM, %.2f GB/s RAM->staging copy, %.3f s/layer\n",
                busGBs, hostGBs, xferPerLayer);

    // ---- phase B: compute only --------------------------------------------
    double t0 = now();
    for (uint32_t n = 0; n < a.sample; ++n) {
        computeLayer(0);
        std::printf("\r   compute layer %u/%u", n + 1, a.sample); std::fflush(stdout);
    }
    const double compPerLayer = (now() - t0) / a.sample;
    const double gflops = flopsPerLayer / compPerLayer / 1e9;
    std::printf("\rB. compute only  : %.1f GFLOPS effective, %.3f s/layer\n", gflops, compPerLayer);

    // ---- phase C: double-buffered ring (copy N+1 while computing N) --------
    upload(0, 0);
    t0 = now();
    for (uint32_t n = 0; n < a.sample; ++n) {
        auto next = std::async(std::launch::async, [&, n] { upload(n + 1, (n + 1) & 1); });
        computeLayer(n & 1);
        next.get();
        std::printf("\r   ring layer %u/%u", n + 1, a.sample); std::fflush(stdout);
    }
    const double ringPerLayer = (now() - t0) / a.sample;
    const double serialPerLayer = xferPerLayer + compPerLayer;
    const double hidden = serialPerLayer > ringPerLayer
        ? std::min(1.0, (serialPerLayer - ringPerLayer) / xferPerLayer) : 0.0;
    std::printf("\rC. async ring    : %.3f s/layer (serial would be %.3f; %.0f%% of transfer time hidden)\n",
                ringPerLayer, serialPerLayer, hidden * 100);

    // ---- VRAM --------------------------------------------------------------
    double heapUsedMB = -1;
    if (c.hasBudgetExt) {
        VkPhysicalDeviceMemoryBudgetPropertiesEXT bud{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
        VkPhysicalDeviceMemoryProperties2 mp2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
        mp2.pNext = &bud;
        vkGetPhysicalDeviceMemoryProperties2(c.phys, &mp2);
        VkDeviceSize used = 0;
        for (uint32_t i = 0; i < mp2.memoryProperties.memoryHeapCount; ++i)
            if (mp2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) used += bud.heapUsage[i];
        heapUsedMB = used / 1048576.0;
    }

    // ---- projection + assertions ------------------------------------------
    const double phase2 = ringPerLayer * a.layers * a.steps;
    const double busOnly = xferPerLayer * a.layers * a.steps;
    const double compOnly = compPerLayer * a.layers * a.steps;
    std::printf("\nProjected denoising phase (%u layers x %u steps), linear layers only:\n", a.layers, a.steps);
    std::printf("  streamed ring : %8.1f s   (budget %.0f s)\n", phase2, a.budgetS);
    std::printf("  of which bus  : %8.1f s if not overlapped\n", busOnly);
    std::printf("  of which math : %8.1f s\n", compOnly);
    std::printf("  bottleneck    : %s\n", compOnly > 2 * busOnly ? "COMPUTE (GPU arithmetic, not PCIe)"
                : busOnly > 2 * compOnly ? "TRANSFER (bus / host copy)" : "mixed");
    if (heapUsedMB >= 0) std::printf("  VRAM in use (driver-reported, this process): %.0f MB\n", heapUsedMB);
    if (!isDiscrete)
        std::printf("  note: this is %s; 'VRAM' is system RAM here and the bus figure is not PCIe.\n",
                    isCpu ? "a CPU software renderer" : "not a discrete GPU");

    bool vramOk = usedMB <= a.vramBudgetMB;
    bool timeOk = phase2 <= a.budgetS;
    std::printf("\nASSERT vram  %.0f MB <= %.0f MB : %s\n", usedMB, a.vramBudgetMB, vramOk ? "PASS" : "FAIL");
    std::printf("ASSERT time  %.1f s <= %.0f s : %s\n", phase2, a.budgetS, timeOk ? "PASS" : "FAIL");
    std::printf("ASSERT math  GPU result matches CPU : %s\n", correct ? "PASS" : "FAIL");

    if (!a.json.empty()) {
        if (FILE* f = std::fopen(a.json.c_str(), "w")) {
            std::fprintf(f,
                "{\n  \"device\": \"%s\",\n  \"discrete\": %s,\n  \"device_local_heap_mb\": %.0f,\n"
                "  \"dedicated_transfer_queue\": %s,\n  \"shared_queue\": %s,\n"
                "  \"bits\": %u,\n  \"params_b\": %.2f,\n  \"layers\": %u,\n  \"steps\": %u,\n  \"tokens\": %u,\n  \"dim\": %u,\n"
                "  \"layer_mb\": %.1f,\n  \"ring_vram_mb\": %.1f,\n  \"heap_used_mb\": %.1f,\n"
                "  \"bus_gb_s\": %.3f,\n  \"host_copy_gb_s\": %.3f,\n  \"gflops\": %.2f,\n"
                "  \"transfer_s_per_layer\": %.4f,\n  \"compute_s_per_layer\": %.4f,\n  \"ring_s_per_layer\": %.4f,\n"
                "  \"transfer_hidden\": %.3f,\n  \"projected_phase2_s\": %.1f,\n  \"budget_s\": %.1f,\n"
                "  \"pass_vram\": %s,\n  \"pass_time\": %s,\n  \"pass_math\": %s\n}\n",
                props.deviceName, isDiscrete ? "true" : "false", dedicatedVram / 1048576.0,
                c.dedicatedTransfer ? "true" : "false", c.sharedQueue ? "true" : "false",
                a.bits, a.paramsB, a.layers, a.steps, T, D,
                layerBytes / 1048576.0, usedMB, heapUsedMB,
                busGBs, hostGBs, gflops, xferPerLayer, compPerLayer, ringPerLayer,
                hidden, phase2, a.budgetS,
                vramOk ? "true" : "false", timeOk ? "true" : "false", correct ? "true" : "false");
            std::fclose(f);
        }
    }

    vkDeviceWaitIdle(c.dev);
    return (a.noAssert || (vramOk && timeOk && correct)) ? 0 : 2;
}
