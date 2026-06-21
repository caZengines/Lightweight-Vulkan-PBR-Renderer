#pragma once
#include <vector>
#include "render_context.hpp"

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>


class Pipeline {
public:
    Pipeline(RenderContext& rct, vk::raii::DescriptorSetLayout& dsl, vk::Format colorFormat);
    ~Pipeline() = default;

    const vk::raii::Pipeline& binding() const { return pipeline; }
    const vk::raii::PipelineLayout& getLayout() const { return pipelineLayout; }

private:
    RenderContext&                       rct_;
    vk::raii::PipelineLayout             pipelineLayout   = nullptr;
    vk::raii::Pipeline                   pipeline  = nullptr;

    void createGraphicsPipeline(vk::raii::DescriptorSetLayout& dsl, vk::Format colorFormat);
    static std::vector<char> readFile(const std::string& filename);
    vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const;
    vk::Format findDepthFormat() const;
    vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates,
                                   vk::ImageTiling tiling, vk::FormatFeatureFlags features) const;
};
