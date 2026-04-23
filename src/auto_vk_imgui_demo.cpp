#include <avk/avk.hpp>

#define GLFW_INCLUDE_NONE

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "soloud_demo_framework.h"

#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

namespace
{
constexpr uint32_t k_window_width = 800;
constexpr uint32_t k_window_height = 400;
constexpr uint32_t k_min_image_count = 2;

void check_vk_result(VkResult result)
{
    if (result == VK_SUCCESS)
    {
        return;
    }

    std::fprintf(stderr, "[vulkan] VkResult = %d\n", result);
    if (result < 0)
    {
        std::abort();
    }
}

void check_vk_result(vk::Result result)
{
    check_vk_result(static_cast<VkResult>(result));
}

void glfw_error_callback(int error, const char* description)
{
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

bool is_extension_available(const std::vector<vk::ExtensionProperties>& properties, const char* extension)
{
    for (const auto& property : properties)
    {
        if (std::strcmp(property.extensionName, extension) == 0)
        {
            return true;
        }
    }

    return false;
}

template <typename Loader>
void init_loader_before_instance(Loader& loader)
{
    if constexpr (requires(Loader& current) { current.init(vkGetInstanceProcAddr); })
    {
        loader.init(vkGetInstanceProcAddr);
    }
    else if constexpr (requires(Loader& current) { current.init(); })
    {
        loader.init();
    }
}

template <typename Loader>
void init_loader_after_instance(Loader& loader, vk::Instance instance)
{
    if constexpr (requires(Loader& current, VkInstance raw_instance, PFN_vkGetInstanceProcAddr get_instance_proc_addr) {
                      current.init(raw_instance, get_instance_proc_addr);
                  })
    {
        loader.init(static_cast<VkInstance>(instance), vkGetInstanceProcAddr);
    }
    else if constexpr (requires(Loader& current, const vk::Instance& current_instance) { current.init(current_instance); })
    {
        loader.init(instance);
    }
}

template <typename Loader>
void init_loader_after_device(Loader& loader, vk::Instance instance, vk::Device device)
{
    if constexpr (requires(Loader& current,
                           VkInstance raw_instance,
                           PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                           VkDevice raw_device,
                           PFN_vkGetDeviceProcAddr get_device_proc_addr) {
                      current.init(raw_instance, get_instance_proc_addr, raw_device, get_device_proc_addr);
                  })
    {
        loader.init(static_cast<VkInstance>(instance), vkGetInstanceProcAddr, static_cast<VkDevice>(device), vkGetDeviceProcAddr);
    }
    else if constexpr (requires(Loader& current, const vk::Instance& current_instance, const vk::Device& current_device) {
                           current.init(current_instance, current_device);
                       })
    {
        loader.init(instance, device);
    }
}

ImTextureID to_imgui_texture_id(VkDescriptorSet descriptor_set)
{
    if constexpr (std::is_pointer_v<ImTextureID>)
    {
        return reinterpret_cast<ImTextureID>(descriptor_set);
    }
    else
    {
        return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(descriptor_set));
    }
}

struct DemoTexture
{
    int width = 0;
    int height = 0;
    vk::UniqueBuffer staging_buffer;
    vk::UniqueDeviceMemory staging_memory;
    vk::UniqueImage image;
    vk::UniqueDeviceMemory image_memory;
    vk::UniqueImageView image_view;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
};

class AutoVkDemoRoot final : public avk::root
{
public:
    explicit AutoVkDemoRoot(std::vector<const char*> required_instance_extensions)
    {
        create_instance(std::move(required_instance_extensions));
    }

    ~AutoVkDemoRoot()
    {
        if (m_surface)
        {
            m_instance.get().destroySurfaceKHR(m_surface, nullptr, m_dispatch_loader_ext);
        }
    }

    AutoVkDemoRoot(const AutoVkDemoRoot&) = delete;
    AutoVkDemoRoot& operator=(const AutoVkDemoRoot&) = delete;

    void create_surface(GLFWwindow* window)
    {
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        check_vk_result(glfwCreateWindowSurface(static_cast<VkInstance>(instance()), window, nullptr, &surface));
        m_surface = vk::SurfaceKHR{surface};
    }

