#include "imr/imr.h"
#include "imr/util.h"

#include <fstream>

#include "VkBootstrap.h"

#include "initializers.h"

#include "libs/camera.h"
//#include "libs/model.h"

Camera camera;
CameraFreelookState camera_state = {
    .fly_speed = 1.0f,
    .mouse_sensitivity = 1,
};
CameraInput camera_input;

static auto time() -> uint64_t {
    timespec t = { 0 };
    timespec_get(&t, TIME_UTC);
    return t.tv_sec * 1000000000 + t.tv_nsec;
}

void camera_update(GLFWwindow*, CameraInput* input);

static VkShaderModule createShaderModule(imr::Device& device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data()); //This requires the code to be int aligned, not char aligned!

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device.device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        throw std::runtime_error("failed to create shader module!");
    return shaderModule;
}

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }
    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

bool hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

static VkFormat findSupportedFormat(imr::Device& device, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(device.physical_device, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

VkPipelineShaderStageCreateInfo load_shader(imr::Device& device, const std::string& filename, VkShaderStageFlagBits stage_bits) {
    auto shaderCode = readFile(filename);
    VkShaderModule shaderModule = createShaderModule(device, shaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = stage_bits;
    vertShaderStageInfo.module = shaderModule;
    vertShaderStageInfo.pName = "main";
    return vertShaderStageInfo;
}

VkPipelineLayout create_pipeline_layout(imr::Device& device) {
    VkPushConstantRange range = initializers::push_constant_range(
            VK_SHADER_STAGE_VERTEX_BIT,
            16 * 4, //mat4 should™ have this size
            0
    );

    VkPipelineLayoutCreateInfo pipelineLayoutInfo =
        initializers::pipeline_layout_create_info(nullptr, 0);
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &range;
    VkPipelineLayout pipeline_layout;
    vkCreatePipelineLayout(device.device, &pipelineLayoutInfo, nullptr, &pipeline_layout);
    return pipeline_layout;
}

struct Vertex {
    vec3 pos;
    vec3 color;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};

const vec3 vertex_1_color = {1.0f, 0.0f, 0.0f};
const vec3 vertex_2_color = {0.0f, 1.0f, 0.0f};
const vec3 vertex_3_color = {1.0f, 1.0f, 0.0f};
const vec3 vertex_4_color = {0.0f, 0.0f, 1.0f};
const vec3 vertex_5_color = {1.0f, 0.0f, 1.0f};
const vec3 vertex_6_color = {0.0f, 1.0f, 1.0f};
const vec3 vertex_7_color = {1.0f, 1.0f, 1.0f};

const std::vector<Vertex> vertices = {
    //face 1
    {{-0.5f, -0.5f,  0.5f}, vertex_1_color},
    {{ 0.5f,  0.5f,  0.5f}, vertex_2_color},
    {{-0.5f,  0.5f,  0.5f}, vertex_3_color},

    {{-0.5f, -0.5f,  0.5f}, vertex_1_color},
    {{ 0.5f, -0.5f,  0.5f}, vertex_2_color},
    {{ 0.5f,  0.5f,  0.5f}, vertex_3_color},

    //face 2
    {{ 0.5f,  0.5f, -0.5f}, vertex_1_color},
    {{-0.5f, -0.5f, -0.5f}, vertex_2_color},
    {{-0.5f,  0.5f, -0.5f}, vertex_3_color},

    {{ 0.5f, -0.5f, -0.5f}, vertex_1_color},
    {{-0.5f, -0.5f, -0.5f}, vertex_2_color},
    {{ 0.5f,  0.5f, -0.5f}, vertex_3_color},

    //face 3
    {{ 0.5f,  0.5f,  0.5f}, vertex_1_color},
    {{-0.5f,  0.5f, -0.5f}, vertex_2_color},
    {{-0.5f,  0.5f,  0.5f}, vertex_3_color},

    {{ 0.5f,  0.5f, -0.5f}, vertex_1_color},
    {{-0.5f,  0.5f, -0.5f}, vertex_2_color},
    {{ 0.5f,  0.5f,  0.5f}, vertex_3_color},

    //face 4
    {{ 0.5f, -0.5f,  0.5f}, vertex_1_color},
    {{-0.5f, -0.5f,  0.5f}, vertex_2_color},
    {{-0.5f, -0.5f, -0.5f}, vertex_3_color},

    {{ 0.5f, -0.5f, -0.5f}, vertex_1_color},
    {{ 0.5f, -0.5f,  0.5f}, vertex_2_color},
    {{-0.5f, -0.5f, -0.5f}, vertex_3_color},

    //face 5
    {{ 0.5f,  0.5f,  0.5f}, vertex_1_color},
    {{ 0.5f, -0.5f,  0.5f}, vertex_2_color},
    {{ 0.5f, -0.5f, -0.5f}, vertex_3_color},

    {{ 0.5f,  0.5f, -0.5f}, vertex_1_color},
    {{ 0.5f,  0.5f,  0.5f}, vertex_2_color},
    {{ 0.5f, -0.5f, -0.5f}, vertex_3_color},

    //face 6
    {{-0.5f,  0.5f,  0.5f}, vertex_1_color},
    {{-0.5f, -0.5f, -0.5f}, vertex_2_color},
    {{-0.5f, -0.5f,  0.5f}, vertex_3_color},

    {{-0.5f,  0.5f,  0.5f}, vertex_1_color},
    {{-0.5f,  0.5f, -0.5f}, vertex_2_color},
    {{-0.5f, -0.5f, -0.5f}, vertex_3_color},

    //face 1
    {{-0.5f, -0.5f,  0.5f-2.0f}, vertex_4_color},
    {{ 0.5f,  0.5f,  0.5f-2.0f}, vertex_5_color},
    {{-0.5f,  0.5f,  0.5f-2.0f}, vertex_6_color},

    {{-0.5f, -0.5f,  0.5f-2.0f}, vertex_4_color},
    {{ 0.5f, -0.5f,  0.5f-2.0f}, vertex_5_color},
    {{ 0.5f,  0.5f,  0.5f-2.0f}, vertex_6_color},

    //face 2
    {{ 0.5f,  0.5f, -0.5f-2.0f}, vertex_4_color},
    {{-0.5f, -0.5f, -0.5f-2.0f}, vertex_5_color},
    {{-0.5f,  0.5f, -0.5f-2.0f}, vertex_6_color},

    {{ 0.5f, -0.5f, -0.5f-2.0f}, vertex_4_color},
    {{-0.5f, -0.5f, -0.5f-2.0f}, vertex_5_color},
    {{ 0.5f,  0.5f, -0.5f-2.0f}, vertex_6_color},

    //face 3
    {{ 0.5f,  0.5f,  0.5f-2.0f}, vertex_4_color},
    {{-0.5f,  0.5f, -0.5f-2.0f}, vertex_5_color},
    {{-0.5f,  0.5f,  0.5f-2.0f}, vertex_6_color},

    {{ 0.5f,  0.5f, -0.5f-2.0f}, vertex_4_color},
    {{-0.5f,  0.5f, -0.5f-2.0f}, vertex_5_color},
    {{ 0.5f,  0.5f,  0.5f-2.0f}, vertex_6_color},

    //face 4
    {{ 0.5f, -0.5f,  0.5f-2.0f}, vertex_4_color},
    {{-0.5f, -0.5f,  0.5f-2.0f}, vertex_5_color},
    {{-0.5f, -0.5f, -0.5f-2.0f}, vertex_6_color},

    {{ 0.5f, -0.5f, -0.5f-2.0f}, vertex_4_color},
    {{ 0.5f, -0.5f,  0.5f-2.0f}, vertex_5_color},
    {{-0.5f, -0.5f, -0.5f-2.0f}, vertex_6_color},

    //face 5
    {{ 0.5f,  0.5f,  0.5f-2.0f}, vertex_4_color},
    {{ 0.5f, -0.5f,  0.5f-2.0f}, vertex_5_color},
    {{ 0.5f, -0.5f, -0.5f-2.0f}, vertex_6_color},

    {{ 0.5f,  0.5f, -0.5f-2.0f}, vertex_4_color},
    {{ 0.5f,  0.5f,  0.5f-2.0f}, vertex_5_color},
    {{ 0.5f, -0.5f, -0.5f-2.0f}, vertex_6_color},

    //face 6
    {{-0.5f,  0.5f,  0.5f-2.0f}, vertex_4_color},
    {{-0.5f, -0.5f, -0.5f-2.0f}, vertex_5_color},
    {{-0.5f, -0.5f,  0.5f-2.0f}, vertex_6_color},

    {{-0.5f,  0.5f,  0.5f-2.0f}, vertex_4_color},
    {{-0.5f,  0.5f, -0.5f-2.0f}, vertex_5_color},
    {{-0.5f, -0.5f, -0.5f-2.0f}, vertex_6_color},
};

static int TESSELATION = 50;
static float GRID_SIZE = 0.1f;

void create_flat_surface(std::vector<Vertex> & data) {
    for (int xi = -TESSELATION ; xi < TESSELATION; xi++) {
        for (int zi = -TESSELATION ; zi < TESSELATION; zi++) {
            Vertex a = {{(xi + 1) * GRID_SIZE,  0.f, (zi + 1) * GRID_SIZE}, vertex_2_color};
            Vertex b = {{     xi  * GRID_SIZE,  0.f, (zi + 1) * GRID_SIZE}, vertex_2_color};
            Vertex c = {{(xi + 1) * GRID_SIZE,  0.f,      zi  * GRID_SIZE}, vertex_2_color};
            Vertex d = {{     xi  * GRID_SIZE,  0.f,      zi  * GRID_SIZE}, vertex_2_color};
            data.push_back(a);
            data.push_back(b);
            data.push_back(d);
            data.push_back(a);
            data.push_back(d);
            data.push_back(c);
        }
    }
}

VkPipeline create_pipeline(imr::Device& device, imr::Swapchain& swapchain, VkPipelineLayout& pipeline_layout, bool use_glsl) {
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state =
        initializers::pipeline_input_assembly_state_create_info(
                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                0,
                VK_FALSE);

    VkPipelineRasterizationStateCreateInfo rasterization_state =
        initializers::pipeline_rasterization_state_create_info(
                VK_POLYGON_MODE_FILL,
                VK_CULL_MODE_BACK_BIT,
                VK_FRONT_FACE_CLOCKWISE,
                0);

    VkPipelineColorBlendAttachmentState blend_attachment_state =
        initializers::pipeline_color_blend_attachment_state(
                0xf,
                VK_FALSE);

    const auto color_attachment_state = initializers::pipeline_color_blend_attachment_state(0xf, VK_FALSE);

    VkPipelineColorBlendStateCreateInfo color_blend_state =
        initializers::pipeline_color_blend_state_create_info(
                1,
                &blend_attachment_state);
    color_blend_state.attachmentCount = 1;
    color_blend_state.pAttachments    = &color_attachment_state;

    // Note: Using reversed depth-buffer for increased precision, so Greater depth values are kept
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state =
        initializers::pipeline_depth_stencil_state_create_info(
                VK_TRUE,
                VK_TRUE,
                VK_COMPARE_OP_LESS);
    depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
    depth_stencil_state.stencilTestEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state =
        initializers::pipeline_viewport_state_create_info(1, 1, 0);

    VkPipelineMultisampleStateCreateInfo multisample_state =
        initializers::pipeline_multisample_state_create_info(
                VK_SAMPLE_COUNT_1_BIT,
                0);

    std::vector<VkDynamicState> dynamic_state_enables = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state =
        initializers::pipeline_dynamic_state_create_info(
                dynamic_state_enables.data(),
                static_cast<uint32_t>(dynamic_state_enables.size()),
                0);

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertex_input_state = initializers::pipeline_vertex_input_state_create_info();
    vertex_input_state.vertexBindingDescriptionCount        = 1;
    vertex_input_state.pVertexBindingDescriptions           = &bindingDescription;
    vertex_input_state.vertexAttributeDescriptionCount      = static_cast<uint32_t>(attributeDescriptions.size());
    vertex_input_state.pVertexAttributeDescriptions         = attributeDescriptions.data();

    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{};
    if (use_glsl) {
        shader_stages[0] = load_shader(device, "shaders/shader.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shader_stages[1] = load_shader(device, "shaders/shader.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    } else {
        shader_stages[0] = load_shader(device, "shaders/shader.vert.cpp.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shader_stages[1] = load_shader(device, "shaders/shader.frag.cpp.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    // Create graphics pipeline for dynamic rendering
    VkFormat color_rendering_format = swapchain.format();
    auto depth_format = findSupportedFormat(device,
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    // Provide information for dynamic rendering
    VkPipelineRenderingCreateInfoKHR pipeline_create{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
    pipeline_create.pNext                   = VK_NULL_HANDLE;
    pipeline_create.colorAttachmentCount    = 1;
    pipeline_create.pColorAttachmentFormats = &color_rendering_format;
    pipeline_create.depthAttachmentFormat   = depth_format;
    if (!hasStencilComponent(depth_format))
    {
        pipeline_create.stencilAttachmentFormat = depth_format;
    }

    // Use the pNext to point to the rendering create struct
    VkGraphicsPipelineCreateInfo graphics_create{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    graphics_create.pNext               = &pipeline_create;
    graphics_create.renderPass          = VK_NULL_HANDLE;
    graphics_create.pInputAssemblyState = &input_assembly_state;
    graphics_create.pRasterizationState = &rasterization_state;
    graphics_create.pColorBlendState    = &color_blend_state;
    graphics_create.pMultisampleState   = &multisample_state;
    graphics_create.pViewportState      = &viewport_state;
    graphics_create.pDepthStencilState  = &depth_stencil_state;
    graphics_create.pDynamicState       = &dynamic_state;
    graphics_create.pVertexInputState   = &vertex_input_state;
    graphics_create.stageCount          = static_cast<uint32_t>(shader_stages.size());
    graphics_create.pStages             = shader_stages.data();
    graphics_create.layout              = pipeline_layout;

    VkPipeline graphicsPipeline;
    vkCreateGraphicsPipelines(device.device, VK_NULL_HANDLE, 1, &graphics_create, VK_NULL_HANDLE, &graphicsPipeline);
    return graphicsPipeline;
}

VkImageView createImageView(imr::Device& device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device.device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image view!");
    }

    return imageView;
}

struct CommandArguments {
    bool use_glsl = false;
    std::optional<float> camera_speed;
    std::optional<vec3> camera_eye;
    std::optional<vec2> camera_rotation;
    std::optional<float> camera_fov;
};

int main(int argc, char ** argv) {
    char * model_filename = nullptr;
    CommandArguments cmd_args;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--speed") == 0) {
            cmd_args.camera_speed = strtof(argv[++i], nullptr);
            continue;
        }
        if (strcmp(argv[i], "--position") == 0) {
            vec3 pos;
            pos.x = strtof(argv[++i], nullptr);
            pos.y = strtof(argv[++i], nullptr);
            pos.z = strtof(argv[++i], nullptr);
            cmd_args.camera_eye = pos;
            continue;
        }
        if (strcmp(argv[i], "--rotation") == 0) {
            vec2 rot;
            rot.x = strtof(argv[++i], nullptr);
            rot.y = strtof(argv[++i], nullptr);
            cmd_args.camera_rotation = rot;
            continue;
        }
        if (strcmp(argv[i], "--fov") == 0) {
            vec2 rot;
            cmd_args.camera_fov = strtof(argv[++i], nullptr);
            continue;
        }
        if (strcmp(argv[i], "--glsl") == 0) {
            cmd_args.use_glsl = true;
            continue;
        }
        if (strcmp(argv[i], "--spv") == 0) {
            cmd_args.use_glsl = false;
            continue;
        }
        //model_filename = argv[i];
    }

    /*if (!model_filename) {
        printf("Usage: ./ra <model>\n");
        exit(-1);
    }*/

    glfwInit();
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    glfwWindowHintString(GLFW_X11_CLASS_NAME, "vcc_demo");
    glfwWindowHintString(GLFW_X11_INSTANCE_NAME, "vcc_demo");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(2048, 2048, "Example", nullptr, nullptr);

    imr::Context context;
    imr::Device device(context);
    imr::Swapchain swapchain(device, window);
    imr::FpsCounter fps_counter;

    auto depth_format = findSupportedFormat(device,
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    imr::Image depth_image (device, VK_IMAGE_TYPE_2D, {width, height, 1}, depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

    //Model model(model_filename, device);

    camera = {{0, 0, 0}, {0, 0}, 60};
    //camera = model.loaded_camera;
    camera.position = cmd_args.camera_eye.value_or(camera.position);
    if (cmd_args.camera_rotation.has_value()) {
        camera.rotation.yaw = cmd_args.camera_rotation.value().x;
        camera.rotation.pitch = cmd_args.camera_rotation.value().y;
    }
    camera.fov = cmd_args.camera_fov.value_or(camera.fov);
    camera_state.fly_speed = cmd_args.camera_speed.value_or(camera_state.fly_speed);

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (action == GLFW_PRESS && key == GLFW_KEY_F4) {
            printf("--position %f %f %f --rotation %f %f --fov %f\n", (float) camera.position.x, (float) camera.position.y, (float) camera.position.z, (float) camera.rotation.yaw, (float) camera.rotation.pitch, (float) camera.fov);
        }
        if (action == GLFW_PRESS && key == GLFW_KEY_MINUS) {
            camera.fov -= 0.02f;
        }
        if (action == GLFW_PRESS && key == GLFW_KEY_EQUAL) {
            camera.fov += 0.02f;
        }
    });

    auto vkCmdBeginRenderingKHR = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(vkGetInstanceProcAddr(context.instance, "vkCmdBeginRenderingKHR"));
    auto vkCmdEndRenderingKHR   = reinterpret_cast<PFN_vkCmdEndRenderingKHR>(vkGetInstanceProcAddr(context.instance, "vkCmdEndRenderingKHR"));

    auto& vk = device.dispatch;

    std::vector<Vertex> vertex_data_cpu;
    create_flat_surface(vertex_data_cpu);
    //auto vertex_data_cpu = vertices;

    std::unique_ptr<imr::Buffer> vertex_data_buffer = std::make_unique<imr::Buffer>(device, sizeof(vertex_data_cpu[0]) * vertex_data_cpu.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    Vertex * vertex_data;
    CHECK_VK(vkMapMemory(device.device, vertex_data_buffer->memory, vertex_data_buffer->memory_offset, vertex_data_buffer->size, 0, (void**) &vertex_data), abort());
    memcpy(vertex_data, vertex_data_cpu.data(), sizeof(vertex_data_cpu[0]) * vertex_data_cpu.size());
    vkUnmapMemory(device.device, vertex_data_buffer->memory);

    VkPipelineLayout pipeline_layout = create_pipeline_layout(device);
    VkPipeline graphics_pipeline = create_pipeline(device, swapchain, pipeline_layout, cmd_args.use_glsl);

    auto epoch = time();
    auto prev_frame = epoch;
    float delta = 0;

    while (!glfwWindowShouldClose(window)) {
        swapchain.beginFrame([&](auto& frame) {
            camera_update(window, &camera_input);
            camera_move_freelook(&camera, &camera_input, &camera_state, delta);


            auto& image = frame.swapchain_image;

            VkCommandBuffer cmdbuf;
            vkAllocateCommandBuffers(device.device, tmp((VkCommandBufferAllocateInfo) {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = device.pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            }), &cmdbuf);

            vkBeginCommandBuffer(cmdbuf, tmp((VkCommandBufferBeginInfo) {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            }));

            VkSemaphore sem;
            CHECK_VK(vkCreateSemaphore(device.device, tmp((VkSemaphoreCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            }), nullptr, &sem), abort());

            vk.setDebugUtilsObjectNameEXT(tmp((VkDebugUtilsObjectNameInfoEXT) {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .objectType = VK_OBJECT_TYPE_SEMAPHORE,
                .objectHandle = reinterpret_cast<uint64_t>(sem),
                .pObjectName = (std::string("12_render_pipeline.sem") + std::to_string(frame.id)).c_str(),
            }));

            mat4 camera_matrix = camera_get_view_mat4(&camera, frame.width, frame.height);
            vkCmdPushConstants(cmdbuf, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 4 * 16, &camera_matrix);

            vk.cmdPipelineBarrier2KHR(cmdbuf, tmp((VkDependencyInfo) {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = 0,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = tmp((VkImageMemoryBarrier2) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                    .srcAccessMask = VK_ACCESS_2_NONE,
                    .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .image = image,
                    .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .levelCount = 1,
                        .layerCount = 1,
                    }
                }),
            }));
            vk.cmdPipelineBarrier2KHR(cmdbuf, tmp((VkDependencyInfo) {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = 0,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = tmp((VkImageMemoryBarrier2) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                    .srcAccessMask = VK_ACCESS_2_NONE,
                    .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    .image = depth_image.handle,
                    .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | (hasStencilComponent(depth_format) ? VK_IMAGE_ASPECT_STENCIL_BIT : (VkImageAspectFlags) 0),
                        .levelCount = 1,
                        .layerCount = 1,
                    }
                }),
            }));

            VkImageView imageView = createImageView(device, image, swapchain.format(), VK_IMAGE_ASPECT_COLOR_BIT);
            VkImageView depthView = createImageView(device, depth_image.handle, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);

            VkRenderingAttachmentInfoKHR color_attachment_info = initializers::rendering_attachment_info();
            color_attachment_info.imageView = imageView;
            color_attachment_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
            color_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            color_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            color_attachment_info.clearValue = {.color = { 0.8f, 0.9f, 1.0f, 1.0f}};
            //color_attachment_info.clearValue = {.color = { 0.7f, 0.7f, 0.7f, 1.0f}};

            VkRenderingInfoKHR render_info = initializers::rendering_info(
                initializers::rect2D(static_cast<int>(frame.width), static_cast<int>(frame.height), 0, 0),
                1,
                &color_attachment_info
            );

            VkRenderingAttachmentInfoKHR depth_attachment_info = initializers::rendering_attachment_info();
            depth_attachment_info.imageView = depthView;
            depth_attachment_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
            depth_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depth_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depth_attachment_info.clearValue = {.depthStencil = { 1.0f, 0 }};

            render_info.pDepthAttachment = &depth_attachment_info;

            render_info.layerCount = 1;
            if (hasStencilComponent(depth_format)) {
                VkRenderingAttachmentInfoKHR stencil_attachment_info = initializers::rendering_attachment_info();
                stencil_attachment_info.imageView = depthView;
                stencil_attachment_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
                stencil_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                stencil_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

                render_info.pStencilAttachment = &stencil_attachment_info;
            }

            vkCmdBeginRenderingKHR(cmdbuf, &render_info);

            VkViewport viewport = initializers::viewport(static_cast<float>(frame.width), static_cast<float>(frame.height), 0.0f, 1.0f);
            vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

            VkRect2D scissor = initializers::rect2D(static_cast<int>(frame.width), static_cast<int>(frame.height), 0, 0);
            vkCmdSetScissor(cmdbuf, 0, 1, &scissor);

            vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

            VkBuffer vertexBuffers[] = {vertex_data_buffer->handle};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmdbuf, 0, 1, vertexBuffers, offsets);

            vkCmdDraw(cmdbuf, static_cast<uint32_t>(vertex_data_cpu.size()), 1, 0, 0);

            vkCmdEndRenderingKHR(cmdbuf);

            vk.cmdPipelineBarrier2KHR(cmdbuf, tmp((VkDependencyInfo) {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = 0,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = tmp((VkImageMemoryBarrier2) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
                    .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    .image = image,
                    .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .levelCount = 1,
                        .layerCount = 1,
                    }
                }),
            }));

            VkFence fence;
            vkCreateFence(device.device, tmp((VkFenceCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = 0,
            }), nullptr, &fence);

            vkEndCommandBuffer(cmdbuf);
            vkQueueSubmit(device.main_queue, 1, tmp((VkSubmitInfo) {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &frame.swapchain_image_available,
                .pWaitDstStageMask = tmp((VkPipelineStageFlags) VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT),
                .commandBufferCount = 1,
                .pCommandBuffers = &cmdbuf,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &sem,
            }), fence);

            //printf("Frame submitted with fence = %llx\n", fence);

            frame.add_to_delete_queue(fence, [=, &device]() {
                //vkWaitForFences(context.device, 1, &fence, true, UINT64_MAX);
                vkDestroyFence(device.device, fence, nullptr);
                vkDestroySemaphore(device.device, sem, nullptr);
                vkDestroyImageView(device.device, imageView, nullptr);
                vkFreeCommandBuffers(device.device, device.pool, 1, &cmdbuf);
            });
            frame.present(sem);

            auto now = time();
            delta = (float) ((now - prev_frame) / 1000000) / 1000.0f;
            prev_frame = now;
        });

        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);
        glfwPollEvents();
    }

    vkDeviceWaitIdle(device.device);

    vkDestroyPipeline(device.device, graphics_pipeline, nullptr);

    return 0;
}
