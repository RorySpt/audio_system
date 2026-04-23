#define GLFW_INCLUDE_VULKAN

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "soloud.h"
#include "soloud_wav.h"

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <numbers>
#include <string>

namespace
{
constexpr unsigned int k_sample_rate = 48000;
constexpr unsigned int k_channels = 2;
constexpr unsigned int k_buffer_size = 512;
constexpr float k_default_orbit_radius = 3.0f;
constexpr float k_default_orbit_speed = 0.65f;
constexpr float k_click_interval_seconds = 0.85f;

VkAllocationCallbacks* g_allocator = nullptr;
VkInstance g_instance = VK_NULL_HANDLE;
VkPhysicalDevice g_physical_device = VK_NULL_HANDLE;
VkDevice g_device = VK_NULL_HANDLE;
uint32_t g_queue_family = static_cast<uint32_t>(-1);
VkQueue g_queue = VK_NULL_HANDLE;
VkPipelineCache g_pipeline_cache = VK_NULL_HANDLE;
VkDescriptorPool g_descriptor_pool = VK_NULL_HANDLE;
ImGui_ImplVulkanH_Window g_main_window_data;
uint32_t g_min_image_count = 2;
bool g_swap_chain_rebuild = false;

void glfw_error_callback(int error, const char* description)
{
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

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

bool is_extension_available(const ImVector<VkExtensionProperties>& properties, const char* extension)
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

std::filesystem::path get_spatial_demo_dir()
{
    return std::filesystem::path(AUDIO_SYSTEM_SOURCE_DIR) / "data" / "spatial_demo";
}

bool load_sound(SoLoud::Wav& sound, const std::filesystem::path& path, const char* label)
{
    const auto result = sound.load(path.string().c_str());
    if (result != SoLoud::SO_NO_ERROR)
    {
        std::cerr << "Failed to load " << label << ": " << path.string() << '\n';
        return false;
    }

    return true;
}

void setup_vulkan(ImVector<const char*> instance_extensions)
{
    VkResult result = VK_SUCCESS;

    {
        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

        uint32_t properties_count = 0;
        ImVector<VkExtensionProperties> properties;
        vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
        properties.resize(properties_count);
        result = vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.Data);
        check_vk_result(result);

        if (is_extension_available(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
        {
            instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        }

#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
        if (is_extension_available(properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
        {
            instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
#endif

        create_info.enabledExtensionCount = static_cast<uint32_t>(instance_extensions.Size);
        create_info.ppEnabledExtensionNames = instance_extensions.Data;
        result = vkCreateInstance(&create_info, g_allocator, &g_instance);
        check_vk_result(result);
    }

    g_physical_device = ImGui_ImplVulkanH_SelectPhysicalDevice(g_instance);
    IM_ASSERT(g_physical_device != VK_NULL_HANDLE);

    g_queue_family = ImGui_ImplVulkanH_SelectQueueFamilyIndex(g_physical_device);
    IM_ASSERT(g_queue_family != static_cast<uint32_t>(-1));

    {
        ImVector<const char*> device_extensions;
        device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        uint32_t properties_count = 0;
        ImVector<VkExtensionProperties> properties;
        vkEnumerateDeviceExtensionProperties(g_physical_device, nullptr, &properties_count, nullptr);
        properties.resize(properties_count);
        vkEnumerateDeviceExtensionProperties(g_physical_device, nullptr, &properties_count, properties.Data);

#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
        if (is_extension_available(properties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
        {
            device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
        }
#endif

        const float queue_priority[] = {1.0f};
        VkDeviceQueueCreateInfo queue_info = {};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = g_queue_family;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = queue_priority;

        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = 1;
        create_info.pQueueCreateInfos = &queue_info;
        create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.Size);
        create_info.ppEnabledExtensionNames = device_extensions.Data;
        result = vkCreateDevice(g_physical_device, &create_info, g_allocator, &g_device);
        check_vk_result(result);
        vkGetDeviceQueue(g_device, g_queue_family, 0, &g_queue);
    }

    {
        VkDescriptorPoolSize pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE},
            {VK_DESCRIPTOR_TYPE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE},
        };

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 0;
        for (const auto& pool_size : pool_sizes)
        {
            pool_info.maxSets += pool_size.descriptorCount;
        }
        pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
        pool_info.pPoolSizes = pool_sizes;
        result = vkCreateDescriptorPool(g_device, &pool_info, g_allocator, &g_descriptor_pool);
        check_vk_result(result);
    }
}

void setup_vulkan_window(ImGui_ImplVulkanH_Window* window_data, VkSurfaceKHR surface, int width, int height)
{
    VkBool32 supports_wsi = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(g_physical_device, g_queue_family, surface, &supports_wsi);
    if (supports_wsi != VK_TRUE)
    {
        std::cerr << "Selected Vulkan queue family does not support the window surface.\n";
        std::exit(1);
    }

    const VkFormat request_surface_image_format[] = {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8_UNORM,
        VK_FORMAT_R8G8B8_UNORM,
    };
    constexpr VkColorSpaceKHR request_surface_color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    const VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_FIFO_KHR};

    window_data->Surface = surface;
    window_data->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
        g_physical_device,
        window_data->Surface,
        request_surface_image_format,
        std::size(request_surface_image_format),
        request_surface_color_space);
    window_data->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
        g_physical_device,
        window_data->Surface,
        present_modes,
        std::size(present_modes));

    IM_ASSERT(g_min_image_count >= 2);
    ImGui_ImplVulkanH_CreateOrResizeWindow(
        g_instance,
        g_physical_device,
        g_device,
        window_data,
        g_queue_family,
        g_allocator,
        width,
        height,
        g_min_image_count,
        0);
}

void cleanup_vulkan_window(ImGui_ImplVulkanH_Window* window_data)
{
    ImGui_ImplVulkanH_DestroyWindow(g_instance, g_device, window_data, g_allocator);
    vkDestroySurfaceKHR(g_instance, window_data->Surface, g_allocator);
}

void cleanup_vulkan()
{
    vkDestroyDescriptorPool(g_device, g_descriptor_pool, g_allocator);
    vkDestroyDevice(g_device, g_allocator);
    vkDestroyInstance(g_instance, g_allocator);
}

void frame_render(ImGui_ImplVulkanH_Window* window_data, ImDrawData* draw_data)
{
    VkSemaphore image_acquired_semaphore = window_data->FrameSemaphores[window_data->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = window_data->FrameSemaphores[window_data->SemaphoreIndex].RenderCompleteSemaphore;
    VkResult result = vkAcquireNextImageKHR(
        g_device,
        window_data->Swapchain,
        UINT64_MAX,
        image_acquired_semaphore,
        VK_NULL_HANDLE,
        &window_data->FrameIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        g_swap_chain_rebuild = true;
    }
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return;
    }
    if (result != VK_SUBOPTIMAL_KHR)
    {
        check_vk_result(result);
    }

    ImGui_ImplVulkanH_Frame* frame_data = &window_data->Frames[window_data->FrameIndex];
    result = vkWaitForFences(g_device, 1, &frame_data->Fence, VK_TRUE, UINT64_MAX);
    check_vk_result(result);
    result = vkResetFences(g_device, 1, &frame_data->Fence);
    check_vk_result(result);

    result = vkResetCommandPool(g_device, frame_data->CommandPool, 0);
    check_vk_result(result);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = vkBeginCommandBuffer(frame_data->CommandBuffer, &begin_info);
    check_vk_result(result);

    VkRenderPassBeginInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = window_data->RenderPass;
    render_pass_info.framebuffer = frame_data->Framebuffer;
    render_pass_info.renderArea.extent.width = window_data->Width;
    render_pass_info.renderArea.extent.height = window_data->Height;
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &window_data->ClearValue;
    vkCmdBeginRenderPass(frame_data->CommandBuffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    ImGui_ImplVulkan_RenderDrawData(draw_data, frame_data->CommandBuffer);

    vkCmdEndRenderPass(frame_data->CommandBuffer);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &image_acquired_semaphore;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &frame_data->CommandBuffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_complete_semaphore;

    result = vkEndCommandBuffer(frame_data->CommandBuffer);
    check_vk_result(result);
    result = vkQueueSubmit(g_queue, 1, &submit_info, frame_data->Fence);
    check_vk_result(result);
}

void frame_present(ImGui_ImplVulkanH_Window* window_data)
{
    if (g_swap_chain_rebuild)
    {
        return;
    }

    VkSemaphore render_complete_semaphore = window_data->FrameSemaphores[window_data->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_complete_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &window_data->Swapchain;
    present_info.pImageIndices = &window_data->FrameIndex;

    VkResult result = vkQueuePresentKHR(g_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        g_swap_chain_rebuild = true;
    }
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return;
    }
    if (result != VK_SUBOPTIMAL_KHR)
    {
        check_vk_result(result);
    }

    window_data->SemaphoreIndex = (window_data->SemaphoreIndex + 1) % window_data->SemaphoreCount;
}

struct AudioDemoState
{
    SoLoud::Soloud engine;
    SoLoud::Wav wind_sound;
    SoLoud::Wav crickets_sound;
    SoLoud::Wav click_sound;
    SoLoud::handle wind_voice = 0;
    SoLoud::handle crickets_voice = 0;
    float orbit_radius = k_default_orbit_radius;
    float orbit_speed = k_default_orbit_speed;
    float source_height = 0.0f;
    float master_volume = 0.8f;
    float time_seconds = 0.0f;
    float next_click_time_seconds = 0.0f;
    bool auto_click = true;
    bool initialized = false;
};

bool init_audio(AudioDemoState& state)
{
    const auto init_result = state.engine.init(
        SoLoud::Soloud::CLIP_ROUNDOFF,
        SoLoud::Soloud::MINIAUDIO,
        k_sample_rate,
        k_buffer_size,
        k_channels);
    if (init_result != SoLoud::SO_NO_ERROR)
    {
        std::cerr << "Failed to initialize SoLoud: " << state.engine.getErrorString(init_result) << '\n';
        return false;
    }

    const auto data_dir = get_spatial_demo_dir();
    if (!load_sound(state.wind_sound, data_dir / "wind1.wav", "wind1.wav") ||
        !load_sound(state.crickets_sound, data_dir / "crickets_loop.mp3", "crickets_loop.mp3") ||
        !load_sound(state.click_sound, data_dir / "click.wav", "click.wav"))
    {
        state.engine.deinit();
        return false;
    }

    // 循环环境声适合常驻，短促提示音则按位置反复触发。
    state.wind_sound.setLooping(true);
    state.crickets_sound.setLooping(true);
    state.wind_sound.setVolume(0.55f);
    state.crickets_sound.setVolume(0.7f);
    state.click_sound.setVolume(1.0f);

    state.wind_sound.set3dMinMaxDistance(1.0f, 18.0f);
    state.crickets_sound.set3dMinMaxDistance(1.0f, 18.0f);
    state.click_sound.set3dMinMaxDistance(0.5f, 10.0f);
    state.wind_sound.set3dAttenuation(SoLoud::AudioSource::LINEAR_DISTANCE, 1.0f);
    state.crickets_sound.set3dAttenuation(SoLoud::AudioSource::LINEAR_DISTANCE, 1.0f);
    state.click_sound.set3dAttenuation(SoLoud::AudioSource::LINEAR_DISTANCE, 1.0f);

    // 监听者固定在原点，面朝 +Z，Y 轴向上；这也是很多游戏音频系统的常见坐标约定。
    state.engine.set3dListenerParameters(
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f);
    state.engine.setGlobalVolume(state.master_volume);

    state.wind_voice = state.engine.play3d(state.wind_sound, -5.0f, 0.0f, 3.0f);
    state.crickets_voice = state.engine.play3d(state.crickets_sound, 4.0f, 0.0f, -4.0f);
    state.engine.update3dAudio();
    state.initialized = true;
    return true;
}

void play_orbit_click(AudioDemoState& state, float source_x, float source_z, float velocity_x, float velocity_z)
{
    const auto voice = state.engine.play3d(
        state.click_sound,
        source_x,
        state.source_height,
        source_z,
        velocity_x,
        0.0f,
        velocity_z);
    state.engine.set3dSourceMinMaxDistance(voice, 0.5f, 10.0f);
    state.engine.set3dSourceAttenuation(voice, SoLoud::AudioSource::LINEAR_DISTANCE, 1.0f);
}

void update_audio(AudioDemoState& state, float delta_seconds)
{
    state.time_seconds += delta_seconds;
    const float angle = state.time_seconds * state.orbit_speed * std::numbers::pi_v<float> * 2.0f;
    const float prev_angle = (state.time_seconds - delta_seconds) * state.orbit_speed * std::numbers::pi_v<float> * 2.0f;
    const float source_x = std::cos(angle) * state.orbit_radius;
    const float source_z = std::sin(angle) * state.orbit_radius;
    const float prev_x = std::cos(prev_angle) * state.orbit_radius;
    const float prev_z = std::sin(prev_angle) * state.orbit_radius;
    const float velocity_x = delta_seconds > 0.0f ? (source_x - prev_x) / delta_seconds : 0.0f;
    const float velocity_z = delta_seconds > 0.0f ? (source_z - prev_z) / delta_seconds : 0.0f;

    if (state.auto_click && state.time_seconds >= state.next_click_time_seconds)
    {
        play_orbit_click(state, source_x, source_z, velocity_x, velocity_z);
        state.next_click_time_seconds += k_click_interval_seconds;
    }

    state.engine.setGlobalVolume(state.master_volume);
    state.engine.update3dAudio();
}

void draw_audio_panel(AudioDemoState& state)
{
    const float angle = state.time_seconds * state.orbit_speed * std::numbers::pi_v<float> * 2.0f;
    const float source_x = std::cos(angle) * state.orbit_radius;
    const float source_z = std::sin(angle) * state.orbit_radius;

    ImGui::Begin("SoLoud 3D Audio");
    ImGui::TextUnformatted("GLFW + Vulkan + ImGui");
    ImGui::Separator();
    ImGui::Text("Backend: SoLoud MINIAUDIO");
    ImGui::Text("Voices: %d", state.engine.getVoiceCount());
    ImGui::SliderFloat("Master volume", &state.master_volume, 0.0f, 1.0f);
    ImGui::SliderFloat("Orbit radius", &state.orbit_radius, 0.5f, 8.0f);
    ImGui::SliderFloat("Orbit speed", &state.orbit_speed, 0.05f, 2.0f);
    ImGui::SliderFloat("Source height", &state.source_height, -2.0f, 2.0f);
    ImGui::Checkbox("Auto click", &state.auto_click);

    if (ImGui::Button("Play click now"))
    {
        play_orbit_click(state, source_x, source_z, 0.0f, 0.0f);
    }

    ImGui::Separator();
    ImGui::Text("Moving source: x %.2f, y %.2f, z %.2f", source_x, state.source_height, source_z);
    ImGui::TextUnformatted("Listener stays at origin, facing +Z.");
    ImGui::End();
}
}

int main()
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
    {
        return 1;
    }

    if (!glfwVulkanSupported())
    {
        std::cerr << "GLFW reports that Vulkan is not supported on this machine.\n";
        glfwTerminate();
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    const float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    GLFWwindow* window = glfwCreateWindow(
        static_cast<int>(1280.0f * main_scale),
        static_cast<int>(800.0f * main_scale),
        "audio_system - GLFW + Vulkan + ImGui",
        nullptr,
        nullptr);
    if (window == nullptr)
    {
        glfwTerminate();
        return 1;
    }

    ImVector<const char*> extensions;
    uint32_t extensions_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
    for (uint32_t index = 0; index < extensions_count; ++index)
    {
        extensions.push_back(glfw_extensions[index]);
    }
    setup_vulkan(extensions);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult result = glfwCreateWindowSurface(g_instance, window, g_allocator, &surface);
    check_vk_result(result);

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    ImGui_ImplVulkanH_Window* window_data = &g_main_window_data;
    setup_vulkan_window(window_data, surface, width, height);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_instance;
    init_info.PhysicalDevice = g_physical_device;
    init_info.Device = g_device;
    init_info.QueueFamily = g_queue_family;
    init_info.Queue = g_queue;
    init_info.PipelineCache = g_pipeline_cache;
    init_info.DescriptorPool = g_descriptor_pool;
    init_info.MinImageCount = g_min_image_count;
    init_info.ImageCount = window_data->ImageCount;
    init_info.Allocator = g_allocator;
    init_info.PipelineInfoMain.RenderPass = window_data->RenderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info);

    AudioDemoState audio_state;
    if (!init_audio(audio_state))
    {
        vkDeviceWaitIdle(g_device);
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        cleanup_vulkan_window(&g_main_window_data);
        cleanup_vulkan();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    ImVec4 clear_color = ImVec4(0.10f, 0.12f, 0.14f, 1.0f);
    double previous_time = glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
        if (framebuffer_width > 0 && framebuffer_height > 0 &&
            (g_swap_chain_rebuild || g_main_window_data.Width != framebuffer_width || g_main_window_data.Height != framebuffer_height))
        {
            ImGui_ImplVulkan_SetMinImageCount(g_min_image_count);
            ImGui_ImplVulkanH_CreateOrResizeWindow(
                g_instance,
                g_physical_device,
                g_device,
                window_data,
                g_queue_family,
                g_allocator,
                framebuffer_width,
                framebuffer_height,
                g_min_image_count,
                0);
            g_main_window_data.FrameIndex = 0;
            g_swap_chain_rebuild = false;
        }

        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        const double current_time = glfwGetTime();
        const float delta_seconds = static_cast<float>(current_time - previous_time);
        previous_time = current_time;
        update_audio(audio_state, delta_seconds);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        draw_audio_panel(audio_state);

        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool minimized = draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f;
        if (!minimized)
        {
            window_data->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
            window_data->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
            window_data->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
            window_data->ClearValue.color.float32[3] = clear_color.w;
            frame_render(window_data, draw_data);
            frame_present(window_data);
        }
    }

    audio_state.engine.deinit();

    result = vkDeviceWaitIdle(g_device);
    check_vk_result(result);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    cleanup_vulkan_window(&g_main_window_data);
    cleanup_vulkan();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