    void create_device()
    {
        if (!m_surface)
        {
            throw std::runtime_error("Create the GLFW Vulkan surface before creating the Vulkan device.");
        }

        const auto devices = instance().enumeratePhysicalDevices(m_dispatch_loader_core);
        if (devices.empty())
        {
            throw std::runtime_error("No Vulkan physical device found.");
        }

        std::optional<uint32_t> graphics_queue_family;
        for (const auto& candidate : devices)
        {
            try
            {
                graphics_queue_family = avk::queue::select_queue_family_index(
                    candidate,
                    vk::QueueFlagBits::eGraphics,
                    avk::queue_selection_preference::versatile_queue,
                    m_surface);
                m_physical_device = candidate;
                break;
            }
            catch (const std::exception&)
            {
            }
        }

        if (!graphics_queue_family.has_value())
        {
            throw std::runtime_error("No Vulkan queue family supports both graphics and the GLFW surface.");
        }

        auto prepared_queues = avk::make_vector(avk::queue::prepare(this, *graphics_queue_family, 0, 1.0f));
        auto queue_config = avk::queue::get_queue_config_for_DeviceCreateInfo(std::begin(prepared_queues), std::end(prepared_queues));
        for (size_t index = 0; index < std::get<0>(queue_config).size(); ++index)
        {
            std::get<0>(queue_config)[index].setPQueuePriorities(std::get<1>(queue_config)[index].data());
        }

        std::vector<const char*> device_extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        const auto device_properties = physical_device().enumerateDeviceExtensionProperties(nullptr, m_dispatch_loader_core);

#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
        if (is_extension_available(device_properties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
        {
            device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
        }
#endif

        const vk::DeviceCreateInfo device_info = vk::DeviceCreateInfo{}
            .setQueueCreateInfos(std::get<0>(queue_config))
            .setPEnabledExtensionNames(device_extensions);

        m_device = physical_device().createDeviceUnique(device_info, nullptr, m_dispatch_loader_core);
        prepared_queues[0].assign_handle();
        m_graphics_queue = std::move(prepared_queues[0]);
        init_loader_after_device(m_dispatch_loader_ext, instance(), device());

#if defined(AVK_USE_VMA)
        VmaAllocatorCreateInfo allocator_info = {};
        allocator_info.physicalDevice = static_cast<VkPhysicalDevice>(physical_device());
        allocator_info.device = static_cast<VkDevice>(device());
        allocator_info.instance = static_cast<VkInstance>(instance());
        vmaCreateAllocator(&allocator_info, &m_memory_allocator);
#else
        m_memory_allocator = std::make_tuple(physical_device(), device());
#endif

        create_descriptor_pool();
        create_upload_command_pool();
    }

    void setup_window(ImGui_ImplVulkanH_Window& window_data, int width, int height)
    {
        const auto supports_wsi = physical_device().getSurfaceSupportKHR(graphics_queue().family_index(), m_surface, m_dispatch_loader_ext);
        if (supports_wsi != vk::True)
        {
            throw std::runtime_error("Selected Vulkan queue family does not support the window surface.");
        }

        const VkFormat surface_formats[] = {
            VK_FORMAT_B8G8R8A8_UNORM,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_B8G8R8_UNORM,
            VK_FORMAT_R8G8B8_UNORM,
        };
        const VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_FIFO_KHR};

        window_data.Surface = static_cast<VkSurfaceKHR>(m_surface);
        window_data.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
            raw_physical_device(),
            window_data.Surface,
            surface_formats,
            4,
            VK_COLORSPACE_SRGB_NONLINEAR_KHR);
        window_data.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(raw_physical_device(), window_data.Surface, present_modes, 1);
        resize_window(window_data, width, height);
    }

    void resize_window(ImGui_ImplVulkanH_Window& window_data, int width, int height)
    {
        ImGui_ImplVulkanH_CreateOrResizeWindow(
            raw_instance(),
            raw_physical_device(),
            raw_device(),
            &window_data,
            graphics_queue().family_index(),
            nullptr,
            width,
            height,
            k_min_image_count,
            0);
    }

