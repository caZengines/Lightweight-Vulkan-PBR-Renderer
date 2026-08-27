#pragma once
#include <vector>
#include "render_context.hpp"

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

// Pipeline provides a public interface for reading files and returning a vector of variable type: uint8_t
class Pipeline {
public:
    Pipeline(RenderContext& rct, const std::vector<vk::DescriptorSetLayout>& dsls, vk::Format colorFormat);
    ~Pipeline() = default;

    const vk::raii::Pipeline& binding() const { return pipeline; }
    const vk::raii::PipelineLayout& getLayout() const { return pipelineLayout; }

    static std::vector<uint8_t> readFile(const std::string& filename);

private:
    RenderContext&                       rct_;
    vk::raii::PipelineLayout             pipelineLayout   = nullptr;
    vk::raii::Pipeline                   pipeline  = nullptr;

    void createGraphicsPipeline(const std::vector<vk::DescriptorSetLayout>& dsls, vk::Format colorFormat);
    vk::raii::ShaderModule createShaderModule(const std::vector<uint8_t>& code) const;
    vk::Format findDepthFormat() const;
    vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates,
                                   vk::ImageTiling tiling, vk::FormatFeatureFlags features) const;
};