    void cleanup_window(ImGui_ImplVulkanH_Window& window_data)
    {
        ImGui_ImplVulkanH_DestroyWindow(raw_instance(), raw_device(), &window_data, nullptr);
        window_data.Surface = VK_NULL_HANDLE;
    }

    int load_texture(const char* filename)
    {
        int width = 0;
        int height = 0;
        int component_count = 0;
        stbi_uc* pixels = stbi_load(filename, &width, &height, &component_count, 4);
        if (pixels == nullptr)
        {
            std::cerr << "Failed to load texture: " << filename << '\n';
            return 0;
        }

        DemoTexture texture;
        texture.width = width;
        texture.height = height;
        const vk::DeviceSize image_size = static_cast<vk::DeviceSize>(width) * static_cast<vk::DeviceSize>(height) * 4;

        create_buffer(
            image_size,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            texture.staging_buffer,
            texture.staging_memory);

        void* mapped_memory = device().mapMemory(texture.staging_memory.get(), 0, image_size);
        std::memcpy(mapped_memory, pixels, static_cast<size_t>(image_size));
        device().unmapMemory(texture.staging_memory.get());
        stbi_image_free(pixels);

        const vk::ImageCreateInfo image_info = vk::ImageCreateInfo{}
            .setImageType(vk::ImageType::e2D)
            .setExtent(vk::Extent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1})
            .setMipLevels(1)
            .setArrayLayers(1)
            .setFormat(vk::Format::eR8G8B8A8Unorm)
            .setTiling(vk::ImageTiling::eOptimal)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setUsage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
            .setSharingMode(vk::SharingMode::eExclusive)
            .setSamples(vk::SampleCountFlagBits::e1);
        texture.image = device().createImageUnique(image_info, nullptr, m_dispatch_loader_core);

        const auto memory_requirements = device().getImageMemoryRequirements(texture.image.get());
        const vk::MemoryAllocateInfo allocation_info = vk::MemoryAllocateInfo{}
            .setAllocationSize(memory_requirements.size)
            .setMemoryTypeIndex(find_memory_type(memory_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal));
        texture.image_memory = device().allocateMemoryUnique(allocation_info, nullptr, m_dispatch_loader_core);
        device().bindImageMemory(texture.image.get(), texture.image_memory.get(), 0);

        upload_texture_image(texture, static_cast<uint32_t>(width), static_cast<uint32_t>(height));

        const vk::ImageViewCreateInfo view_info = vk::ImageViewCreateInfo{}
            .setImage(texture.image.get())
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(vk::Format::eR8G8B8A8Unorm)
            .setSubresourceRange(vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
        texture.image_view = device().createImageViewUnique(view_info, nullptr, m_dispatch_loader_core);
        texture.descriptor_set = ImGui_ImplVulkan_AddTexture(static_cast<VkImageView>(texture.image_view.get()), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        m_textures.push_back(std::move(texture));
        return static_cast<int>(m_textures.size());
    }

    const DemoTexture* texture(int id) const
    {
        if (id <= 0 || static_cast<size_t>(id) > m_textures.size())
        {
            return nullptr;
        }
        return &m_textures[static_cast<size_t>(id - 1)];
    }

    vk::Instance instance() const
    {
        return m_instance.get();
    }

    vk::PhysicalDevice& physical_device() override
    {
        return m_physical_device;
    }

    const vk::PhysicalDevice& physical_device() const override
    {
        return m_physical_device;
    }

    vk::Device& device() override
    {
        m_device_handle = m_device.get();
        return m_device_handle;
    }

    const vk::Device& device() const override
    {
        m_device_handle = m_device.get();
        return m_device_handle;
    }

    avk::queue& graphics_queue()
    {
        return m_graphics_queue;
    }

    const avk::queue& graphics_queue() const
    {
        return m_graphics_queue;
    }

    DISPATCH_LOADER_CORE_TYPE& dispatch_loader_core() override
    {
        return m_dispatch_loader_core;
    }

    const DISPATCH_LOADER_CORE_TYPE& dispatch_loader_core() const override
    {
        return m_dispatch_loader_core;
    }

    DISPATCH_LOADER_EXT_TYPE& dispatch_loader_ext() override
    {
        return m_dispatch_loader_ext;
    }

    const DISPATCH_LOADER_EXT_TYPE& dispatch_loader_ext() const override
    {
        return m_dispatch_loader_ext;
    }

    AVK_MEM_ALLOCATOR_TYPE& memory_allocator() override
    {
        return m_memory_allocator;
    }

    const AVK_MEM_ALLOCATOR_TYPE& memory_allocator() const override
    {
        return m_memory_allocator;
    }

    VkInstance raw_instance() const
    {
        return static_cast<VkInstance>(instance());
    }

    VkPhysicalDevice raw_physical_device() const
    {
        return static_cast<VkPhysicalDevice>(physical_device());
    }

    VkDevice raw_device() const
    {
        return static_cast<VkDevice>(device());
    }

    VkQueue raw_graphics_queue() const
    {
        return static_cast<VkQueue>(graphics_queue().handle());
    }

    VkDescriptorPool raw_descriptor_pool() const
    {
        return static_cast<VkDescriptorPool>(m_descriptor_pool.get());
    }

private:
    void create_instance(std::vector<const char*> required_extensions)
    {
        init_loader_before_instance(m_dispatch_loader_core);

        auto instance_extensions = std::move(required_extensions);
        const auto properties = vk::enumerateInstanceExtensionProperties(nullptr, m_dispatch_loader_core);
        vk::InstanceCreateFlags instance_flags{};

        if (is_extension_available(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
        {
            instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        }

#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
        if (is_extension_available(properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
        {
            instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            instance_flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
        }
#endif

        const vk::ApplicationInfo application_info = vk::ApplicationInfo{}
            .setPApplicationName("audio_system_soloud_megademo_auto_vk")
            .setApplicationVersion(VK_MAKE_VERSION(0, 1, 0))
            .setPEngineName("audio_system")
            .setEngineVersion(VK_MAKE_VERSION(0, 1, 0))
            .setApiVersion(VK_API_VERSION_1_1);

        const vk::InstanceCreateInfo instance_info = vk::InstanceCreateInfo{}
            .setFlags(instance_flags)
            .setPApplicationInfo(&application_info)
            .setPEnabledExtensionNames(instance_extensions);

        m_instance = vk::createInstanceUnique(instance_info, nullptr, m_dispatch_loader_core);
        init_loader_after_instance(m_dispatch_loader_core, instance());
        init_loader_after_instance(m_dispatch_loader_ext, instance());
    }

    void create_descriptor_pool()
    {
        const std::array pool_sizes{
            vk::DescriptorPoolSize{vk::DescriptorType::eSampledImage, 2048},
            vk::DescriptorPoolSize{vk::DescriptorType::eSampler, 2048},
            vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 2048},
        };

        const vk::DescriptorPoolCreateInfo pool_info = vk::DescriptorPoolCreateInfo{}
            .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
            .setMaxSets(2048)
            .setPoolSizes(pool_sizes);
        m_descriptor_pool = device().createDescriptorPoolUnique(pool_info, nullptr, m_dispatch_loader_core);
    }

    void create_upload_command_pool()
    {
        const vk::CommandPoolCreateInfo pool_info = vk::CommandPoolCreateInfo{}
            .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
            .setQueueFamilyIndex(graphics_queue().family_index());
        m_upload_command_pool = device().createCommandPoolUnique(pool_info, nullptr, m_dispatch_loader_core);
    }

    uint32_t find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties) const
    {
        const auto memory_properties = physical_device().getMemoryProperties();
        for (uint32_t index = 0; index < memory_properties.memoryTypeCount; ++index)
        {
            if ((type_filter & (1u << index)) != 0 &&
                (memory_properties.memoryTypes[index].propertyFlags & properties) == properties)
            {
                return index;
            }
        }

        throw std::runtime_error("Failed to find a suitable Vulkan memory type.");
    }

    void create_buffer(
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags properties,
        vk::UniqueBuffer& buffer,
        vk::UniqueDeviceMemory& memory)
    {
        const vk::BufferCreateInfo buffer_info = vk::BufferCreateInfo{}
            .setSize(size)
            .setUsage(usage)
            .setSharingMode(vk::SharingMode::eExclusive);
        buffer = device().createBufferUnique(buffer_info, nullptr, m_dispatch_loader_core);

        const auto memory_requirements = device().getBufferMemoryRequirements(buffer.get());
        const vk::MemoryAllocateInfo allocation_info = vk::MemoryAllocateInfo{}
            .setAllocationSize(memory_requirements.size)
            .setMemoryTypeIndex(find_memory_type(memory_requirements.memoryTypeBits, properties));
        memory = device().allocateMemoryUnique(allocation_info, nullptr, m_dispatch_loader_core);
        device().bindBufferMemory(buffer.get(), memory.get(), 0);
    }

    vk::CommandBuffer begin_single_time_commands()
    {
        const vk::CommandBufferAllocateInfo allocation_info = vk::CommandBufferAllocateInfo{}
            .setCommandPool(m_upload_command_pool.get())
            .setLevel(vk::CommandBufferLevel::ePrimary)
            .setCommandBufferCount(1);
        auto command_buffers = device().allocateCommandBuffers(allocation_info);
        vk::CommandBuffer command_buffer = command_buffers.front();

        const vk::CommandBufferBeginInfo begin_info = vk::CommandBufferBeginInfo{}
            .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        command_buffer.begin(begin_info);
        return command_buffer;
    }

    void end_single_time_commands(vk::CommandBuffer command_buffer)
    {
        command_buffer.end();
        const vk::SubmitInfo submit_info = vk::SubmitInfo{}.setCommandBuffers(command_buffer);
        graphics_queue().handle().submit(submit_info);
        graphics_queue().handle().waitIdle();
        device().freeCommandBuffers(m_upload_command_pool.get(), command_buffer);
    }

    void transition_image_layout(vk::CommandBuffer command_buffer, vk::Image image, vk::ImageLayout old_layout, vk::ImageLayout new_layout)
    {
        vk::AccessFlags source_access{};
        vk::AccessFlags destination_access{};
        vk::PipelineStageFlags source_stage{};
        vk::PipelineStageFlags destination_stage{};

        if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal)
        {
            source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
            destination_stage = vk::PipelineStageFlagBits::eTransfer;
            destination_access = vk::AccessFlagBits::eTransferWrite;
        }
        else if (old_layout == vk::ImageLayout::eTransferDstOptimal && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal)
        {
            source_stage = vk::PipelineStageFlagBits::eTransfer;
            destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
            source_access = vk::AccessFlagBits::eTransferWrite;
            destination_access = vk::AccessFlagBits::eShaderRead;
        }
        else
        {
            throw std::runtime_error("Unsupported texture layout transition.");
        }

        const vk::ImageMemoryBarrier barrier = vk::ImageMemoryBarrier{}
            .setOldLayout(old_layout)
            .setNewLayout(new_layout)
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setImage(image)
            .setSubresourceRange(vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1})
            .setSrcAccessMask(source_access)
            .setDstAccessMask(destination_access);
        command_buffer.pipelineBarrier(source_stage, destination_stage, {}, nullptr, nullptr, barrier);
    }

    void upload_texture_image(DemoTexture& texture, uint32_t width, uint32_t height)
    {
        vk::CommandBuffer command_buffer = begin_single_time_commands();
        transition_image_layout(command_buffer, texture.image.get(), vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        const vk::BufferImageCopy copy_region = vk::BufferImageCopy{}
            .setImageSubresource(vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1})
            .setImageExtent(vk::Extent3D{width, height, 1});
        command_buffer.copyBufferToImage(texture.staging_buffer.get(), texture.image.get(), vk::ImageLayout::eTransferDstOptimal, copy_region);

        transition_image_layout(command_buffer, texture.image.get(), vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
        end_single_time_commands(command_buffer);
    }

    vk::UniqueHandle<vk::Instance, DISPATCH_LOADER_CORE_TYPE> m_instance;
    vk::PhysicalDevice m_physical_device;
    vk::SurfaceKHR m_surface;
    vk::UniqueHandle<vk::Device, DISPATCH_LOADER_CORE_TYPE> m_device;
    mutable vk::Device m_device_handle;
    avk::queue m_graphics_queue;
    vk::UniqueHandle<vk::DescriptorPool, DISPATCH_LOADER_CORE_TYPE> m_descriptor_pool;
    vk::UniqueCommandPool m_upload_command_pool;
    std::vector<DemoTexture> m_textures;
    DISPATCH_LOADER_CORE_TYPE m_dispatch_loader_core;
    DISPATCH_LOADER_EXT_TYPE m_dispatch_loader_ext;
    AVK_MEM_ALLOCATOR_TYPE m_memory_allocator;
};

struct DemoApplication
{
    GLFWwindow* window = nullptr;
    AutoVkDemoRoot* root = nullptr;
    ImGui_ImplVulkanH_Window window_data{};
    bool swap_chain_rebuild = false;
    bool running = true;
    unsigned int desktop_texture = 0;
    std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
};

DemoApplication g_app;

void frame_render(AutoVkDemoRoot& root, ImGui_ImplVulkanH_Window& window_data, ImDrawData* draw_data, bool& swap_chain_rebuild)
{
    const auto& semaphore_set = window_data.FrameSemaphores[window_data.SemaphoreIndex];
    const vk::Semaphore image_acquired{semaphore_set.ImageAcquiredSemaphore};
    const vk::Semaphore render_complete{semaphore_set.RenderCompleteSemaphore};
    const auto acquire_result = root.device().acquireNextImageKHR(
        vk::SwapchainKHR{window_data.Swapchain},
        UINT64_MAX,
        image_acquired,
        nullptr,
        &window_data.FrameIndex,
        root.dispatch_loader_ext());

    if (acquire_result == vk::Result::eErrorOutOfDateKHR || acquire_result == vk::Result::eSuboptimalKHR)
    {
        swap_chain_rebuild = true;
    }
    if (acquire_result == vk::Result::eErrorOutOfDateKHR)
    {
        return;
    }
    if (acquire_result != vk::Result::eSuboptimalKHR)
    {
        check_vk_result(acquire_result);
    }

    ImGui_ImplVulkanH_Frame& frame = window_data.Frames[window_data.FrameIndex];
    const vk::Fence frame_fence{frame.Fence};
    const vk::CommandPool command_pool{frame.CommandPool};
    const vk::CommandBuffer command_buffer{frame.CommandBuffer};

    check_vk_result(root.device().waitForFences(frame_fence, vk::True, UINT64_MAX));
    root.device().resetFences(frame_fence);
    root.device().resetCommandPool(command_pool);

    const vk::CommandBufferBeginInfo begin_info = vk::CommandBufferBeginInfo{}
        .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    command_buffer.begin(begin_info);

    const vk::ClearValue clear_value = vk::ClearColorValue{std::array{
        window_data.ClearValue.color.float32[0],
        window_data.ClearValue.color.float32[1],
        window_data.ClearValue.color.float32[2],
        window_data.ClearValue.color.float32[3],
    }};

    const vk::RenderPassBeginInfo pass_info = vk::RenderPassBeginInfo{}
        .setRenderPass(vk::RenderPass{window_data.RenderPass})
        .setFramebuffer(vk::Framebuffer{frame.Framebuffer})
        .setRenderArea(vk::Rect2D{{0, 0}, {static_cast<uint32_t>(window_data.Width), static_cast<uint32_t>(window_data.Height)}})
        .setClearValues(clear_value);

    command_buffer.beginRenderPass(pass_info, vk::SubpassContents::eInline);
    ImGui_ImplVulkan_RenderDrawData(draw_data, static_cast<VkCommandBuffer>(command_buffer));
    command_buffer.endRenderPass();

    const std::array wait_semaphores{image_acquired};
    const std::array wait_stages{vk::PipelineStageFlags{vk::PipelineStageFlagBits::eColorAttachmentOutput}};
    const std::array command_buffers{command_buffer};
    const std::array signal_semaphores{render_complete};
    const vk::SubmitInfo submit_info = vk::SubmitInfo{}
        .setWaitSemaphores(wait_semaphores)
        .setWaitDstStageMask(wait_stages)
        .setCommandBuffers(command_buffers)
        .setSignalSemaphores(signal_semaphores);

    command_buffer.end();
    root.graphics_queue().handle().submit(submit_info, frame_fence);
}

void frame_present(AutoVkDemoRoot& root, ImGui_ImplVulkanH_Window& window_data, bool& swap_chain_rebuild)
{
    if (swap_chain_rebuild)
    {
        return;
    }

    const std::array wait_semaphores{vk::Semaphore{window_data.FrameSemaphores[window_data.SemaphoreIndex].RenderCompleteSemaphore}};
    const std::array swapchains{vk::SwapchainKHR{window_data.Swapchain}};
    const vk::PresentInfoKHR present_info = vk::PresentInfoKHR{}
        .setWaitSemaphores(wait_semaphores)
        .setSwapchains(swapchains)
        .setPImageIndices(&window_data.FrameIndex);

    const auto result = root.graphics_queue().handle().presentKHR(present_info, root.dispatch_loader_ext());
    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
    {
        swap_chain_rebuild = true;
    }
    if (result != vk::Result::eErrorOutOfDateKHR && result != vk::Result::eSuboptimalKHR)
    {
        check_vk_result(result);
    }
    window_data.SemaphoreIndex = (window_data.SemaphoreIndex + 1) % window_data.SemaphoreCount;
}
}

int gPressed[256] = {};
int gWasPressed[256] = {};
int gMouseX = 0;
int gMouseY = 0;

unsigned int DemoLoadTexture(const char* filename)
{
    if (g_app.root == nullptr)
    {
        return 0;
    }
    return static_cast<unsigned int>(g_app.root->load_texture(filename));
}

void DemoInit()
{
    for (int index = 0; index < 256; ++index)
    {
        gPressed[index] = 0;
        gWasPressed[index] = 0;
    }

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW.");
    }
    if (!glfwVulkanSupported())
    {
        throw std::runtime_error("GLFW reports that Vulkan is not supported.");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    g_app.window = glfwCreateWindow(k_window_width, k_window_height, "http://soloud-audio.com", nullptr, nullptr);
    if (g_app.window == nullptr)
    {
        throw std::runtime_error("Failed to create GLFW window.");
    }

    std::vector<const char*> extensions;
    uint32_t extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&extension_count);
    for (uint32_t index = 0; index < extension_count; ++index)
    {
        extensions.push_back(glfw_extensions[index]);
    }

    g_app.root = new AutoVkDemoRoot{std::move(extensions)};
    g_app.root->create_surface(g_app.window);
    g_app.root->create_device();

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(g_app.window, &width, &height);
    g_app.root->setup_window(g_app.window_data, width, height);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(g_app.window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_app.root->raw_instance();
    init_info.PhysicalDevice = g_app.root->raw_physical_device();
    init_info.Device = g_app.root->raw_device();
    init_info.QueueFamily = g_app.root->graphics_queue().family_index();
    init_info.Queue = g_app.root->raw_graphics_queue();
    init_info.DescriptorPool = g_app.root->raw_descriptor_pool();
    init_info.MinImageCount = k_min_image_count;
    init_info.ImageCount = g_app.window_data.ImageCount;
    init_info.Allocator = nullptr;
    init_info.PipelineInfoMain.RenderPass = g_app.window_data.RenderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info);

    g_app.desktop_texture = DemoLoadTexture("graphics/soloud_bg.png");
    g_app.start_time = std::chrono::steady_clock::now();
}

void DemoUpdateStart()
{
    glfwPollEvents();
    if (glfwWindowShouldClose(g_app.window))
    {
        g_app.running = false;
    }

    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(g_app.window, &framebuffer_width, &framebuffer_height);
    if (framebuffer_width > 0 && framebuffer_height > 0 &&
        (g_app.swap_chain_rebuild || g_app.window_data.Width != framebuffer_width || g_app.window_data.Height != framebuffer_height))
    {
        ImGui_ImplVulkan_SetMinImageCount(k_min_image_count);
        g_app.root->resize_window(g_app.window_data, framebuffer_width, framebuffer_height);
        g_app.window_data.FrameIndex = 0;
        g_app.swap_chain_rebuild = false;
    }

    double mouse_x = 0.0;
    double mouse_y = 0.0;
    glfwGetCursorPos(g_app.window, &mouse_x, &mouse_y);
    gMouseX = static_cast<int>(mouse_x);
    gMouseY = static_cast<int>(mouse_y);

    for (int index = 0; index < 256; ++index)
    {
        gWasPressed[index] = gPressed[index];
        gPressed[index] = glfwGetKey(g_app.window, index) == GLFW_PRESS ? 1 : 0;
    }

    if (gPressed[GLFW_KEY_ESCAPE] != 0 && !ImGui::GetIO().WantCaptureKeyboard)
    {
        g_app.running = false;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (g_app.desktop_texture != 0)
    {
        DemoTexQuad(g_app.desktop_texture, 0.0f, 0.0f, static_cast<float>(k_window_width), static_cast<float>(k_window_height));
    }
}

void DemoUpdateEnd()
{
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    if (draw_data->DisplaySize.x > 0.0f && draw_data->DisplaySize.y > 0.0f)
    {
        g_app.window_data.ClearValue.color.float32[0] = 0.2f;
        g_app.window_data.ClearValue.color.float32[1] = 0.2f;
        g_app.window_data.ClearValue.color.float32[2] = 0.4f;
        g_app.window_data.ClearValue.color.float32[3] = 1.0f;
        frame_render(*g_app.root, g_app.window_data, draw_data, g_app.swap_chain_rebuild);
        frame_present(*g_app.root, g_app.window_data, g_app.swap_chain_rebuild);
    }
}

int DemoTick()
{
    const auto elapsed = std::chrono::steady_clock::now() - g_app.start_time;
    return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

void DemoYield()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void DemoTriangle(float x0, float y0, float x1, float y1, float x2, float y2, unsigned int color)
{
    ImGui::GetBackgroundDrawList()->AddTriangleFilled(ImVec2{x0, y0}, ImVec2{x1, y1}, ImVec2{x2, y2}, color);
}

void DemoQuad(float x0, float y0, float x1, float y1, unsigned int color)
{
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2{x0, y0}, ImVec2{x1, y1}, color);
}

void DemoTexQuad(int texture_id, float x0, float y0, float x1, float y1)
{
    if (g_app.root == nullptr)
    {
        return;
    }

    const DemoTexture* texture = g_app.root->texture(texture_id);
    if (texture == nullptr || texture->descriptor_set == VK_NULL_HANDLE)
    {
        return;
    }

    ImGui::GetBackgroundDrawList()->AddImage(
        to_imgui_texture_id(texture->descriptor_set),
        ImVec2{x0, y0},
        ImVec2{x1, y1});
}

extern int DemoEntry(int argc, char* argv[]);
extern void DemoMainloop();

int main(int argc, char* argv[])
{
    try
    {
        std::filesystem::current_path(std::filesystem::path(AUDIO_SYSTEM_SOURCE_DIR) / "data" / "soloud_official");
        DemoInit();
        const int result = DemoEntry(argc, argv);
        if (result != 0)
        {
            return result;
        }

        while (g_app.running)
        {
            if (glfwGetWindowAttrib(g_app.window, GLFW_ICONIFIED) != 0)
            {
                ImGui_ImplGlfw_Sleep(10);
                glfwPollEvents();
                continue;
            }

            DemoMainloop();
        }

        g_app.root->device().waitIdle();
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        g_app.root->cleanup_window(g_app.window_data);
        delete g_app.root;
        g_app.root = nullptr;
        glfwDestroyWindow(g_app.window);
        glfwTerminate();
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
